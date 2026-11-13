#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EldoriaSettings.h"
#include "EnemyCharacter.generated.h"

class UHealthComponent;
class UAnimMontage;
class UAnimSequenceBase;
class UMaterialInstanceDynamic;

UENUM()
enum class EEnemyState : uint8
{
	Idle,
	Chase,
	Attack,
	Stagger,
	Return,
	// on the ground after a heavy blow, then standing back up
	Down,
	GettingUp,
	// guard broken: dazed and open to an execution
	Stunned,
	Dead
};

// A member of the Bone King's army. Behaviour is a small state machine; the archetype
// (mesh, animations, stats) comes from UEldoriaSettings so one class serves every enemy type.
UCLASS()
class ELDORIA_API AEnemyCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AEnemyCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	// how hard the next hit throws this enemy back (set by the hero's move before applying damage)
	float PendingKnockback = 420.f;
	// guard damage and knockdown for the next hit (also set by the hero's move)
	float PendingStance = 10.f;
	bool bPendingKnockdown = false;

	bool IsStunned() const { return State == EEnemyState::Stunned; }
	bool IsDown() const { return State == EEnemyState::Down || State == EEnemyState::GettingUp; }
	// where a blow should land: the body on the ground when knocked down, otherwise the capsule
	FVector GetAimPoint() const;
	// speed for the current difficulty
	void ApplyDifficulty();
	float GetStancePercent() const { return Stats.MaxStance > 0.f ? FMath::Clamp(Stance / Stats.MaxStance, 0.f, 1.f) : 0.f; }
	// the glint shown while an attack winds up; red ones cannot be blocked
	bool IsTelegraphing(float Now) const { return State == EEnemyState::Attack && Now - TelegraphTime < 0.6f; }
	bool IsAttackUnblockable() const { return State == EEnemyState::Attack && bCurrentUnblockable; }
	// the hero blocked at the last moment: the attack is deflected and the enemy reels
	void OnParried(AActor* By);
	// a finishing blow on a stunned enemy
	void Execute(AActor* By);
	EEnemyState GetStateForTest() const { return State; }
	void KnockdownForTest(const FVector& Away) { Knockdown(Away, 500.f); }

	// which entry of UEldoriaSettings::Enemies to use
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy") FName Archetype = TEXT("Grunt");
	// quest group this enemy belongs to (e.g. WestCamp, Ruins, Castle)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy") FName Group;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy") bool bBoss = false;
	// a dormant enemy waits until the player comes close or its group is alerted
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy") bool bDormant = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy") UHealthComponent* Health;
	// simple stand-in body shown until a real character mesh is configured
	UPROPERTY(VisibleAnywhere) class UStaticMeshComponent* Placeholder;

	UFUNCTION(BlueprintPure) bool IsDead() const { return State == EEnemyState::Dead; }
	UFUNCTION(BlueprintPure) bool IsEngaged() const { return State != EEnemyState::Idle && State != EEnemyState::Return && State != EEnemyState::Dead; }
	FText GetDisplayName() const { return Stats.DisplayName; }

	void Alert();
	void ReleaseToken();
	void SetMaterialScalar(FName Param, float Value);
	void UpdateDeath(float DeltaSeconds);
	bool HasKey() const;

private:
	void ApplyArchetype();
	void StartAttack();
	void AttackHit();
	void Die();
	void Summon();
	float PlayAnim(UAnimSequenceBase* Anim, float Rate, float DefaultLength);
	void FaceTowards(const FVector& Point, float DeltaSeconds, float Speed);
	UAnimSequenceBase* DirectionalHitAnim(const FVector& FromAttacker) const;
	void Flinch(const FVector& Away, float Knock);
	void Knockdown(const FVector& Away, float Knock);
	// hands the falling body over to physics so it drops and lies flat wherever it lands
	void GoLimp();
	// takes the body back from physics: moves the capsule under it, lines a rising clip up with how
	// it lies and blends from the ragdoll's pose into it. Returns the clip used (null without a ragdoll).
	UAnimSequenceBase* RecoverFromRagdoll(float& OutDuration);
	void GetUp();
	void Stun();
	void InterruptAttack();

	FEnemyArchetype Stats;
	EEnemyState State = EEnemyState::Idle;
	FVector Home;
	float Cooldown = 1.f;
	int32 Summons = 0;
	float RepathTimer = 0.f;
	float StepDistance = 0.f;

	UPROPERTY() TArray<UAnimSequenceBase*> Attacks;
	UPROPERTY() UAnimSequenceBase* HitAnim;
	UPROPERTY() UAnimSequenceBase* DeathAnim;
	UPROPERTY() UAnimSequenceBase* KnockdownAnim;
	UPROPERTY() UAnimSequenceBase* HitBackAnim;
	UPROPERTY() UAnimSequenceBase* HitLeftAnim;
	UPROPERTY() UAnimSequenceBase* HitRightAnim;
	UPROPERTY() UAnimSequenceBase* StunAnim;
	UPROPERTY() UAnimSequenceBase* GetUpAnim;
	UPROPERTY() TArray<UAnimSequenceBase*> KnockdownAnims;
	UPROPERTY() TArray<UAnimSequenceBase*> DeathAnims;
	UPROPERTY() UAnimSequenceBase* CurrentFall = nullptr;
	float Stance = 0.f;
	float LastStanceHitTime = -100.f;
	float TelegraphTime = -100.f;
	bool bCurrentUnblockable = false;
	float KnockdownLength = 0.f;
	// when the current knockdown started; ground hits keep it down, but not forever
	float DownSince = 0.f;
	UPROPERTY() TArray<UMaterialInstanceDynamic*> DynamicMaterials;
	float DeathTime = 0.f;
	bool bDissolving = false;
	float MeltStartZ = 0.f;
	// the body is a ragdoll now (see GoLimp)
	bool bLimp = false;
	// direction the killing blow pushed the body
	FVector DeathPush = FVector::ZeroVector;
	// melting begins only after the body has fully fallen and lain still for a while
	float MeltDelay = 5.f;

	FTimerHandle AttackHitTimer;
	FTimerHandle AttackEndTimer;
	FTimerHandle StaggerTimer;
};
