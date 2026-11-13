#include "EldoriaHUD.h"
#include "EldoriaCharacter.h"
#include "EldoriaGameMode.h"
#include "EnemyCharacter.h"
#include "HealthComponent.h"
#include "Chest.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "CanvasItem.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "RenderUtils.h"
#include "Engine/Font.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "EldoriaSettings.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	const FLinearColor Gold(0.98f, 0.78f, 0.36f);
	const FLinearColor Ink(0.97f, 0.94f, 0.88f);
	const FLinearColor Muted(0.78f, 0.74f, 0.66f);
	const FLinearColor HealthRed(0.82f, 0.13f, 0.12f);
	const FLinearColor Good(0.45f, 0.9f, 0.45f);

	// Roboto as a runtime-cached font asset renders crisply on the game canvas at any size
	FSlateFontInfo Font(float Size, bool bBold)
	{
		static UFont* Roboto = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
		return FSlateFontInfo(Roboto, FMath::Max(8, FMath::RoundToInt(Size)), bBold ? FName("Bold") : FName("Regular"));
	}
}

// ---------- drawing helpers ----------

FVector2D AEldoriaHUD::Measure(const FString& Str, float Size, bool bBold)
{
	if (!FSlateApplication::IsInitialized()) return FVector2D(Str.Len() * Size * Ui * 0.55f, Size * Ui);
	return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Str, Font(Size * Ui, bBold));
}

FVector2D AEldoriaHUD::Text(const FString& Str, float X, float Y, float Size, const FLinearColor& Color, bool bCenter, bool bBold)
{
	FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(Str), Font(Size * Ui, bBold), Color);
	Item.bCentreX = bCenter;
	Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.85f * Color.A), FVector2D(2.f, 2.f));
	Canvas->DrawItem(Item);
	return Measure(Str, Size, bBold);
}

void AEldoriaHUD::Panel(float X, float Y, float W, float H, float Alpha)
{
	DrawRect(FLinearColor(0.04f, 0.03f, 0.02f, Alpha), X, Y, W, H);
	// thin gold frame
	const float T = FMath::Max(1.f, 2.f * Ui);
	const FLinearColor Edge(Gold.R, Gold.G, Gold.B, 0.55f * Alpha / 0.72f);
	DrawRect(Edge, X, Y, W, T);
	DrawRect(Edge, X, Y + H - T, W, T);
	DrawRect(Edge, X, Y, T, H);
	DrawRect(Edge, X + W - T, Y, T, H);
}

void AEldoriaHUD::Bar(float X, float Y, float W, float H, float Pct, const FLinearColor& Fill, float LagPct)
{
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.85f), X - 3 * Ui, Y - 3 * Ui, W + 6 * Ui, H + 6 * Ui);
	DrawRect(FLinearColor(0.18f, 0.05f, 0.04f, 0.95f), X, Y, W, H);
	if (LagPct > Pct) DrawRect(FLinearColor(0.98f, 0.86f, 0.62f, 0.9f), X, Y, W * FMath::Clamp(LagPct, 0.f, 1.f), H);
	DrawRect(Fill, X, Y, W * FMath::Clamp(Pct, 0.f, 1.f), H);
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.14f), X, Y, W * FMath::Clamp(Pct, 0.f, 1.f), H * 0.4f);
}

void AEldoriaHUD::Coin(float X, float Y, float R)
{
	FCanvasNGonItem Outer(FVector2D(X, Y), FVector2D(R, R), 24, FLinearColor(0.55f, 0.36f, 0.06f));
	Canvas->DrawItem(Outer);
	FCanvasNGonItem Inner(FVector2D(X, Y), FVector2D(R * 0.78f, R * 0.78f), 24, FLinearColor(1.f, 0.78f, 0.25f));
	Canvas->DrawItem(Inner);
	FCanvasNGonItem Shine(FVector2D(X - R * 0.25f, Y - R * 0.25f), FVector2D(R * 0.25f, R * 0.25f), 12, FLinearColor(1.f, 0.95f, 0.7f));
	Canvas->DrawItem(Shine);
}

TArray<FString> AEldoriaHUD::Wrap(const FString& Str, float Size, float MaxWidth)
{
	TArray<FString> Words, Lines;
	Str.ParseIntoArray(Words, TEXT(" "));
	FString Line;
	for (const FString& Word : Words)
	{
		const FString Try = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
		if (!Line.IsEmpty() && Measure(Try, Size, false).X > MaxWidth)
		{
			Lines.Add(Line);
			Line = Word;
		}
		else
		{
			Line = Try;
		}
	}
	if (!Line.IsEmpty()) Lines.Add(Line);
	return Lines;
}

void AEldoriaHUD::ToggleHelp()
{
	if (bHelpVisible)
	{
		bShowHelp = false;
		bIntroHelpDismissed = true;
	}
	else
	{
		bShowHelp = true;
	}
}

void AEldoriaHUD::AddNumber(const UObject* Context, const FVector& Location, float Amount, const FLinearColor& Color)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(Context, 0);
	if (AEldoriaHUD* HUD = PC ? Cast<AEldoriaHUD>(PC->GetHUD()) : nullptr)
	{
		const FVector Jitter(FMath::FRandRange(-30.f, 30.f), FMath::FRandRange(-30.f, 30.f), 0.f);
		HUD->Popups.Add({ Location + Jitter, FString::Printf(TEXT("%d"), FMath::RoundToInt(Amount)), Color, static_cast<float>(PC->GetWorld()->GetTimeSeconds()) });
	}
}

// ---------- main ----------

void AEldoriaHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas) return;
	// keep everything readable even in a small window
	Ui = FMath::Max(Canvas->ClipY / 1080.f, 0.9f);
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	const float Now = GetWorld()->GetTimeSeconds();

	AEldoriaCharacter* Hero = Cast<AEldoriaCharacter>(GetOwningPawn());
	AEldoriaGameMode* GM = GetWorld()->GetAuthGameMode<AEldoriaGameMode>();
	if (!Hero || !GM || GM->IsMenuOpen() || GM->IsIntroPlaying()) return;

	DrawObjective();
	DrawEnemyBars();
	DrawPopups(Now);
	DrawHeroPanel(Hero);
	DrawQuestCard(GM);

	// blood on the screen when the hero is hurt; a faint, slowly pulsing trace while he is near death
	{
		const float HealthPct = Hero->Health->GetPercent();
		const float Lingering = HealthPct < 0.3f && !Hero->IsDead() ? (0.3f - HealthPct) / 0.3f * (0.35f + 0.1f * FMath::Sin(Now * 3.f)) : 0.f;
		const float Amount = FMath::Clamp(FMath::Max(Hero->GetBloodFlash(), Lingering), 0.f, 1.f);
		if (Amount > 0.01f) DrawBloodOverlay(Amount);
	}

	// boss bar
	if (AEnemyCharacter* Boss = GM->ActiveBoss())
	{
		const float BW = FMath::Min(1100.f * Ui, W * 0.7f);
		Text(Boss->GetDisplayName().ToString().ToUpper(), W / 2, H - 150 * Ui, 30, FLinearColor(1.f, 0.85f, 0.7f), true);
		Bar(W / 2 - BW / 2, H - 100 * Ui, BW, 26 * Ui, Boss->Health->GetPercent(), FLinearColor(0.72f, 0.08f, 0.06f));
		Bar(W / 2 - BW / 2, H - 66 * Ui, BW, 10 * Ui, Boss->GetStancePercent(), FLinearColor(1.f, 0.72f, 0.15f));
		const FVector BossHead = Project(Boss->GetActorLocation() + FVector(0.f, 0.f, 220.f));
		if (BossHead.Z > 0.f) DrawGlint(Boss, BossHead);
	}

	// execution prompt over a dazed enemy within reach
	if (Hero->CanExecuteNow())
	{
		const FString P = TEXT("Press  E  to execute");
		const float PW = Measure(P, 30).X + 60 * Ui;
		Panel(W / 2 - PW / 2, H * 0.62f, PW, 66 * Ui, 0.85f);
		Text(P, W / 2, H * 0.62f + 14 * Ui, 30, FLinearColor(1.f, 0.35f, 0.25f), true);
	}

	// interaction prompt
	for (TActorIterator<AChest> It(GetWorld()); It; ++It)
	{
		if (!It->IsOpened() && FVector::Dist(It->GetActorLocation(), Hero->GetActorLocation()) < 260.f)
		{
			const FString P = TEXT("Press  E  to open the chest");
			const float PW = Measure(P, 28).X + 60 * Ui;
			Panel(W / 2 - PW / 2, H * 0.70f, PW, 64 * Ui, 0.8f);
			Text(P, W / 2, H * 0.70f + 14 * Ui, 28, Gold, true);
			break;
		}
	}

	// combo counter and named finishers
	if (Hero->GetHitCombo() >= 2 && Now - Hero->GetLastHitTime() < 2.5f)
	{
		Text(FString::Printf(TEXT("%d HITS"), Hero->GetHitCombo()), W - 150 * Ui, H * 0.42f, 40, Gold, true);
	}
	if (Now < Hero->GetCalloutUntil())
	{
		const float Alpha = FMath::Clamp(Hero->GetCalloutUntil() - Now, 0.f, 1.f);
		Text(Hero->GetCallout().ToString().ToUpper() + TEXT("!"), W / 2, H * 0.6f, 42, FLinearColor(1.f, 0.82f, 0.45f, Alpha), true);
	}

	// controls card: shown for the first moments of play, and any time with H
	const float Since = Now - GM->StartTime;
	bHelpVisible = bShowHelp || (Since < 30.f && !bIntroHelpDismissed);
	if (bShowHelp) DrawHelp(1.f);
	else if (bHelpVisible) DrawHelp(FMath::Clamp(30.f - Since, 0.f, 1.f));
	else Text(TEXT("H  -  Controls"), 40 * Ui, H - 50 * Ui, 18, FLinearColor(Muted.R, Muted.G, Muted.B, 0.8f), false, false);

	// centre messages
	if (Now < GM->MessageUntil)
	{
		const float Alpha = FMath::Clamp(GM->MessageUntil - Now, 0.f, 1.f);
		Text(GM->Message.ToString(), W / 2, H * 0.2f, 46, FLinearColor(1.f, 0.95f, 0.85f, Alpha), true);
	}

	if (GM->IsHeroDown())
	{
		DrawRect(FLinearColor(0.15f, 0.f, 0.f, 0.5f), 0, 0, W, H);
		Text(TEXT("YOU HAVE FALLEN"), W / 2, H * 0.4f, 72, FLinearColor(0.95f, 0.25f, 0.22f), true);
		Text(TEXT("Rising again at the last checkpoint..."), W / 2, H * 0.4f + 100 * Ui, 26, Ink, true, false);
	}
	if (GM->IsVictory())
	{
		const int32 Secs = FMath::FloorToInt(GM->FinishTime - GM->StartTime);
		const FString Stats = FString::Printf(TEXT("Time %d:%02d     Coins %d     Enemies %d     Falls %d"), Secs / 60, Secs % 60, Hero->GetCoins(), GM->Kills, GM->Deaths);
		const float SW = Measure(Stats, 30).X + 80 * Ui;
		Panel(W / 2 - SW / 2, H * 0.3f - 20 * Ui, SW, 80 * Ui, 0.8f);
		Text(Stats, W / 2, H * 0.3f, 30, Gold, true);
	}
}

void AEldoriaHUD::DrawHeroPanel(AEldoriaCharacter* Hero)
{
	const float X = 32 * Ui, Y = 32 * Ui, PW = 520 * Ui, PH = 172 * Ui;
	Panel(X, Y, PW, PH);
	Text(TEXT("SIR ALDRIC"), X + 24 * Ui, Y + 16 * Ui, 26, Gold);

	const float Pct = Hero->Health->GetPercent();
	ShownHealth = FMath::FInterpTo(ShownHealth, Pct, GetWorld()->GetDeltaSeconds(), 2.5f);
	const float BX = X + 24 * Ui, BY = Y + 62 * Ui, BW = PW - 48 * Ui, BH = 38 * Ui;
	const FLinearColor Fill = Pct < 0.3f ? FLinearColor(0.95f, 0.15f, 0.1f) : HealthRed;
	Bar(BX, BY, BW, BH, Pct, Fill, ShownHealth);
	Text(FString::Printf(TEXT("HEALTH   %d / %d"), FMath::CeilToInt(Hero->Health->Health), FMath::RoundToInt(Hero->Health->MaxHealth)), BX + BW / 2, BY + 6 * Ui, 20, Ink, true);

	Coin(X + 46 * Ui, Y + 136 * Ui, 18 * Ui);
	Text(FString::Printf(TEXT("%d  coins"), Hero->GetCoins()), X + 76 * Ui, Y + 118 * Ui, 28, Gold);
}

void AEldoriaHUD::DrawQuestCard(AEldoriaGameMode* GM)
{
	const float W = Canvas->ClipX;
	const float CW = 600 * Ui, X = W - CW - 32 * Ui, Y = 32 * Ui;
	const FQuestStage* Stage = GM->CurrentStage();
	const FString Title = Stage ? Stage->Title.ToString() : TEXT("Eldoria Is Free");
	const FString Desc = Stage ? Stage->Description.ToString() : TEXT("Explore the kingdom and find every coin.");
	const TArray<FString> Lines = Wrap(Desc, 22, CW - 48 * Ui);
	const float CH = (130 + 32 * Lines.Num() + (Stage ? 46 : 0)) * Ui;

	Panel(X, Y, CW, CH);
	DrawRect(Gold, X, Y, 6 * Ui, CH);
	Text(TEXT("CURRENT QUEST"), X + 28 * Ui, Y + 16 * Ui, 18, Gold);
	Text(Title, X + 28 * Ui, Y + 44 * Ui, 34, Ink);
	float LY = Y + 100 * Ui;
	for (const FString& L : Lines)
	{
		Text(L, X + 28 * Ui, LY, 22, Muted, false, false);
		LY += 32 * Ui;
	}
	if (Stage)
	{
		Text(TEXT(">  ") + GM->ProgressText().ToString(), X + 28 * Ui, LY + 12 * Ui, 24, Good);
	}
}

void AEldoriaHUD::DrawHelp(float Alpha)
{
	const float H = Canvas->ClipY;
	const float PW = 980 * Ui, PH = 364 * Ui, X = 32 * Ui, Y = H - PH - 32 * Ui;
	Panel(X, Y, PW, PH, 0.75f * Alpha);
	const FLinearColor C(Ink.R, Ink.G, Ink.B, Alpha);
	const FLinearColor K(Gold.R, Gold.G, Gold.B, Alpha);
	Text(TEXT("CONTROLS"), X + 24 * Ui, Y + 16 * Ui, 22, K);
	const TPair<const TCHAR*, const TCHAR*> Rows[] = {
		{ TEXT("Move"), TEXT("W A S D  or  Arrow keys") },
		{ TEXT("Light attack"), TEXT("J  or  Z") },
		{ TEXT("Heavy attack"), TEXT("K  or  X") },
		{ TEXT("Dodge (with a direction)"), TEXT("C  or  L   (also Q)") },
		{ TEXT("Block  /  Parry"), TEXT("Hold  F  or  V  /  tap just before a hit") },
		{ TEXT("Execute a dazed enemy"), TEXT("E") },
		{ TEXT("Jump  /  Run"), TEXT("Space (x2)   /   hold Shift") },
	};
	float RY = Y + 58 * Ui;
	for (const auto& Row : Rows)
	{
		Text(Row.Key, X + 24 * Ui, RY, 21, C, false, false);
		Text(Row.Value, X + 400 * Ui, RY, 21, K);
		RY += 34 * Ui;
	}
	const FLinearColor M(Muted.R, Muted.G, Muted.B, Alpha);
	Text(TEXT("Blue glint: block or parry it.   Red glint: unblockable, dodge it!"), X + 24 * Ui, RY + 4 * Ui, 18, M, false, false);
	Text(TEXT("Combos: Light x3  |  Light, Light, Heavy  |  Heavy, Heavy  |  attack in the air.   H hides this card."), X + 24 * Ui, RY + 32 * Ui, 17, M, false, false);
}

void AEldoriaHUD::DrawObjective()
{
	AEldoriaGameMode* GM = GetWorld()->GetAuthGameMode<AEldoriaGameMode>();
	FVector Target;
	APawn* Hero = GetOwningPawn();
	if (!GM || !Hero || !GM->ObjectiveLocation(Target)) return;

	const float Dist = FVector::Dist(Target, Hero->GetActorLocation()) / 100.f;
	if (Dist < 8.f) return;
	FVector Screen = Project(Target + FVector(0.f, 0.f, 250.f));
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	// behind the camera or off screen: pin the marker to the edge
	const FVector CamDir = GetOwningPlayerController()->PlayerCameraManager->GetCameraRotation().Vector();
	if (FVector::DotProduct(CamDir, Target - Hero->GetActorLocation()) < 0.f) Screen = FVector(W - Screen.X, H - 120 * Ui, 0.f);
	Screen.X = FMath::Clamp(Screen.X, 90 * Ui, W - 90 * Ui);
	Screen.Y = FMath::Clamp(Screen.Y, 260 * Ui, H - 120 * Ui);

	const float S = 24 * Ui;
	auto Diamond = [this, &Screen](float R, const FLinearColor& C)
	{
		FCanvasTriangleItem Top(FVector2D(Screen.X, Screen.Y - R), FVector2D(Screen.X + R, Screen.Y), FVector2D(Screen.X - R, Screen.Y), GWhiteTexture);
		FCanvasTriangleItem Bottom(FVector2D(Screen.X, Screen.Y + R), FVector2D(Screen.X - R, Screen.Y), FVector2D(Screen.X + R, Screen.Y), GWhiteTexture);
		Top.SetColor(C);
		Bottom.SetColor(C);
		Canvas->DrawItem(Top);
		Canvas->DrawItem(Bottom);
	};
	Diamond(S + 4 * Ui, FLinearColor(0.f, 0.f, 0.f, 0.7f));
	Diamond(S, Gold);
	Text(TEXT("OBJECTIVE"), Screen.X, Screen.Y + S + 8 * Ui, 18, Gold, true);
	Text(FString::Printf(TEXT("%d m"), FMath::RoundToInt(Dist)), Screen.X, Screen.Y + S + 32 * Ui, 22, Ink, true);
}

void AEldoriaHUD::DrawEnemyBars()
{
	APawn* Hero = GetOwningPawn();
	for (TActorIterator<AEnemyCharacter> It(GetWorld()); It; ++It)
	{
		AEnemyCharacter* E = *It;
		if (E->IsDead() || E->bBoss || !E->IsEngaged()) continue;
		if (FVector::Dist(E->GetActorLocation(), Hero->GetActorLocation()) > 3000.f) continue;
		const FVector Head = E->IsDown()
			? E->GetAimPoint() + FVector(0.f, 0.f, 90.f * E->GetActorScale3D().Z)
			: E->GetActorLocation() + FVector(0.f, 0.f, 140.f * E->GetActorScale3D().Z);
		const FVector Screen = Project(Head);
		if (Screen.Z <= 0.f) continue;
		Bar(Screen.X - 70 * Ui, Screen.Y, 140 * Ui, 14 * Ui, E->Health->GetPercent(), FLinearColor(0.85f, 0.14f, 0.1f));
		// guard (stance) meter: fills with hits; full = dazed
		const float Stance = E->GetStancePercent();
		if (Stance > 0.01f || E->IsStunned())
		{
			const bool bPulse = E->IsStunned() && FMath::Fmod(GetWorld()->GetTimeSeconds(), 0.4f) < 0.2f;
			Bar(Screen.X - 70 * Ui, Screen.Y + 20 * Ui, 140 * Ui, 8 * Ui, Stance, bPulse ? FLinearColor(1.f, 1.f, 0.7f) : FLinearColor(1.f, 0.72f, 0.15f));
		}
		DrawGlint(E, Screen);
	}
}

void AEldoriaHUD::DrawGlint(AEnemyCharacter* E, const FVector& Screen)
{
	// Ghost of Tsushima style warning: a glint above an enemy about to strike
	const float Now = GetWorld()->GetTimeSeconds();
	if (!E->IsTelegraphing(Now)) return;
	const bool bRed = E->IsAttackUnblockable();
	const FLinearColor C = bRed ? FLinearColor(1.f, 0.15f, 0.1f) : FLinearColor(0.35f, 0.75f, 1.f);
	const float Pulse = 1.f + 0.25f * FMath::Sin(Now * 30.f);
	const float R = 18.f * Ui * Pulse;
	const FVector2D P(Screen.X, Screen.Y - 40.f * Ui);
	FCanvasTriangleItem A(FVector2D(P.X, P.Y - R * 1.6f), FVector2D(P.X + R * 0.5f, P.Y), FVector2D(P.X - R * 0.5f, P.Y), GWhiteTexture);
	FCanvasTriangleItem B(FVector2D(P.X, P.Y + R * 1.6f), FVector2D(P.X - R * 0.5f, P.Y), FVector2D(P.X + R * 0.5f, P.Y), GWhiteTexture);
	FCanvasTriangleItem Cc(FVector2D(P.X - R * 1.6f, P.Y), FVector2D(P.X, P.Y - R * 0.5f), FVector2D(P.X, P.Y + R * 0.5f), GWhiteTexture);
	FCanvasTriangleItem D(FVector2D(P.X + R * 1.6f, P.Y), FVector2D(P.X, P.Y + R * 0.5f), FVector2D(P.X, P.Y - R * 0.5f), GWhiteTexture);
	for (FCanvasTriangleItem* T : { &A, &B, &Cc, &D })
	{
		T->SetColor(C);
		T->BlendMode = SE_BLEND_Additive;
		Canvas->DrawItem(*T);
	}
	if (bRed) Text(TEXT("!"), P.X, P.Y - R * 3.2f, 30, C, true);
}

void AEldoriaHUD::DrawPopups(float Now)
{
	for (int32 i = Popups.Num() - 1; i >= 0; --i)
	{
		const float Age = Now - Popups[i].Born;
		if (Age > 1.f)
		{
			Popups.RemoveAt(i);
			continue;
		}
		const FVector Screen = Project(Popups[i].Location + FVector(0.f, 0.f, 60.f + Age * 120.f));
		if (Screen.Z <= 0.f) continue;
		FLinearColor C = Popups[i].Color;
		C.A = 1.f - FMath::Max(0.f, Age - 0.5f) * 2.f;
		Text(Popups[i].Text, Screen.X, Screen.Y, 34 + (1.f - Age) * 10.f, C, true);
	}
}

void AEldoriaHUD::DrawBloodOverlay(float Amount)
{
	if (!BloodMaterial)
	{
		UMaterialInterface* Base = UEldoriaSettings::Get()->BloodScreenMaterial.LoadSynchronous();
		if (!Base) return;
		BloodMaterial = UMaterialInstanceDynamic::Create(Base, this);
	}
	BloodMaterial->SetScalarParameterValue(TEXT("Amount"), Amount);
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	// splashes from each corner and edge reaching in toward the middle, the centre left clear
	struct FSplash { float X, Y, Size, Rotation; float U, V; };
	const FSplash Splashes[] =
	{
		{ -0.12f, -0.18f, 0.62f,  20.f, 0.f, 0.f },
		{  0.70f, -0.22f, 0.58f, 160.f, 0.5f, 0.f },
		{ -0.16f,  0.62f, 0.60f, -35.f, 0.f, 0.5f },
		{  0.74f,  0.58f, 0.64f, 215.f, 0.5f, 0.5f },
		{  0.36f,  0.80f, 0.40f, 100.f, 0.25f, 0.25f },
	};
	for (const FSplash& S : Splashes)
	{
		const float Size = S.Size * H;
		DrawMaterial(BloodMaterial, S.X * W, S.Y * H, Size, Size, S.U, S.V, 0.5f, 0.5f, 1.f, false, S.Rotation, FVector2D(0.5f, 0.5f));
	}
}
