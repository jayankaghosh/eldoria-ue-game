#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "EldoriaSettings.h"
#include "EldoriaCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UHealthComponent;
class UInputAction;
class UInputMappingContext;
class UAnimMontage;
class UAnimSequenceBase;
class AEnemyCharacter;

UENUM()
enum class EAttackInput : uint8
{
	Light,
	Heavy
};

// The hero, Sir Aldric: third-person movement, light/heavy/special attacks that chain into combos,
// an air slam, shield block and dodge roll.
UCLASS()
class ELDORIA_API AEldoriaCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AEldoriaCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void NotifyControllerChanged() override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
	virtual void Landed(const FHitResult& Hit) override;
	// the head and torso follow the hero's facing, not the camera
	virtual FRotator GetBaseAimRotation() const override { return FRotator(0.f, GetActorRotation().Yaw, 0.f); }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera") USpringArmComponent* CameraBoom;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera") UCameraComponent* FollowCamera;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat") UHealthComponent* Health;
	// simple stand-in body shown until a real character mesh is configured
	UPROPERTY(VisibleAnywhere) class UStaticMeshComponent* Placeholder;

	// walking is the default; holding Shift runs
	UPROPERTY(EditAnywhere, Category = "Movement") float WalkSpeed = 240.f;
	UPROPERTY(EditAnywhere, Category = "Movement") float RunSpeed = 560.f;
	// enemies inside this distance and cone are auto-targeted when attacking
	UPROPERTY(EditAnywhere, Category = "Combat") float AssistRange = 450.f;

	UFUNCTION(BlueprintPure) bool IsBlocking() const { return bBlocking; }
	// thrown to the ground by a crushing or critical blow: a ragdoll, then a slow get-up
	void KnockDown(const FVector& Push);
	bool IsKnockedDown() const { return bKnockedDown; }
	// level camera behind the hero, the whole body in view; used whenever play starts or resumes
	void ResetCamera();
	void ResetCameraLookOnly() { IgnoreLookUntil = FPlatformTime::Seconds() + 0.35; bSkipNextLook = true; }
	UFUNCTION(BlueprintPure) bool IsDead() const;
	UFUNCTION(BlueprintPure) int32 GetCoins() const { return Coins; }

	void AddCoins(int32 Amount) { Coins += Amount; }
	void SetCoins(int32 Amount) { Coins = Amount; }
	void Heal(float Amount);
	void Respawn(const FTransform& At);

	// Weight of a blow, 0..1: freezes the action for a moment, jolts and punches the camera, rumbles.
	void ImpactFeedback(float Strength);

	bool CanExecuteNow() const { return StunnedEnemyInReach() != nullptr; }

	// development: the hero fights the nearest enemy on his own and logs anything odd (e.g. enemies not rendering)
	UFUNCTION(Exec) void EldoriaCombatTest();

	// combo feedback for the HUD
	int32 GetHitCombo() const { return HitCombo; }
	const FText& GetCallout() const { return Callout; }
	float GetCalloutUntil() const { return CalloutUntil; }
	float GetLastHitTime() const { return LastHitTime; }
	float GetDamageFlash() const { return DamageFlash; }
	float GetBloodFlash() const { return BloodFlash; }
	// seconds until the Whirlwind can be used again (0 = ready)

private:
	void BuildInput();
	void LoadArt();

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void StartRun() { bRunning = true; }
	void StopRun() { bRunning = false; }
	// takes the body back from the ragdoll and stands up out of it
	void GetUpFromRagdoll();
	void FinishGetUp();
	void LightAttack() { OnAttackInput(EAttackInput::Light); }
	void HeavyAttack() { OnAttackInput(EAttackInput::Heavy); }
	void StartBlock();
	void StopBlock();
	void Dodge();
	void Interact();
	void ToggleHelp();
	void OpenPauseMenu();
	void JumpPressed();

	void OnAttackInput(EAttackInput Input);
	// picks the move an input leads to, given the move currently playing (None if idle)
	FName ChooseMove(EAttackInput Input, FName From) const;
	bool StartMove(FName MoveName);
	void FaceAttackTarget();
	void MoveHit();
	void MoveEnd();
	// slow-motion moment (perfect dodge, parry, execution), timed in real seconds
	void SlowMotion(float Dilation, float RealSeconds);
	void TryExecute(class AEnemyCharacter* Target);
	class AEnemyCharacter* StunnedEnemyInReach() const;
	void DealDamage(const FHeroMove& Move, const FVector& Origin, bool bAllAround);
	void Die();
	float PlayOrDefault(UAnimSequenceBase* Anim, float Rate, float DefaultLength);

	UPROPERTY() UInputMappingContext* Mapping;
	UPROPERTY() UInputAction* MoveAction;
	UPROPERTY() UInputAction* LookAction;
	UPROPERTY() UInputAction* JumpAction;
	UPROPERTY() UInputAction* RunAction;
	UPROPERTY() UInputAction* LightAction;
	UPROPERTY() UInputAction* HeavyAction;
	UPROPERTY() UInputAction* BlockAction;
	UPROPERTY() UInputAction* DodgeAction;
	UPROPERTY() UInputAction* InteractAction;
	UPROPERTY() UInputAction* HelpAction;
	UPROPERTY() UInputAction* PauseAction;

	UPROPERTY() TMap<FName, UAnimSequenceBase*> MoveAnims;
	UPROPERTY() UAnimSequenceBase* DodgeAnim;
	UPROPERTY() UAnimSequenceBase* HitAnim;
	UPROPERTY() UAnimSequenceBase* DeathAnim;
	UPROPERTY() UAnimSequenceBase* BlockAnim;
	UPROPERTY() UAnimSequenceBase* JumpAnim = nullptr;
	UPROPERTY() UAnimSequenceBase* LandAnim = nullptr;
	UPROPERTY() UAnimMontage* BlockMontage;

	FHeroMove CurrentMove;
	TMap<FName, float> MoveReadyAt;
	EAttackInput QueuedInput = EAttackInput::Light;
	FVector LastInputDir = FVector::ZeroVector;
	FText Callout;

	bool bRunning = false;
	bool bKnockedDown = false;
	// the body is a ragdoll (knocked down or dead)
	bool bLimp = false;
	// how much of the ragdoll's pose still shows while standing up (1 = all physics)
	float RagdollBlend = 0.f;
	float LastKnockdownTime = -100.f;
	FTimerHandle KnockdownTimer;
	bool bBlocking = false;
	bool bAttacking = false;
	bool bQueued = false;
	bool bDodging = false;
	bool bAirSlam = false;
	int32 Coins = 0;
	int32 HitCombo = 0;
	float LastHitTime = -100.f;
	float CalloutUntil = 0.f;
	float InvulnerableUntil = 0.f;
	float ComboWindowStart = 0.f;
	float LastInputTime = -100.f;
	float TargetArmLength = 420.f;
	float StepDistance = 0.f;
	float DamageFlash = 0.f;
	float BloodFlash = 0.f;
	double IgnoreLookUntil = 0.0;
	bool bSkipNextLook = false;
	float ShakeTime = 0.f;
	float ShakeStrength = 0.f;
	float ShakeDuration = 0.35f;
	FVector2D ShakeDirection = FVector2D::ZeroVector;
	float CameraPunch = 0.f;
	float FovPunch = 0.f;
	float FringePulse = 0.f;
	double HitStopUntil = 0.0;
	float HeartbeatTimer = 0.f;
	float CombatTestTime = -1.f;
	int32 CombatTestStep = 0;
	TWeakObjectPtr<class AEnemyCharacter> CombatTestTarget;
	// -eldoriasidecam: films the test fight from the side, so nothing stands between camera and enemy
	TWeakObjectPtr<class ACameraActor> CombatTestCamera;
	void TickCombatTest(float DeltaSeconds);
	float DodgeStartTime = -100.f;
	float BlockStartTime = -100.f;
	bool bPerfectDodgeUsed = false;
	bool bExecuting = false;
	UPROPERTY() class UParticleSystemComponent* DodgeTrail = nullptr;
	UPROPERTY() UAnimSequenceBase* ExecutionAnim = nullptr;
	UPROPERTY() class UParticleSystemComponent* BlockGlow = nullptr;
	FVector BaseSocketOffset = FVector(0.f, 55.f, 70.f);
	// in a fight the camera moves further over the right shoulder and back, so the hero's own body
	// never hides the enemy he is facing
	FVector CombatSocketOffset = FVector(0.f, 115.f, 95.f);
	float CombatFraming = 0.f;
	UPROPERTY() TArray<class UMaterialInstanceDynamic*> DynamicMaterials;

	FTimerHandle HitTimer;
	FTimerHandle EndTimer;
	FTimerHandle DodgeTimer;
	FTimerHandle BlockHoldTimer;
};
