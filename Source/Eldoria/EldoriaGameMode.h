#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "EldoriaGameMode.generated.h"

class AEnemyCharacter;
class SEldoriaMenu;
class UEldoriaSaveGame;

enum class EMenuPage : uint8
{
	Main,
	Pause,
	Save,
	Load,
	Difficulty
};

enum class EDifficulty : uint8
{
	Easy,
	Medium,
	Hard
};

// How hard the world pushes back. Chosen when a new game starts and kept in the save.
struct FDifficultyTuning
{
	float HeroHealth = 150.f;
	float EnemyDamage = 1.f;		// multiplier on every enemy blow
	float EnemySpeed = 1.f;			// walk/run speed multiplier
	float AttackCooldown = 1.f;		// multiplier on the pause between an enemy's attacks
	float AttackRate = 1.f;			// enemy swing animation speed (faster = less time to react)
	int32 MaxAttackers = 2;			// how many enemies may swing at the hero at once
};

struct FSaveSlotInfo
{
	FString Slot;
	FString Label;
	bool bExists = false;
	FDateTime SavedAt;
	FString QuestTitle;
	float PlayTime = 0.f;
};

UENUM()
enum class EQuestGoal : uint8
{
	DefeatGroup,
	FindRoyalKey,
	OpenGate,
	DefeatBoss
};

USTRUCT()
struct FQuestStage
{
	GENERATED_BODY()

	FText Title;
	FText Description;
	EQuestGoal Goal = EQuestGoal::DefeatGroup;
	FName Group;
	// actors carrying this tag mark where the objective is (drawn as the HUD waypoint)
	FName ObjectiveTag;
};

// Runs the quest line: break the western warband, clear the ruins, claim the Royal Key,
// open the castle and defeat the Bone King. Also handles death, respawn and victory.
UCLASS()
class ELDORIA_API AEldoriaGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AEldoriaGameMode();
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void FinishRestartPlayer(AController* NewPlayer, const FRotator& StartRotation) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	// ---------- menus ----------
	void ShowMenu(EMenuPage Page);
	void CloseMenu();
	bool IsMenuOpen() const { return Menu.IsValid(); }
	void TogglePauseMenu();

	// ---------- game flow ----------
	void NewGame(EDifficulty InDifficulty);
	void SetDifficulty(EDifficulty InDifficulty);
	EDifficulty GetDifficulty() const { return Difficulty; }
	static FText DifficultyName(EDifficulty D);

	// graphics quality 0 Low, 1 Medium, 2 High (kept in GameUserSettings.ini)
	static int32 GetQuality();
	static void ApplyQuality(int32 Level, bool bSave);
	static FText QualityName(int32 Level);
	// the tuning for the current game (Medium when there is no game mode, e.g. in the editor)
	static const FDifficultyTuning& Tuning(const UObject* Context);
	void ContinueGame();
	void LoadFromSlot(const FString& Slot);
	bool SaveToSlot(const FString& Slot);
	void ReturnToMainMenu();
	void QuitGame();

	static TArray<FSaveSlotInfo> ListSlots();
	static FString LatestSlot();
	static const TCHAR* AutosaveSlot() { return TEXT("Autosave"); }

	// world changes recorded for saves
	void OnChestOpened(const AActor* Chest) { OpenedChests.AddUnique(Chest->GetName()); }
	void OnPickupCollected(const AActor* Pickup) { CollectedPickups.AddUnique(Pickup->GetName()); }

	void OnEnemyKilled(AEnemyCharacter* Enemy);
	void OnCoinCollected(int32 Value) { CoinsCollected += Value; }
	void OnRoyalKeyFound();
	void OnHeroDied();
	void ShowMessage(const FText& Text, float Seconds);

	// ---------- the new-game story ----------
	bool IsIntroPlaying() const { return bIntroPlaying; }
	FText IntroLine() const { return IntroLineText; }
	float IntroLineAlpha() const { return IntroLineOpacity; }
	// ends the story (also when the player skips it) and hands the camera to the hero
	void FinishIntro();
	virtual void Tick(float DeltaSeconds) override;

	// Only a few enemies may swing at the hero at once; the rest hold back and wait their turn.
	bool RequestAttackToken(AEnemyCharacter* Enemy);
	void ReleaseAttackToken(AEnemyCharacter* Enemy);
	bool HasAttackToken(const AEnemyCharacter* Enemy) const;
	int32 MaxAttackers = 2;

	bool HasRoyalKey() const { return bHasRoyalKey; }
	bool IsVictory() const { return bVictory; }
	bool IsHeroDown() const { return bHeroDown; }
	bool IsQuestDone() const { return Stage >= Stages.Num(); }

	const FQuestStage* CurrentStage() const { return Stages.IsValidIndex(Stage) ? &Stages[Stage] : nullptr; }
	FText ProgressText() const;
	bool ObjectiveLocation(FVector& Out) const;
	AEnemyCharacter* ActiveBoss() const;

	FText Message;
	float MessageUntil = 0.f;
	int32 CoinsCollected = 0;
	int32 Kills = 0;
	int32 Deaths = 0;
	float StartTime = 0.f;
	float FinishTime = 0.f;

private:
	void CheckProgress();
	void StartPlaying();
	// tells the story, then starts playing (or starts at once when there is no story to tell)
	void StartIntro();
	void BeginIntroShot(int32 Index);
	void ApplySave(UEldoriaSaveGame* Save);
	void ApplyHeroSave();
	// foliage trees ship without collision; give every tall tree a solid trunk
	void AddTreeTrunkColliders();
	// Undergrowth that hides fighters: thinned everywhere, cleared around enemies, objectives and the
	// paths between them (done at load, so rebuilding the map from the village never loses it)
	void ThinFoliage();
	void StartMusic();
	void SetMusicLevel(bool bMenu);
	UFUNCTION() void OnMusicFinished();
	void CompleteStage();
	void RespawnHero();
	int32 AliveInGroup(FName Group) const;

	TArray<FQuestStage> Stages;
	TArray<TWeakObjectPtr<AEnemyCharacter>> AttackTokens;
	TMap<FName, int32> GroupTotals;
	int32 Stage = 0;
	bool bHasRoyalKey = false;
	bool bVictory = false;
	bool bHeroDown = false;
	FTransform Checkpoint;

	TSharedPtr<SEldoriaMenu> Menu;
	UPROPERTY() class UAudioComponent* MusicComponent = nullptr;
	FString PendingLoad;
	bool bNewGameRequested = false;
	EDifficulty Difficulty = EDifficulty::Medium;
	UPROPERTY() UEldoriaSaveGame* LoadedSave = nullptr;
	TArray<FString> KilledEnemies;
	TArray<FString> OpenedChests;
	TArray<FString> CollectedPickups;

	FTimerHandle ProgressTimer;

	bool bIntroPlaying = false;
	int32 IntroShot = -1;
	float IntroShotTime = 0.f;
	float IntroShotLength = 0.f;
	FVector IntroAnchor = FVector::ZeroVector;
	FRotator IntroAnchorFrame = FRotator::ZeroRotator;
	FText IntroLineText;
	float IntroLineOpacity = 0.f;
	bool bScoreDucked = false;
	bool bSceneryOnly = false;
	UPROPERTY() class ACameraActor* IntroCamera = nullptr;
	UPROPERTY() class UAudioComponent* IntroVoice = nullptr;
	UPROPERTY() class UAudioComponent* IntroScore = nullptr;
	TSharedPtr<class SEldoriaIntro> IntroOverlay;
	FTimerHandle RespawnTimer;
};
