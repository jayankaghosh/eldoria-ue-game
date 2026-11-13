#include "EldoriaGameMode.h"
#include "EldoriaCharacter.h"
#include "EldoriaHUD.h"
#include "EnemyCharacter.h"
#include "Chest.h"
#include "EldoriaSettings.h"
#include "Components/AudioComponent.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "TimerManager.h"
#include "EldoriaMenu.h"
#include "EldoriaIntro.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "EldoriaSaveGame.h"
#include "HealthComponent.h"
#include "Pickups.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Scalability.h"
#include "Containers/Ticker.h"
#include "HAL/IConsoleManager.h"

namespace
{
	const TCHAR* MapPath = TEXT("/Game/Maps/Eldoria");
	const TCHAR* SaveSlots[] = { TEXT("Autosave"), TEXT("Slot1"), TEXT("Slot2"), TEXT("Slot3") };
	const TCHAR* SaveLabels[] = { TEXT("Autosave"), TEXT("Save Slot 1"), TEXT("Save Slot 2"), TEXT("Save Slot 3") };
}

#define LOCTEXT_NAMESPACE "Eldoria"

AEldoriaGameMode::AEldoriaGameMode()
{
	DefaultPawnClass = AEldoriaCharacter::StaticClass();
	HUDClass = AEldoriaHUD::StaticClass();
	PrimaryActorTick.bCanEverTick = true;
	// the story runs while the world is otherwise idle
	PrimaryActorTick.bTickEvenWhenPaused = false;
}

void AEldoriaGameMode::BeginPlay()
{
	Super::BeginPlay();
	// development builds print engine status text (texture streaming, reflection captures); keep the screen clean
	if (GEngine) GEngine->Exec(GetWorld(), TEXT("DisableAllScreenMessages"));

	auto Add = [this](FText Title, FText Desc, EQuestGoal Goal, FName Group, FName Tag)
	{
		FQuestStage S;
		S.Title = Title;
		S.Description = Desc;
		S.Goal = Goal;
		S.Group = Group;
		S.ObjectiveTag = Tag;
		Stages.Add(S);
	};
	Add(LOCTEXT("Q1", "The Village Fields"), LOCTEXT("Q1d", "A warband of the Bone King has camped in the fields outside the village. Destroy it."), EQuestGoal::DefeatGroup, TEXT("WestCamp"), TEXT("Objective_WestCamp"));
	Add(LOCTEXT("Q2", "The Old Ruins"), LOCTEXT("Q2d", "His champions hold the ruins on the eastern hill. Cut them down."), EQuestGoal::DefeatGroup, TEXT("Ruins"), TEXT("Objective_Ruins"));
	Add(LOCTEXT("Q3", "The Sky Shrine"), LOCTEXT("Q3d", "Climb the broken stones above the ruins and claim the Royal Key."), EQuestGoal::FindRoyalKey, NAME_None, TEXT("Objective_Shrine"));
	Add(LOCTEXT("Q4", "The Bone King"), LOCTEXT("Q4d", "With the Royal Key his ward is broken. Mordrak holds the village square. End his reign."), EQuestGoal::DefeatBoss, TEXT("Castle"), TEXT("Objective_Boss"));

	for (TActorIterator<AEnemyCharacter> It(GetWorld()); It; ++It)
	{
		if (!It->Group.IsNone()) GroupTotals.FindOrAdd(It->Group)++;
	}
	if (AActor* Start = UGameplayStatics::GetActorOfClass(this, APlayerStart::StaticClass()))
	{
		Checkpoint = Start->GetActorTransform();
	}
	StartTime = GetWorld()->GetTimeSeconds();
	ApplyQuality(GetQuality(), false);
	// coins come only from defeated enemies; the trails laid along the roads are cleared away
	{
		TArray<AActor*> Placed;
		UGameplayStatics::GetAllActorsOfClass(this, ACoinPickup::StaticClass(), Placed);
		for (AActor* Coin : Placed) Coin->Destroy();
	}
	// the village ships its terrain-sculpting brushes (Landmass): an editor tool that keeps re-rendering
	// the landscape. The terrain is already baked, so in the game they only cost frame time.
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		const FString Class = It->GetClass()->GetName();
		if (Class.Contains(TEXT("Landmass")) || Class.Contains(TEXT("CustomBrush")))
		{
			It->SetActorTickEnabled(false);
			It->Destroy();
		}
	}
	// thin the undergrowth first, so felled trees get no trunk colliders
	ThinFoliage();
	AddTreeTrunkColliders();
	StartMusic();

	if (!PendingLoad.IsEmpty())
	{
		// arriving from "Continue" / "Load Game": rebuild the saved state, then play
		LoadedSave = Cast<UEldoriaSaveGame>(UGameplayStatics::LoadGameFromSlot(PendingLoad, 0));
		if (LoadedSave) ApplySave(LoadedSave);
		StartPlaying();
	}
	else if (bNewGameRequested)
	{
		// the hero pawn appears just after BeginPlay
		GetWorldTimerManager().SetTimerForNextTick(this, &AEldoriaGameMode::StartIntro);
	}
	else
	{
		// a fresh launch opens on the title screen, over the live village
		GetWorldTimerManager().SetTimerForNextTick([this]() { ShowMenu(EMenuPage::Main); });
		// development: -eldoriacontinue presses Continue on the title screen
		if (FParse::Param(FCommandLine::Get(), TEXT("eldoriacontinue")))
		{
			// the title menu pauses the world, so this waits on real time
			TWeakObjectPtr<AEldoriaGameMode> Self(this);
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self](float)
			{
				if (Self.IsValid())
				{
					UE_LOG(LogTemp, Display, TEXT("CONTINUETEST pressed"));
					Self->ContinueGame();
					UE_LOG(LogTemp, Display, TEXT("CONTINUETEST playing=%d"), Self->ProgressTimer.IsValid() ? 1 : 0);
				}
				return false;
			}), 3.f);
		}
	}
}

void AEldoriaGameMode::StartMusic()
{
	USoundBase* Track = UEldoriaSettings::Get()->Music.LoadSynchronous();
	if (!Track) return;
	MusicComponent = UGameplayStatics::CreateSound2D(this, Track, 1.f, 1.f, 0.f, nullptr, false, false);
	if (!MusicComponent) return;
	// a UI sound keeps playing while the game is paused behind a menu
	MusicComponent->bIsUISound = true;
	MusicComponent->OnAudioFinished.AddDynamic(this, &AEldoriaGameMode::OnMusicFinished);
	const bool bMenuFirst = PendingLoad.IsEmpty() && !bNewGameRequested;
	MusicComponent->FadeIn(2.f, bMenuFirst ? UEldoriaSettings::Get()->MusicMenuVolume : UEldoriaSettings::Get()->MusicGameVolume);
}

void AEldoriaGameMode::SetMusicLevel(bool bMenu)
{
	if (!MusicComponent) return;
	const UEldoriaSettings* S = UEldoriaSettings::Get();
	MusicComponent->AdjustVolume(bMenu ? 0.6f : 1.5f, bMenu ? S->MusicMenuVolume : S->MusicGameVolume);
}

void AEldoriaGameMode::OnMusicFinished()
{
	// keep the score going: restart when the track ends
	if (MusicComponent) MusicComponent->Play();
}

void AEldoriaGameMode::ThinFoliage()
{
	// Places where fights happen: every enemy, every quest objective, and the paths between objectives
	// in quest order, starting from where the hero spawns.
	struct FClearing { FVector2D At; float Under; float Trees; };
	TArray<FClearing> Clearings;
	for (TActorIterator<AEnemyCharacter> It(GetWorld()); It; ++It)
	{
		// every enemy stands in its own little meadow
		Clearings.Add({ FVector2D(It->GetActorLocation()), 1800.f, 1200.f });
	}
	TArray<FVector2D> Route;
	if (AActor* Start = UGameplayStatics::GetActorOfClass(this, APlayerStart::StaticClass())) Route.Add(FVector2D(Start->GetActorLocation()));
	for (const FQuestStage& S : Stages)
	{
		TArray<AActor*> Marks;
		UGameplayStatics::GetAllActorsWithTag(this, S.ObjectiveTag, Marks);
		for (AActor* M : Marks)
		{
			// camps and quest sites sit in a wide open field
			Clearings.Add({ FVector2D(M->GetActorLocation()), 4000.f, 3000.f });
			Route.Add(FVector2D(M->GetActorLocation()));
		}
	}
	auto DistToRoute = [&Route](const FVector2D& P)
	{
		float Best = TNumericLimits<float>::Max();
		for (int32 i = 1; i < Route.Num(); ++i)
		{
			const FVector2D A = Route[i - 1], B = Route[i];
			const FVector2D AB = B - A;
			const float T = FMath::Clamp(FVector2D::DotProduct(P - A, AB) / FMath::Max(1.f, AB.SizeSquared()), 0.f, 1.f);
			Best = FMath::Min(Best, FVector2D::Distance(P, A + AB * T));
		}
		return Best;
	};

	// Low ground cover (moss, grass tufts, stones) stays everywhere; it never hides anyone. Knee- to
	// head-high growth (bushes, ferns, tall grass, young spruce) is thinned out across the map and cleared
	// where fights happen; tall trees are felled around camps and posts (making fields) and along the paths.
	constexpr float GroundCover = 45.f;
	constexpr float TallTree = 500.f;
	constexpr float ThinShare = 0.65f;
	int32 Removed = 0, Kept = 0;
	for (TObjectIterator<UInstancedStaticMeshComponent> It; It; ++It)
	{
		UInstancedStaticMeshComponent* C = *It;
		if (C->GetWorld() != GetWorld() || !C->GetStaticMesh() || C->GetInstanceCount() == 0) continue;
		if (!C->GetOwner() || !C->GetOwner()->GetClass()->GetName().Contains(TEXT("InstancedFoliageActor"))) continue;
		// foliage has no collision, so it never shapes a path; keeping it out of the navigation system stops
		// every removal below from queueing a navigation rebuild
		C->SetCanEverAffectNavigation(false);
		const float MeshHeight = C->GetStaticMesh()->GetBounds().BoxExtent.Z * 2.f;
		// ivy grows up tree trunks: it goes wherever the trees go, or it would be left hanging in the air
		const FString MeshName = C->GetStaticMesh()->GetName();
		const bool bClimber = MeshName.Contains(TEXT("Ivy")) || MeshName.Contains(TEXT("Creeper"));
		if (!bClimber && MeshHeight * 2.f < GroundCover) continue;
		const uint32 Seed = GetTypeHash(C->GetStaticMesh()->GetFName());
		TArray<int32> Remove;
		for (int32 i = 0; i < C->GetInstanceCount(); ++i)
		{
			FTransform T;
			C->GetInstanceTransform(i, T, true);
			const float Height = MeshHeight * T.GetScale3D().Z;
			if (!bClimber && Height < GroundCover) continue;
			const FVector2D P(T.GetLocation());
			const bool bTree = bClimber || Height >= TallTree;
			bool bCleared = DistToRoute(P) < (bTree ? 300.f : 700.f);
			for (const FClearing& Cl : Clearings)
			{
				if (bCleared) break;
				bCleared = FVector2D::Distance(P, Cl.At) < (bTree ? Cl.Trees : Cl.Under);
			}
			// a stable pseudo-random pick, so the same plants go every time the game starts
			const float Roll = FMath::Frac(FMath::Sin(float(i) * 12.9898f + float(Seed % 1000)) * 43758.5453f);
			if (bCleared || (!bTree && Roll < ThinShare)) Remove.Add(i);
			else if (bClimber) continue;
			else ++Kept;
		}
		if (Remove.Num() > 0)
		{
			C->RemoveInstances(Remove);
			Removed += Remove.Num();
		}
	}
	// some trunks wear ivy placed as separate props; it goes with the trees it was wrapped around
	int32 IvyRemoved = 0;
	for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
	{
		const UStaticMesh* Mesh = It->GetStaticMeshComponent() ? It->GetStaticMeshComponent()->GetStaticMesh() : nullptr;
		if (!Mesh || !(Mesh->GetName().Contains(TEXT("Ivy")) || Mesh->GetName().Contains(TEXT("Creeper")))) continue;
		const FVector2D P(It->GetActorLocation());
		bool bCleared = DistToRoute(P) < 300.f;
		for (const FClearing& Cl : Clearings) bCleared |= FVector2D::Distance(P, Cl.At) < Cl.Trees;
		if (bCleared)
		{
			It->Destroy();
			++IvyRemoved;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("Eldoria: thinned foliage, removed %d plants and trees and %d ivy props, kept %d taller ones"), Removed, IvyRemoved, Kept);
}

void AEldoriaGameMode::AddTreeTrunkColliders()
{
	UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (!Cylinder) return;

	// gather tall tree foliage; low plants (grass, ferns, bushes, moss) stay walk-through
	static const TCHAR* Skip[] = { TEXT("grass"), TEXT("fern"), TEXT("bush"), TEXT("plant"), TEXT("moss"), TEXT("ivy"), TEXT("branch"), TEXT("stone"), TEXT("rock"), TEXT("cover"), TEXT("flower"), TEXT("weed") };
	TArray<UInstancedStaticMeshComponent*> Trees;
	for (TObjectIterator<UInstancedStaticMeshComponent> It; It; ++It)
	{
		UInstancedStaticMeshComponent* C = *It;
		if (C->GetWorld() != GetWorld() || !C->GetStaticMesh() || C->GetInstanceCount() == 0) continue;
		const FString Name = C->GetStaticMesh()->GetName().ToLower();
		bool bSkip = false;
		for (const TCHAR* S : Skip) bSkip |= Name.Contains(S);
		const FVector Size = C->GetStaticMesh()->GetBounds().BoxExtent * 2.f;
		if (!bSkip && Size.Z >= 420.f && FMath::Max(Size.X, Size.Y) >= 250.f) Trees.Add(C);
	}

	AActor* Holder = GetWorld()->SpawnActor<AActor>();
	USceneComponent* Root = NewObject<USceneComponent>(Holder, TEXT("Root"));
	Holder->SetRootComponent(Root);
	Root->RegisterComponent();
	int32 Total = 0;
	for (UInstancedStaticMeshComponent* Tree : Trees)
	{
		UHierarchicalInstancedStaticMeshComponent* Trunks = NewObject<UHierarchicalInstancedStaticMeshComponent>(Holder);
		Trunks->SetStaticMesh(Cylinder);
		Trunks->SetHiddenInGame(true);
		Trunks->SetCastShadow(false);
		Trunks->SetCollisionProfileName(TEXT("BlockAll"));
		Trunks->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		// enemies only path around their camps; 17,000 trunks in the navigation system cost every frame
		Trunks->SetCanEverAffectNavigation(false);
		Trunks->SetupAttachment(Root);
		Trunks->RegisterComponent();

		const float TreeHeight = Tree->GetStaticMesh()->GetBounds().BoxExtent.Z * 2.f;
		TArray<FTransform> Transforms;
		Transforms.Reserve(Tree->GetInstanceCount());
		for (int32 i = 0; i < Tree->GetInstanceCount(); ++i)
		{
			FTransform T;
			Tree->GetInstanceTransform(i, T, true);
			const float Scale = T.GetScale3D().Z;
			// trunk thickness grows with the tree; the cylinder is 100 cm wide and tall, centred on its origin
			const float Radius = FMath::Clamp(TreeHeight * Scale * 0.018f, 18.f, 55.f);
			const float Height = 420.f;
			FTransform Trunk(FRotator(0.f, T.Rotator().Yaw, 0.f), T.GetLocation() + FVector(0.f, 0.f, Height * 0.5f - 40.f), FVector(Radius / 50.f, Radius / 50.f, Height / 100.f));
			Transforms.Add(Trunk);
		}
		Trunks->AddInstances(Transforms, false, true);
		Total += Transforms.Num();
	}
	UE_LOG(LogTemp, Log, TEXT("Eldoria: added %d tree trunk colliders across %d tree types"), Total, Trees.Num());
}

void AEldoriaGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	PendingLoad = UGameplayStatics::ParseOption(Options, TEXT("load"));
	bNewGameRequested = UGameplayStatics::HasOption(Options, TEXT("newgame"));
	if (UGameplayStatics::HasOption(Options, TEXT("difficulty")))
	{
		Difficulty = static_cast<EDifficulty>(FMath::Clamp(UGameplayStatics::GetIntOption(Options, TEXT("difficulty"), 1), 0, 2));
	}
	MaxAttackers = Tuning(this).MaxAttackers;
}

void AEldoriaGameMode::FinishRestartPlayer(AController* NewPlayer, const FRotator& StartRotation)
{
	Super::FinishRestartPlayer(NewPlayer, StartRotation);
	// the camera starts level behind the hero with his whole body in view, ready to move, rather than
	// copying the spawn point's downward tilt
	if (AEldoriaCharacter* Hero = NewPlayer ? Cast<AEldoriaCharacter>(NewPlayer->GetPawn()) : nullptr) Hero->ResetCamera();
}

const FDifficultyTuning& AEldoriaGameMode::Tuning(const UObject* Context)
{
	// Easy: a sturdier hero, slower enemies who take turns. Hard: a fragile hero against fast,
	// heavy-hitting enemies who swing sooner and gang up.
	static const FDifficultyTuning Table[] =
	{
		{ 230.f, 0.55f, 0.85f, 1.5f, 0.85f, 1 },
		{ 150.f, 1.f,   1.f,   1.f,  1.f,   2 },
		{ 100.f, 1.6f,  1.15f, 0.65f, 1.15f, 3 },
	};
	const UWorld* World = Context ? Context->GetWorld() : nullptr;
	const AEldoriaGameMode* GM = World ? World->GetAuthGameMode<AEldoriaGameMode>() : nullptr;
	return Table[GM ? static_cast<int32>(GM->Difficulty) : 1];
}

FText AEldoriaGameMode::DifficultyName(EDifficulty D)
{
	switch (D)
	{
	case EDifficulty::Easy: return LOCTEXT("Easy", "Easy");
	case EDifficulty::Hard: return LOCTEXT("Hard", "Hard");
	default: return LOCTEXT("Medium", "Medium");
	}
}

void AEldoriaGameMode::SetDifficulty(EDifficulty InDifficulty)
{
	Difficulty = InDifficulty;
	const FDifficultyTuning& T = Tuning(this);
	MaxAttackers = T.MaxAttackers;
	for (TActorIterator<AEnemyCharacter> It(GetWorld()); It; ++It)
	{
		It->ApplyDifficulty();
	}
	if (AEldoriaCharacter* Hero = Cast<AEldoriaCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		const float Pct = Hero->Health->GetPercent();
		Hero->Health->MaxHealth = T.HeroHealth;
		Hero->Health->Health = FMath::Max(1.f, Pct * T.HeroHealth);
	}
}

void AEldoriaGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Menu.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(Menu.ToSharedRef());
	}
	Menu.Reset();
	Super::EndPlay(Reason);
}

void AEldoriaGameMode::StartPlaying()
{
	StartTime = GetWorld()->GetTimeSeconds() - (LoadedSave ? LoadedSave->PlayTime : 0.f);
	GetWorldTimerManager().SetTimer(ProgressTimer, this, &AEldoriaGameMode::CheckProgress, 0.25f, true);
	ShowMessage(LoadedSave ? LOCTEXT("Resumed", "Welcome back") : LOCTEXT("Start", "Save the Kingdom"), 4.f);
	if (USoundBase* Ambient = UEldoriaSettings::Get()->AmbientLoop.LoadSynchronous())
	{
		UGameplayStatics::SpawnSound2D(this, Ambient, 0.6f);
	}
	if (LoadedSave) ApplyHeroSave();
	else if (AEldoriaCharacter* Hero = Cast<AEldoriaCharacter>(UGameplayStatics::GetPlayerPawn(this, 0))) Hero->ResetCamera();
}

// ---------- menus ----------

void AEldoriaGameMode::ShowMenu(EMenuPage Page)
{
	if (!GEngine || !GEngine->GameViewport) return;
	if (!Menu.IsValid())
	{
		SAssignNew(Menu, SEldoriaMenu).Game(this).Page(Page);
		GEngine->GameViewport->AddViewportWidgetContent(Menu.ToSharedRef(), 100);
	}
	else
	{
		Menu->ShowPage(Page);
	}
	UGameplayStatics::SetGamePaused(this, true);
	SetMusicLevel(true);
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		PC->bShowMouseCursor = true;
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(Menu);
		PC->SetInputMode(Mode);
	}
	Menu->FocusFirst();
}

void AEldoriaGameMode::CloseMenu()
{
	if (Menu.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(Menu.ToSharedRef());
	}
	Menu.Reset();
	UGameplayStatics::SetGamePaused(this, false);
	SetMusicLevel(false);
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		PC->bShowMouseCursor = false;
		PC->SetInputMode(FInputModeGameOnly());
		// the mouse is captured again: don't let that first jump spin the camera
		if (AEldoriaCharacter* Hero = Cast<AEldoriaCharacter>(PC->GetPawn())) Hero->ResetCameraLookOnly();
	}
	if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetAllUserFocusToGameViewport();
}

void AEldoriaGameMode::TogglePauseMenu()
{
	if (IsMenuOpen()) CloseMenu();
	else ShowMenu(EMenuPage::Pause);
}

// ---------- game flow ----------

void AEldoriaGameMode::NewGame(EDifficulty InDifficulty)
{
	// on the title screen the world is already fresh; from anywhere else, reload it
	if (!ProgressTimer.IsValid() && !LoadedSave)
	{
		SetDifficulty(InDifficulty);
		CloseMenu();
		StartIntro();
	}
	else
	{
		UGameplayStatics::OpenLevel(this, FName(MapPath), true, FString::Printf(TEXT("newgame?difficulty=%d"), static_cast<int32>(InDifficulty)));
	}
}

void AEldoriaGameMode::ContinueGame()
{
	const FString Slot = LatestSlot();
	if (!Slot.IsEmpty()) LoadFromSlot(Slot);
}

void AEldoriaGameMode::LoadFromSlot(const FString& Slot)
{
	if (!UGameplayStatics::DoesSaveGameExist(Slot, 0)) return;
	// On the title screen the world behind the menu is still untouched, so the save can be laid onto it
	// right here: no reload, play resumes at once. Mid-game the world has changed (enemies killed, chests
	// opened), so it is rebuilt from scratch.
	if (!ProgressTimer.IsValid() && !LoadedSave && !bIntroPlaying)
	{
		LoadedSave = Cast<UEldoriaSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
		if (LoadedSave)
		{
			ApplySave(LoadedSave);
			CloseMenu();
			StartPlaying();
			return;
		}
	}
	{
		UGameplayStatics::OpenLevel(this, FName(MapPath), true, FString::Printf(TEXT("load=%s"), *Slot));
	}
}

bool AEldoriaGameMode::SaveToSlot(const FString& Slot)
{
	AEldoriaCharacter* Hero = Cast<AEldoriaCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Hero || Hero->IsDead()) return false;
	UEldoriaSaveGame* Save = Cast<UEldoriaSaveGame>(UGameplayStatics::CreateSaveGameObject(UEldoriaSaveGame::StaticClass()));
	Save->SavedAt = FDateTime::Now();
	Save->QuestTitle = CurrentStage() ? CurrentStage()->Title.ToString() : TEXT("Eldoria Is Free");
	Save->PlayTime = GetWorld()->GetTimeSeconds() - StartTime;
	Save->HeroLocation = Hero->GetActorLocation();
	Save->HeroRotation = Hero->GetActorRotation();
	Save->Health = Hero->Health->Health;
	Save->Coins = Hero->GetCoins();
	Save->Stage = Stage;
	Save->bHasRoyalKey = bHasRoyalKey;
	Save->Kills = Kills;
	Save->Deaths = Deaths;
	Save->Difficulty = static_cast<uint8>(Difficulty);
	Save->KilledEnemies = KilledEnemies;
	Save->OpenedChests = OpenedChests;
	Save->CollectedPickups = CollectedPickups;
	const bool bOk = UGameplayStatics::SaveGameToSlot(Save, Slot, 0);
	if (bOk && Slot != AutosaveSlot()) ShowMessage(LOCTEXT("Saved", "Game saved"), 2.f);
	return bOk;
}

void AEldoriaGameMode::ReturnToMainMenu()
{
	UGameplayStatics::OpenLevel(this, FName(MapPath), true);
}

void AEldoriaGameMode::QuitGame()
{
	UKismetSystemLibrary::QuitGame(this, GetWorld()->GetFirstPlayerController(), EQuitPreference::Quit, false);
}

TArray<FSaveSlotInfo> AEldoriaGameMode::ListSlots()
{
	TArray<FSaveSlotInfo> Out;
	for (int32 i = 0; i < UE_ARRAY_COUNT(SaveSlots); ++i)
	{
		FSaveSlotInfo Info;
		Info.Slot = SaveSlots[i];
		Info.Label = SaveLabels[i];
		if (UGameplayStatics::DoesSaveGameExist(Info.Slot, 0))
		{
			if (const UEldoriaSaveGame* S = Cast<UEldoriaSaveGame>(UGameplayStatics::LoadGameFromSlot(Info.Slot, 0)))
			{
				Info.bExists = true;
				Info.SavedAt = S->SavedAt;
				Info.QuestTitle = S->QuestTitle;
				Info.PlayTime = S->PlayTime;
			}
		}
		Out.Add(Info);
	}
	return Out;
}

FString AEldoriaGameMode::LatestSlot()
{
	FString Best;
	FDateTime BestTime;
	for (const FSaveSlotInfo& Info : ListSlots())
	{
		if (Info.bExists && (Best.IsEmpty() || Info.SavedAt > BestTime))
		{
			Best = Info.Slot;
			BestTime = Info.SavedAt;
		}
	}
	return Best;
}

void AEldoriaGameMode::ApplySave(UEldoriaSaveGame* Save)
{
	Stage = Save->Stage;
	bHasRoyalKey = Save->bHasRoyalKey;
	Kills = Save->Kills;
	Deaths = Save->Deaths;
	SetDifficulty(static_cast<EDifficulty>(FMath::Min<uint8>(Save->Difficulty, 2)));
	KilledEnemies = Save->KilledEnemies;
	OpenedChests = Save->OpenedChests;
	CollectedPickups = Save->CollectedPickups;
	Checkpoint = FTransform(Save->HeroRotation, Save->HeroLocation);

	TArray<AActor*> Gone;
	for (TActorIterator<AEnemyCharacter> It(GetWorld()); It; ++It)
	{
		if (KilledEnemies.Contains(It->GetName())) Gone.Add(*It);
	}
	for (TActorIterator<APickup> It(GetWorld()); It; ++It)
	{
		if (CollectedPickups.Contains(It->GetName())) Gone.Add(*It);
	}
	for (AActor* A : Gone) A->Destroy();
	for (TActorIterator<AChest> It(GetWorld()); It; ++It)
	{
		if (OpenedChests.Contains(It->GetName())) It->RestoreOpened();
	}
	if (IsQuestDone()) bVictory = true;
}

void AEldoriaGameMode::ApplyHeroSave()
{
	// the hero pawn is spawned after BeginPlay; keep trying until it exists
	AEldoriaCharacter* Hero = Cast<AEldoriaCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Hero || !LoadedSave)
	{
		if (LoadedSave) GetWorldTimerManager().SetTimerForNextTick(this, &AEldoriaGameMode::ApplyHeroSave);
		return;
	}
	Hero->SetActorLocationAndRotation(LoadedSave->HeroLocation + FVector(0.f, 0.f, 20.f), LoadedSave->HeroRotation, false, nullptr, ETeleportType::TeleportPhysics);
	Hero->Health->Health = FMath::Clamp(LoadedSave->Health, 1.f, Hero->Health->MaxHealth);
	Hero->SetCoins(LoadedSave->Coins);
	if (AController* C = Hero->GetController())
	{
		C->SetControlRotation(FRotator(-10.f, LoadedSave->HeroRotation.Yaw, 0.f));
	}
}

int32 AEldoriaGameMode::AliveInGroup(FName Group) const
{
	int32 Alive = 0;
	for (TActorIterator<AEnemyCharacter> It(GetWorld()); It; ++It)
	{
		if (It->Group == Group && !It->IsDead()) ++Alive;
	}
	return Alive;
}

AEnemyCharacter* AEldoriaGameMode::ActiveBoss() const
{
	for (TActorIterator<AEnemyCharacter> It(GetWorld()); It; ++It)
	{
		if (It->bBoss && !It->IsDead() && It->IsEngaged()) return *It;
	}
	return nullptr;
}

FText AEldoriaGameMode::ProgressText() const
{
	const FQuestStage* S = CurrentStage();
	if (!S) return LOCTEXT("Free", "Eldoria is free. Explore and find every coin.");
	switch (S->Goal)
	{
	case EQuestGoal::DefeatGroup:
	{
		const int32 Total = GroupTotals.FindRef(S->Group);
		return FText::Format(LOCTEXT("Kills", "Enemies defeated: {0} / {1}"), Total - FMath::Min(Total, AliveInGroup(S->Group)), Total);
	}
	case EQuestGoal::FindRoyalKey: return LOCTEXT("Key", "Double jump to reach the higher stones");
	case EQuestGoal::OpenGate: return LOCTEXT("Gate", "Approach the gate");
	case EQuestGoal::DefeatBoss: return LOCTEXT("Boss", "Defeat Mordrak");
	}
	return FText::GetEmpty();
}

bool AEldoriaGameMode::ObjectiveLocation(FVector& Out) const
{
	const FQuestStage* S = CurrentStage();
	if (!S) return false;
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsWithTag(this, S->ObjectiveTag, Found);
	if (Found.Num() == 0) return false;
	Out = Found[0]->GetActorLocation();
	return true;
}

void AEldoriaGameMode::CheckProgress()
{
	const FQuestStage* S = CurrentStage();
	if (!S || bHeroDown) return;
	bool bDone = false;
	switch (S->Goal)
	{
	case EQuestGoal::DefeatGroup: bDone = GroupTotals.FindRef(S->Group) > 0 && AliveInGroup(S->Group) == 0; break;
	case EQuestGoal::FindRoyalKey: bDone = bHasRoyalKey; break;
	case EQuestGoal::OpenGate:
		for (TActorIterator<ACastleGate> It(GetWorld()); It; ++It) bDone |= It->IsOpen();
		break;
	case EQuestGoal::DefeatBoss:
	{
		bool bBossAlive = false;
		for (TActorIterator<AEnemyCharacter> It(GetWorld()); It; ++It) bBossAlive |= It->bBoss && !It->IsDead();
		bDone = !bBossAlive;
		break;
	}
	}
	if (bDone) CompleteStage();
}

void AEldoriaGameMode::CompleteStage()
{
	if (APawn* Hero = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		Checkpoint = Hero->GetActorTransform();
	}
	const FText Done = Stages[Stage].Title;
	UEldoriaSettings::PlaySound2D(this, UEldoriaSettings::Get()->QuestSound, 0.9f);
	++Stage;
	SaveToSlot(AutosaveSlot());
	if (IsQuestDone())
	{
		bVictory = true;
		FinishTime = GetWorld()->GetTimeSeconds();
		ShowMessage(LOCTEXT("Victory", "The Kingdom Is Saved"), 8.f);
	}
	else
	{
		ShowMessage(FText::Format(LOCTEXT("Complete", "Quest complete: {0}"), Done), 4.f);
	}
}

void AEldoriaGameMode::OnEnemyKilled(AEnemyCharacter* Enemy)
{
	++Kills;
	if (Enemy) KilledEnemies.AddUnique(Enemy->GetName());
	if (Enemy && Enemy->bBoss) ShowMessage(LOCTEXT("BossDown", "Mordrak has fallen!"), 4.f);
}

void AEldoriaGameMode::OnRoyalKeyFound()
{
	bHasRoyalKey = true;
	ShowMessage(LOCTEXT("KeyFound", "You found the Royal Key!"), 4.f);
}

void AEldoriaGameMode::OnHeroDied()
{
	bHeroDown = true;
	++Deaths;
	GetWorldTimerManager().SetTimer(RespawnTimer, this, &AEldoriaGameMode::RespawnHero, 4.f, false);
}

void AEldoriaGameMode::RespawnHero()
{
	bHeroDown = false;
	if (AEldoriaCharacter* Hero = Cast<AEldoriaCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		FTransform At = Checkpoint;
		At.AddToTranslation(FVector(0.f, 0.f, 50.f));
		Hero->Respawn(At);
	}
	ShowMessage(LOCTEXT("Rise", "Rise again, knight"), 2.f);
}

bool AEldoriaGameMode::RequestAttackToken(AEnemyCharacter* Enemy)
{
	AttackTokens.RemoveAll([](const TWeakObjectPtr<AEnemyCharacter>& E) { return !E.IsValid() || E->IsDead(); });
	if (HasAttackToken(Enemy)) return true;
	// the boss never waits in line
	if (Enemy->bBoss || AttackTokens.Num() < MaxAttackers)
	{
		AttackTokens.Add(Enemy);
		return true;
	}
	return false;
}

void AEldoriaGameMode::ReleaseAttackToken(AEnemyCharacter* Enemy)
{
	AttackTokens.RemoveAll([Enemy](const TWeakObjectPtr<AEnemyCharacter>& E) { return !E.IsValid() || E.Get() == Enemy; });
}

bool AEldoriaGameMode::HasAttackToken(const AEnemyCharacter* Enemy) const
{
	return AttackTokens.ContainsByPredicate([Enemy](const TWeakObjectPtr<AEnemyCharacter>& E) { return E.Get() == Enemy; });
}

void AEldoriaGameMode::ShowMessage(const FText& Text, float Seconds)
{
	Message = Text;
	MessageUntil = GetWorld()->GetTimeSeconds() + Seconds;
}


// ---------- the new-game story ----------

void AEldoriaGameMode::StartIntro()
{
	const TArray<FIntroShot>& Shots = UEldoriaSettings::Get()->IntroShots;
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	// development runs (automated tests) and a missing story go straight into play
	const bool bTesting = FParse::Param(FCommandLine::Get(), TEXT("eldoriacombattest")) || FParse::Param(FCommandLine::Get(), TEXT("eldoriawalktest")) || FParse::Param(FCommandLine::Get(), TEXT("eldorianointro"));
	if (Shots.Num() == 0 || !PC || bTesting)
	{
		StartPlaying();
		return;
	}
	bIntroPlaying = true;
	IntroCamera = GetWorld()->SpawnActor<ACameraActor>();
	IntroCamera->GetCameraComponent()->bConstrainAspectRatio = false;
	PC->SetViewTarget(IntroCamera);

	// development: -eldoriascenery films the story's shots bare (no bars, words or voice) for loading-screen art
	bSceneryOnly = FParse::Param(FCommandLine::Get(), TEXT("eldoriascenery"));
	IntroOverlay = SNew(SEldoriaIntro).Game(this);
	if (GEngine && GEngine->GameViewport && !bSceneryOnly)
	{
		GEngine->GameViewport->AddViewportWidgetContent(IntroOverlay.ToSharedRef(), 20);
	}
	PC->bShowMouseCursor = false;
	PC->SetInputMode(FInputModeUIOnly().SetWidgetToFocus(IntroOverlay));
	if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetAllUserFocus(IntroOverlay, EFocusCause::SetDirectly);

	// the field music steps aside for the story's own score, which rises to its peak on the last line
	const UEldoriaSettings* S = UEldoriaSettings::Get();
	if (USoundBase* Score = S->IntroMusic.LoadSynchronous())
	{
		if (MusicComponent) MusicComponent->AdjustVolume(1.5f, 0.f);
		IntroScore = UGameplayStatics::CreateSound2D(this, Score, 1.f, 1.f, 0.f, nullptr, false, false);
		if (IntroScore)
		{
			IntroScore->bIsUISound = true;
			IntroScore->FadeIn(2.f, S->IntroMusicVolume);
			bScoreDucked = false;
		}
	}
	else if (MusicComponent)
	{
		// no score: keep the field music low under the voice
		MusicComponent->AdjustVolume(1.f, S->MusicMenuVolume * 0.45f);
	}
	BeginIntroShot(0);
}

void AEldoriaGameMode::BeginIntroShot(int32 Index)
{
	const TArray<FIntroShot>& Shots = UEldoriaSettings::Get()->IntroShots;
	if (!Shots.IsValidIndex(Index))
	{
		FinishIntro();
		return;
	}
	IntroShot = Index;
	IntroShotTime = 0.f;
	const FIntroShot& Shot = Shots[Index];

	// where the shot happens
	IntroAnchor = FVector::ZeroVector;
	IntroAnchorFrame = FRotator::ZeroRotator;
	if (Shot.Anchor == TEXT("Hero"))
	{
		if (APawn* Hero = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			IntroAnchor = Hero->GetActorLocation();
			IntroAnchorFrame = FRotator(0.f, Hero->GetActorRotation().Yaw, 0.f);
		}
	}
	else if (!Shot.Anchor.IsNone())
	{
		TArray<AActor*> Found;
		UGameplayStatics::GetAllActorsWithTag(this, Shot.Anchor, Found);
		if (Found.Num() > 0) IntroAnchor = Found[0]->GetActorLocation();
	}

	// the voice sets the pace: a breath before and after each line
	float Spoken = 3.f;
	if (IntroVoice) IntroVoice->Stop();
	IntroVoice = nullptr;
	if (USoundBase* Voice = bSceneryOnly ? nullptr : Shot.Narration.LoadSynchronous())
	{
		Spoken = Voice->GetDuration();
		IntroVoice = UGameplayStatics::CreateSound2D(this, Voice, 1.f, 1.f, 0.f, nullptr, false, false);
		if (IntroVoice)
		{
			IntroVoice->bIsUISound = true;
			IntroVoice->Play();
		}
	}
	IntroShotLength = bSceneryOnly ? 6.f : FMath::Max(4.f, Spoken + 1.4f);
	UE_LOG(LogTemp, Display, TEXT("INTROSHOT %d"), Index);
	IntroLineText = Shot.Line;
	if (IntroCamera) IntroCamera->GetCameraComponent()->SetFieldOfView(Shot.Fov);
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		// every cut comes up out of black
		PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, Index == 0 ? 1.6f : 0.7f, FLinearColor::Black, false, true);
	}
}

void AEldoriaGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bIntroPlaying || !IntroCamera) return;
	const TArray<FIntroShot>& Shots = UEldoriaSettings::Get()->IntroShots;
	if (!Shots.IsValidIndex(IntroShot)) return;
	const FIntroShot& Shot = Shots[IntroShot];

	IntroShotTime += DeltaSeconds;
	const float T = FMath::Clamp(IntroShotTime / IntroShotLength, 0.f, 1.f);
	// a slow, steady drift that eases in and out like a crane move
	const float Ease = 0.5f - 0.5f * FMath::Cos(T * PI);
	const FVector From = IntroAnchor + IntroAnchorFrame.RotateVector(Shot.From);
	const FVector To = IntroAnchor + IntroAnchorFrame.RotateVector(Shot.To);
	const FVector Look = IntroAnchor + IntroAnchorFrame.RotateVector(Shot.LookAt);
	const FVector Eye = FMath::Lerp(From, To, Ease);
	IntroCamera->SetActorLocationAndRotation(Eye, (Look - Eye).Rotation());

	// the score dips under the narrator so every word is clear, and rises again in the gaps
	const bool bSpeaking = IntroVoice && IntroVoice->IsPlaying();
	if (IntroScore && bSpeaking != bScoreDucked)
	{
		bScoreDucked = bSpeaking;
		const UEldoriaSettings* S = UEldoriaSettings::Get();
		IntroScore->AdjustVolume(bSpeaking ? 0.4f : 1.2f, bSpeaking ? S->IntroMusicUnderVoice : S->IntroMusicVolume);
	}

	// the subtitle fades in with the voice and out before the cut
	IntroLineOpacity = FMath::Clamp(IntroShotTime / 0.6f, 0.f, 1.f) * FMath::Clamp((IntroShotLength - IntroShotTime) / 0.6f, 0.f, 1.f);

	const bool bLast = IntroShot == Shots.Num() - 1;
	if (!bLast && IntroShotTime > IntroShotLength - 0.7f && IntroShotTime - DeltaSeconds <= IntroShotLength - 0.7f)
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, 0.7f, FLinearColor::Black, false, true);
		}
	}
	if (IntroShotTime >= IntroShotLength)
	{
		if (bLast) FinishIntro();
		else BeginIntroShot(IntroShot + 1);
	}
}

void AEldoriaGameMode::FinishIntro()
{
	if (!bIntroPlaying) return;
	bIntroPlaying = false;
	IntroLineOpacity = 0.f;
	if (IntroVoice) IntroVoice->Stop();
	IntroVoice = nullptr;
	if (IntroOverlay.IsValid() && GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(IntroOverlay.ToSharedRef());
	}
	IntroOverlay.Reset();
	// the score fades away as play begins and the field music returns
	if (IntroScore) IntroScore->FadeOut(3.f, 0.f);
	IntroScore = nullptr;
	SetMusicLevel(false);

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	AEldoriaCharacter* Hero = Cast<AEldoriaCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (PC)
	{
		PC->bShowMouseCursor = false;
		PC->SetInputMode(FInputModeGameOnly());
		if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetAllUserFocusToGameViewport();
		PC->PlayerCameraManager->StopCameraFade();
		PC->PlayerCameraManager->SetManualCameraFade(0.f, FLinearColor::Black, false);
		if (Hero)
		{
			// the story's last shot settles behind him, and the game camera takes over from there
			Hero->ResetCamera();
			PC->SetViewTargetWithBlend(Hero, 1.2f, VTBlend_Cubic);
		}
	}
	if (IntroCamera)
	{
		ACameraActor* Old = IntroCamera;
		FTimerHandle Cleanup;
		GetWorldTimerManager().SetTimer(Cleanup, [Old]() { if (IsValid(Old)) Old->Destroy(); }, 1.5f, false);
	}
	IntroCamera = nullptr;
	StartPlaying();
}

// ---------- graphics quality ----------

int32 AEldoriaGameMode::GetQuality()
{
	// Medium unless the player chose otherwise: smooth on a laptop-class GPU
	int32 Level = 1;
	if (GConfig) GConfig->GetInt(TEXT("Eldoria"), TEXT("GraphicsQuality"), Level, GGameUserSettingsIni);
	// development: -eldoriaquality=N measures a level without changing the player's choice
	FParse::Value(FCommandLine::Get(), TEXT("eldoriaquality="), Level);
	return FMath::Clamp(Level, 0, 2);
}

FText AEldoriaGameMode::QualityName(int32 Level)
{
	switch (Level)
	{
	case 0: return LOCTEXT("QLow", "Low");
	case 2: return LOCTEXT("QHigh", "High");
	default: return LOCTEXT("QMedium", "Medium");
	}
}

void AEldoriaGameMode::ApplyQuality(int32 Level, bool bSave)
{
	Level = FMath::Clamp(Level, 0, 2);
	// Low: no Lumen, simpler shadows and foliage, rendered at 65% and upscaled.
	// Medium: Lumen at high (not epic) quality, rendered at 75% and upscaled by TSR, which looks nearly native.
	// High: everything at epic, full resolution.
	Scalability::FQualityLevels Q = Scalability::GetQualityLevels();
	Q.SetFromSingleQualityLevel(Level == 2 ? 3 : (Level == 1 ? 2 : 1));
	Q.TextureQuality = 3;
	Scalability::SetQualityLevels(Q);
	static const float Percent[] = { 65.f, 75.f, 100.f };
	if (IConsoleVariable* Screen = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage")))
	{
		Screen->Set(Percent[Level], ECVF_SetByGameSetting);
	}
	if (bSave && GConfig)
	{
		GConfig->SetInt(TEXT("Eldoria"), TEXT("GraphicsQuality"), Level, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
	UE_LOG(LogTemp, Log, TEXT("Eldoria: graphics quality %s"), *QualityName(Level).ToString());
}

#undef LOCTEXT_NAMESPACE
