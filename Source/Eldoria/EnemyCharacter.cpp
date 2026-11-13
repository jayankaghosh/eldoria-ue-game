#include "EnemyCharacter.h"
#include "EldoriaCharacter.h"
#include "EldoriaGameMode.h"
#include "HealthComponent.h"
#include "Pickups.h"
#include "EldoriaAnimInstance.h"
#include "EldoriaHUD.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Particles/ParticleSystem.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/DamageEvents.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/BodySetup.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "EngineUtils.h"
#include "TimerManager.h"

AEnemyCharacter::AEnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = AAIController::StaticClass();
	bUseControllerRotationYaw = false;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.f, 420.f, 0.f);
	Move->bUseRVOAvoidance = true;
	// animation blueprints read acceleration to decide when to play the run cycle
	Move->GetNavMovementProperties()->bUseAccelerationForPaths = true;
	Move->AvoidanceConsiderationRadius = 300.f;

	// the follow camera passes through characters instead of snapping in front of them
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	Health = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Capsule(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	Placeholder = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Placeholder"));
	Placeholder->SetupAttachment(RootComponent);
	Placeholder->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Placeholder->SetRelativeScale3D(FVector(0.8f, 0.8f, 1.9f));
	if (Capsule.Succeeded()) Placeholder->SetStaticMesh(Capsule.Object);
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -90.f), FRotator(0.f, -90.f, 0.f));
}

void AEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();
	ApplyArchetype();
	Home = GetActorLocation();
	Cooldown = FMath::FRandRange(0.5f, 1.5f);
}

void AEnemyCharacter::ApplyArchetype()
{
	if (const FEnemyArchetype* A = UEldoriaSettings::Get()->FindEnemy(Archetype))
	{
		Stats = *A;
	}
	if (Stats.DisplayName.IsEmpty()) Stats.DisplayName = FText::FromName(Archetype);

	if (USkeletalMesh* Mesh = Stats.Mesh.LoadSynchronous())
	{
		Placeholder->SetVisibility(false);
		GetMesh()->SetSkeletalMesh(Mesh);
		GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, Stats.MeshZ), FRotator(0.f, Stats.MeshYaw, 0.f));
		// own material instances so this enemy can flash when hit and dissolve when it dies
		DynamicMaterials.Reset();
		for (int32 i = 0; i < GetMesh()->GetNumMaterials(); ++i)
		{
			if (UMaterialInstanceDynamic* M = GetMesh()->CreateAndSetMaterialInstanceDynamic(i)) DynamicMaterials.Add(M);
		}
		// generous bounds so fast hit reactions never get culled for a frame
		GetMesh()->SetBoundsScale(1.6f);
		// Stay on the full-detail LOD: an LOD switch mid-fight needs pipeline states that may not be
		// compiled yet, and until they are the renderer skips the mesh, so the enemy blinks out.
		GetMesh()->SetForcedLOD(1);
	}
	if (UClass* Anim = Stats.AnimClass.LoadSynchronous())
	{
		GetMesh()->SetAnimInstanceClass(Anim);
	}
	else if (!Stats.IdleAnim.IsNull() || !Stats.MoveAnim.IsNull())
	{
		GetMesh()->SetAnimInstanceClass(UEldoriaAnimInstance::StaticClass());
		if (UEldoriaAnimInstance* Native = Cast<UEldoriaAnimInstance>(GetMesh()->GetAnimInstance()))
		{
			Native->Setup(Stats.IdleAnim.LoadSynchronous(), Stats.MoveAnim.LoadSynchronous(), Stats.MoveAnimSpeed);
		}
	}
	Attacks.Reset();
	for (const TSoftObjectPtr<UAnimSequenceBase>& M : Stats.AttackMontages)
	{
		if (UAnimSequenceBase* Loaded = M.LoadSynchronous()) Attacks.Add(Loaded);
	}
	HitAnim = Stats.HitAnim.LoadSynchronous();
	DeathAnim = Stats.DeathAnim.LoadSynchronous();
	KnockdownAnim = Stats.KnockdownAnim.LoadSynchronous();
	HitBackAnim = Stats.HitBackAnim.LoadSynchronous();
	HitLeftAnim = Stats.HitLeftAnim.LoadSynchronous();
	HitRightAnim = Stats.HitRightAnim.LoadSynchronous();
	StunAnim = Stats.StunAnim.LoadSynchronous();
	GetUpAnim = Stats.GetUpAnim.LoadSynchronous();
	KnockdownAnims.Reset();
	for (const TSoftObjectPtr<UAnimSequenceBase>& A : Stats.KnockdownAnims) if (UAnimSequenceBase* L = A.LoadSynchronous()) KnockdownAnims.Add(L);
	if (KnockdownAnims.Num() == 0 && KnockdownAnim) KnockdownAnims.Add(KnockdownAnim);
	DeathAnims.Reset();
	for (const TSoftObjectPtr<UAnimSequenceBase>& A : Stats.DeathAnims) if (UAnimSequenceBase* L = A.LoadSynchronous()) DeathAnims.Add(L);
	if (DeathAnims.Num() == 0 && DeathAnim) DeathAnims.Add(DeathAnim);

	SetActorScale3D(FVector(Stats.Scale));
	ApplyDifficulty();
	Health->Reset(Stats.MaxHealth);
}

float AEnemyCharacter::PlayAnim(UAnimSequenceBase* Anim, float Rate, float DefaultLength)
{
	const float Played = UEldoriaSettings::PlayAnim(GetMesh(), Anim, Rate, Stats.HitSlot);
	return Played > 0.f ? Played : DefaultLength;
}

void AEnemyCharacter::FaceTowards(const FVector& Point, float DeltaSeconds, float Speed)
{
	const FRotator Want = (Point - GetActorLocation()).GetSafeNormal2D().Rotation();
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(0.f, Want.Yaw, 0.f), DeltaSeconds, Speed));
}

bool AEnemyCharacter::HasKey() const
{
	const AEldoriaGameMode* GM = GetWorld()->GetAuthGameMode<AEldoriaGameMode>();
	return GM && GM->HasRoyalKey();
}

void AEnemyCharacter::Alert()
{
	if (bBoss && !HasKey()) return;
	if (State == EEnemyState::Idle && !IsDead())
	{
		bDormant = false;
		State = EEnemyState::Chase;
		if (bBoss) UEldoriaSettings::PlaySound(this, UEldoriaSettings::Get()->BossRoarSound, GetActorLocation(), 1.6f, 0.f);
	}
}

void AEnemyCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (IsDead())
	{
		UpdateDeath(DeltaSeconds);
		return;
	}

	if (State != EEnemyState::Stunned && GetWorld()->GetTimeSeconds() - LastStanceHitTime > 2.5f)
	{
		Stance = FMath::Max(0.f, Stance - Stats.MaxStance * 0.25f * DeltaSeconds);
	}
	if (State == EEnemyState::Down || State == EEnemyState::GettingUp || State == EEnemyState::Stunned) return;

	ACharacter* Hero = UGameplayStatics::GetPlayerCharacter(this, 0);
	AEldoriaCharacter* Player = Cast<AEldoriaCharacter>(Hero);
	if (!Player) return;

	const FVector ToPlayer = Player->GetActorLocation() - GetActorLocation();
	const float Dist = ToPlayer.Size2D();
	const float Reach = Stats.AttackRange * Stats.Scale;
	Cooldown -= DeltaSeconds;
	AAIController* AI = Cast<AAIController>(GetController());
	// while the story is being told the world holds still
	if (const AEldoriaGameMode* GM = GetWorld()->GetAuthGameMode<AEldoriaGameMode>(); GM && GM->IsIntroPlaying()) return;

	switch (State)
	{
	case EEnemyState::Idle:
		// the Bone King is warded until the hero carries the Royal Key
		if (bBoss && !HasKey()) break;
		if (!Player->IsDead() && Dist < Stats.AggroRange * (bDormant ? 0.6f : 1.f) && FMath::Abs(ToPlayer.Z) < 400.f)
		{
			Alert();
			// wake the rest of the group
			if (!Group.IsNone())
			{
				for (TActorIterator<AEnemyCharacter> It(GetWorld()); It; ++It)
				{
					if (It->Group == Group && FVector::Dist(It->GetActorLocation(), GetActorLocation()) < 1200.f) It->Alert();
				}
			}
		}
		break;

	case EEnemyState::Chase:
	{
		if (Player->IsDead() || FVector::Dist2D(GetActorLocation(), Home) > (bBoss ? 3000.f : 4500.f))
		{
			State = EEnemyState::Return;
			break;
		}
		AEldoriaGameMode* GM = GetWorld()->GetAuthGameMode<AEldoriaGameMode>();
		const bool bMayAttack = !GM || GM->HasAttackToken(this) || bBoss;
		if (Dist <= Reach * 0.9f || (!bMayAttack && Dist <= Reach + 320.f))
		{
			if (AI) AI->StopMovement();
			FaceTowards(Player->GetActorLocation(), DeltaSeconds, 8.f);
			if (Cooldown <= 0.f && Dist <= Reach * 0.9f && (!GM || GM->RequestAttackToken(this)))
			{
				StartAttack();
			}
			else if (!bMayAttack)
			{
				// waiting for a turn: circle slowly instead of standing frozen
				if (GM && Cooldown <= 0.f && GM->RequestAttackToken(this)) break;
				const FVector Side = FVector::CrossProduct(ToPlayer.GetSafeNormal2D(), FVector::UpVector) * (GetUniqueID() % 2 ? 1.f : -1.f);
				AddMovementInput(Side, 0.35f);
				if (Dist < Reach + 150.f) AddMovementInput(-ToPlayer.GetSafeNormal2D(), 0.4f);
			}
			break;
		}
		// follow the navmesh when the level has one, otherwise walk straight at the hero
		RepathTimer -= DeltaSeconds;
		bool bPathing = false;
		if (AI && RepathTimer <= 0.f)
		{
			RepathTimer = 0.4f;
			bPathing = AI->MoveToActor(Player, Reach * 0.6f, true, true, true) != EPathFollowingRequestResult::Failed;
		}
		else if (AI)
		{
			bPathing = AI->GetMoveStatus() == EPathFollowingStatus::Moving;
		}
		if (!bPathing) AddMovementInput(ToPlayer.GetSafeNormal2D(), 1.f);
		break;
	}

	case EEnemyState::Attack:
		FaceTowards(Player->GetActorLocation(), DeltaSeconds, bBoss ? 2.5f : 4.f);
		break;

	case EEnemyState::Return:
		ReleaseToken();
		if (AI) AI->MoveToLocation(Home, 80.f);
		else AddMovementInput((Home - GetActorLocation()).GetSafeNormal2D(), 1.f);
		Health->Heal(DeltaSeconds * Stats.MaxHealth * 0.2f);
		if (FVector::Dist2D(GetActorLocation(), Home) < 150.f) State = EEnemyState::Idle;
		if (!Player->IsDead() && Dist < Stats.AggroRange * 0.5f) State = EEnemyState::Chase;
		break;

	default:
		break;
	}

	// footsteps
	const float Speed = GetVelocity().Size2D();
	if (Speed > 80.f && GetCharacterMovement()->IsMovingOnGround())
	{
		StepDistance += Speed * DeltaSeconds;
		if (StepDistance > (bBoss ? 230.f : 150.f) * Stats.Scale)
		{
			StepDistance = 0.f;
			UEldoriaSettings::PlaySoundRandom(this, UEldoriaSettings::Get()->RunFootstepSounds, GetActorLocation() - FVector(0.f, 0.f, 80.f), bBoss ? 0.9f : 0.35f, bBoss ? 0.02f : 0.08f);
		}
	}

	if (bBoss && Summons < 2 && Health->GetPercent() < (Summons == 0 ? 0.6f : 0.3f) && State == EEnemyState::Chase)
	{
		Summon();
	}
}

void AEnemyCharacter::ApplyDifficulty()
{
	GetCharacterMovement()->MaxWalkSpeed = Stats.WalkSpeed * AEldoriaGameMode::Tuning(this).EnemySpeed;
}

FVector AEnemyCharacter::GetAimPoint() const
{
	if ((IsDown() || IsDead()) && GetMesh()->DoesSocketExist(TEXT("pelvis")))
	{
		return GetMesh()->GetSocketLocation(TEXT("pelvis"));
	}
	return GetActorLocation();
}

void AEnemyCharacter::StartAttack()
{
	State = EEnemyState::Attack;
	const FDifficultyTuning& Tune = AEldoriaGameMode::Tuning(this);
	Cooldown = Stats.AttackCooldown * Tune.AttackCooldown * FMath::FRandRange(0.8f, 1.3f);
	const int32 Index = Attacks.Num() ? FMath::RandRange(0, Attacks.Num() - 1) : INDEX_NONE;
	UAnimSequenceBase* Montage = Attacks.IsValidIndex(Index) ? Attacks[Index] : nullptr;
	bCurrentUnblockable = Stats.UnblockableAttacks.Contains(Index);
	TelegraphTime = GetWorld()->GetTimeSeconds();
	const float Duration = PlayAnim(Montage, (bBoss ? 0.95f : 1.05f) * Tune.AttackRate, 1.1f);
	const UEldoriaSettings* SoundSettings = UEldoriaSettings::Get();
	const bool bHeavySwing = bBoss || Stats.Damage >= 18.f;
	UEldoriaSettings::PlaySoundRandom(this, bHeavySwing ? SoundSettings->HeavySwingSounds : SoundSettings->SwingSounds, GetActorLocation(), bBoss ? 1.2f : 0.75f, 0.06f);
	GetWorldTimerManager().SetTimer(AttackHitTimer, this, &AEnemyCharacter::AttackHit, Duration * 0.48f, false);
	GetWorldTimerManager().SetTimer(AttackEndTimer, [this]()
	{
		if (State == EEnemyState::Attack) State = EEnemyState::Chase;
		ReleaseToken();
	}, Duration, false);
}

void AEnemyCharacter::ReleaseToken()
{
	if (AEldoriaGameMode* GM = GetWorld()->GetAuthGameMode<AEldoriaGameMode>()) GM->ReleaseAttackToken(this);
}

void AEnemyCharacter::AttackHit()
{
	if (State != EEnemyState::Attack) return;
	ACharacter* Player = UGameplayStatics::GetPlayerCharacter(this, 0);
	if (!Player) return;
	const FVector To = Player->GetActorLocation() - GetActorLocation();
	const bool bInFront = FVector::DotProduct(GetActorForwardVector(), To.GetSafeNormal2D()) > 0.25f;
	if (To.Size2D() < Stats.AttackRange * Stats.Scale + 60.f && bInFront && FMath::Abs(To.Z) < 200.f)
	{
		const float Dealt = UGameplayStatics::ApplyDamage(Player, Stats.Damage * AEldoriaGameMode::Tuning(this).EnemyDamage, GetController(), this, nullptr);
		if (Dealt > 0.f)
		{
			if (UParticleSystem* FX = Stats.ImpactEffect.LoadSynchronous())
			{
				UGameplayStatics::SpawnEmitterAtLocation(this, FX, Player->GetActorLocation() + FVector(0.f, 0.f, 40.f) - To.GetSafeNormal2D() * 40.f);
			}
		}
	}
}

float AEnemyCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (IsDead()) return 0.f;
	if (bBoss && !HasKey())
	{
		if (AEldoriaGameMode* GM = GetWorld()->GetAuthGameMode<AEldoriaGameMode>())
		{
			GM->ShowMessage(FText::FromString(TEXT("A dark ward protects Mordrak. Find the Royal Key.")), 2.5f);
		}
		return 0.f;
	}
	Alert();
	const float Dealt = Health->ApplyDamage(DamageAmount);
	if (Health->IsDead())
	{
		Die();
		return Dealt;
	}

	// visible feedback: sparks, blood, a damage number. (No material hit flash: the Paragon materials'
	// HitFlash also drives their opacity, so flashing a body made it vanish for a moment.)
	const UEldoriaSettings* S = UEldoriaSettings::Get();
	if (UParticleSystem* FX = S->HeroImpactEffect.LoadSynchronous())
	{
		UGameplayStatics::SpawnEmitterAtLocation(this, FX, GetActorLocation() + FVector(0.f, 0.f, 50.f * Stats.Scale));
	}
	AEldoriaHUD::AddNumber(this, GetActorLocation() + FVector(0.f, 0.f, 120.f * Stats.Scale), Dealt, FLinearColor(1.f, 0.93f, 0.75f));
	// the blow throws blood onto the ground behind the wound
	if (DamageCauser)
	{
		UEldoriaSettings::SpillBlood(this, GetAimPoint(), GetActorLocation() - DamageCauser->GetActorLocation(), FMath::GetMappedRangeValueClamped(FVector2D(10.f, 55.f), FVector2D(45.f, 110.f), Dealt));
	}

	const float Knock = PendingKnockback;
	const float StanceHit = PendingStance;
	const bool bWantsKnockdown = bPendingKnockdown;
	PendingKnockback = 420.f;
	PendingStance = 10.f;
	bPendingKnockdown = false;
	const FVector Away = DamageCauser ? (GetActorLocation() - DamageCauser->GetActorLocation()).GetSafeNormal2D() : -GetActorForwardVector();

	// on the ground: the body is driven along the dirt and pinned there a little longer
	if (State == EEnemyState::Down)
	{
		if (bLimp) GetMesh()->AddImpulse(Away * FMath::Clamp(Knock * 0.4f, 100.f, 260.f) + FVector(0.f, 0.f, 60.f), TEXT("pelvis"), true);
		else LaunchCharacter(Away * FMath::Clamp(Knock * 0.35f, 80.f, 220.f), true, false);
		const float Now = GetWorld()->GetTimeSeconds();
		const float Remaining = GetWorldTimerManager().GetTimerRemaining(StaggerTimer);
		const float Latest = DownSince + KnockdownLength + 4.f;
		const float Want = FMath::Min(FMath::Max(Remaining, 0.9f), Latest - Now);
		if (Want > Remaining) GetWorldTimerManager().SetTimer(StaggerTimer, this, &AEnemyCharacter::GetUp, Want, false);
		return Dealt;
	}
	// a heavy blow while it struggles up puts it straight back down
	if (State == EEnemyState::GettingUp)
	{
		if (bWantsKnockdown && KnockdownAnims.Num() > 0) Knockdown(Away, Knock);
		return Dealt;
	}
	// dazed: the hit lands, but there's no new reaction to interrupt
	if (State == EEnemyState::Stunned)
	{
		return Dealt;
	}

	// every hit wears down the guard; when it breaks the enemy is dazed and open to an execution
	if (Stats.MaxStance > 0.f)
	{
		Stance += StanceHit;
		LastStanceHitTime = GetWorld()->GetTimeSeconds();
		if (Stance >= Stats.MaxStance)
		{
			Stun();
			return Dealt;
		}
	}

	// Only a big hit throws a body to the ground. How big: the share of its health the blow took plus
	// the force behind it, against how sturdy it is; worn down (hurt, guard cracking) it goes down sooner,
	// and a little luck either way so the same swing doesn't always land the same.
	const float Impact = Dealt / FMath::Max(1.f, Stats.MaxHealth) + Knock / 1200.f;
	float Sturdiness = Stats.bEasilyKnockedDown ? 1.f : 1.5f;
	if (Health->GetPercent() < 0.35f) Sturdiness -= 0.25f;
	if (GetStancePercent() >= 0.6f) Sturdiness -= 0.2f;
	const bool bBigHit = Impact * FMath::FRandRange(0.85f, 1.15f) >= Sturdiness;
	const bool bCanKnockDown = !bBoss && KnockdownAnims.Num() > 0;
	if (bWantsKnockdown && bCanKnockDown && bBigHit)
	{
		Knockdown(Away, Knock);
	}
	else if (!bBoss || Knock >= 700.f)
	{
		Flinch(Away, Knock);
	}
	Cooldown = FMath::Max(Cooldown, 0.7f);
	return Dealt;
}

UAnimSequenceBase* AEnemyCharacter::DirectionalHitAnim(const FVector& FromAttacker) const
{
	// which side the blow came from, in the enemy's own frame
	const FVector Local = GetActorRotation().UnrotateVector(FromAttacker);
	UAnimSequenceBase* Pick = HitAnim;
	if (Local.X < -0.6f && HitBackAnim) Pick = HitBackAnim;
	else if (Local.Y > 0.6f && HitRightAnim) Pick = HitRightAnim;
	else if (Local.Y < -0.6f && HitLeftAnim) Pick = HitLeftAnim;
	return Pick;
}

void AEnemyCharacter::InterruptAttack()
{
	GetWorldTimerManager().ClearTimer(AttackHitTimer);
	GetWorldTimerManager().ClearTimer(AttackEndTimer);
	ReleaseToken();
	if (AAIController* AI = Cast<AAIController>(GetController())) AI->StopMovement();
}

void AEnemyCharacter::Flinch(const FVector& Away, float Knock)
{
	InterruptAttack();
	// a short, weighty shove along the ground (bosses barely budge)
	LaunchCharacter(Away * Knock * (bBoss ? 0.2f : 1.f), true, false);
	State = EEnemyState::Stagger;
	const float Duration = FMath::Max(PlayAnim(DirectionalHitAnim(-Away), 1.2f, 0.4f), 0.35f);
	GetWorldTimerManager().SetTimer(StaggerTimer, [this]()
	{
		if (State == EEnemyState::Stagger) State = EEnemyState::Chase;
	}, Duration, false);
}

void AEnemyCharacter::Knockdown(const FVector& Away, float Knock)
{
	InterruptAttack();
	State = EEnemyState::Down;
	DownSince = GetWorld()->GetTimeSeconds();
	FaceTowards(GetActorLocation() - Away * 100.f, 1.f, 100.f);
	CurrentFall = KnockdownAnims[FMath::RandRange(0, KnockdownAnims.Num() - 1)];
	// thrown by the blow: the body goes limp and physics carries it to the ground, landing
	// differently every time; the capsule stays put and no longer blocks anyone
	DeathPush = Away;
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	GoLimp();
	if (bLimp)
	{
		GetMesh()->SetAllPhysicsLinearVelocity(Away * FMath::Clamp(Knock * 0.75f, 260.f, 480.f) + FVector(0.f, 0.f, 200.f));
		KnockdownLength = 1.f;
	}
	else
	{
		// no ragdoll for this body: the fall clip it is
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		LaunchCharacter(Away * FMath::Min(Knock, 520.f) + FVector(0.f, 0.f, 260.f), true, true);
		KnockdownLength = UEldoriaSettings::PlayAnim(GetMesh(), CurrentFall, FMath::FRandRange(0.9f, 1.1f), Stats.HitSlot, true);
		if (KnockdownLength <= 0.f) KnockdownLength = 0.8f;
	}
	// stay down for a moment, vulnerable
	const float Lie = KnockdownLength + FMath::FRandRange(1.2f, 2.2f);
	GetWorldTimerManager().SetTimer(StaggerTimer, this, &AEnemyCharacter::GetUp, Lie, false);
}

void AEnemyCharacter::GetUp()
{
	if (IsDead()) return;
	State = EEnemyState::GettingUp;
	// push back up slowly: a real rising clip when there is one, otherwise the fall played gently in reverse
	float Duration = 0.f;
	if (bLimp && RecoverFromRagdoll(Duration))
	{
		// rising from wherever the ragdoll came to rest
	}
	else if (GetUpAnim)
	{
		Duration = UEldoriaSettings::PlayAnim(GetMesh(), GetUpAnim, 0.9f, Stats.HitSlot);
	}
	else
	{
		UAnimSequenceBase* Fall = CurrentFall ? CurrentFall : KnockdownAnim;
		const float Length = Fall ? Fall->GetPlayLength() : 1.f;
		const float Rate = Length / FMath::FRandRange(1.5f, 2.0f);
		UEldoriaSettings::PlayAnim(GetMesh(), Fall, Rate, Stats.HitSlot, false, true);
		Duration = Length / Rate;
	}
	GetWorldTimerManager().SetTimer(StaggerTimer, [this]()
	{
		if (State == EEnemyState::GettingUp)
		{
			GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
			if (GetCharacterMovement()->MovementMode == MOVE_None) GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			State = EEnemyState::Chase;
			Cooldown = FMath::Max(Cooldown, 0.8f);
		}
	}, FMath::Max(Duration, 0.6f), false);
}

void AEnemyCharacter::Stun()
{
	InterruptAttack();
	State = EEnemyState::Stunned;
	Stance = Stats.MaxStance;
	UEldoriaSettings::PlayAnim(GetMesh(), StunAnim ? StunAnim : HitAnim, 0.8f, Stats.HitSlot, true);
	if (AEldoriaGameMode* GM = GetWorld()->GetAuthGameMode<AEldoriaGameMode>())
	{
		GM->ShowMessage(FText::FromString(TEXT("Guard broken!  Press E to execute")), 1.5f);
	}
	GetWorldTimerManager().SetTimer(StaggerTimer, [this]()
	{
		if (State != EEnemyState::Stunned) return;
		// recovered: guard restored, back to the fight
		Stance = 0.f;
		State = EEnemyState::Chase;
		PlayAnim(HitAnim, 1.f, 0.3f);
	}, bBoss ? 3.f : 3.5f, false);
}

void AEnemyCharacter::OnParried(AActor* By)
{
	if (IsDead() || State != EEnemyState::Attack) return;
	const FVector Away = (GetActorLocation() - By->GetActorLocation()).GetSafeNormal2D();
	Stance += Stats.MaxStance * 0.45f;
	LastStanceHitTime = GetWorld()->GetTimeSeconds();
	if (Stats.MaxStance > 0.f && Stance >= Stats.MaxStance)
	{
		Stun();
		return;
	}
	// deflected: thrown off balance for longer than a normal flinch
	InterruptAttack();
	LaunchCharacter(Away * (bBoss ? 120.f : 300.f), true, false);
	State = EEnemyState::Stagger;
	const float Duration = FMath::Max(PlayAnim(HitAnim, 0.8f, 0.8f), 0.8f);
	GetWorldTimerManager().SetTimer(StaggerTimer, [this]()
	{
		if (State == EEnemyState::Stagger) State = EEnemyState::Chase;
	}, Duration, false);
	Cooldown = FMath::Max(Cooldown, 1.2f);
}

void AEnemyCharacter::Execute(AActor* By)
{
	if (State != EEnemyState::Stunned) return;
	GetWorldTimerManager().ClearTimer(StaggerTimer);
	if (bBoss)
	{
		// the Bone King survives an execution, but it costs him dearly
		Health->ApplyDamage(Health->MaxHealth * 0.22f);
		Stance = 0.f;
		if (Health->IsDead())
		{
			Die();
			return;
		}
		State = EEnemyState::Chase;
		PlayAnim(HitAnim, 0.8f, 0.6f);
		return;
	}
	Health->ApplyDamage(Health->Health + 1.f);
	Die();
}

void AEnemyCharacter::Die()
{
	const EEnemyState Before = State;
	State = EEnemyState::Dead;
	GetWorldTimerManager().ClearAllTimersForObject(this);
	ReleaseToken();
	if (AAIController* AI = Cast<AAIController>(GetController())) AI->StopMovement();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	UEldoriaSettings::PlaySound(this, UEldoriaSettings::Get()->EnemyDeathSound, GetActorLocation(), bBoss ? 1.5f : 0.9f, 0.12f);
	if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		DeathPush = (GetActorLocation() - Player->GetActorLocation()).GetSafeNormal2D();
	}
	// The clip starts the fall (the stagger, the buckling knees); the ragdoll finishes it, so the
	// body always ends up flat on the ground however the clip itself ends.
	float LimpAt = 0.f;
	if (Before == EEnemyState::Down)
	{
		// already falling or lying there: let the rest of the fall play out, then go limp
		const float SinceDown = GetWorld()->GetTimeSeconds() - DownSince;
		LimpAt = FMath::Max(0.1f, KnockdownLength * Stats.RagdollAt - SinceDown);
	}
	else if (Before == EEnemyState::GettingUp && CurrentFall)
	{
		// killed while rising: buckle back the way it fell
		LimpAt = UEldoriaSettings::PlayAnim(GetMesh(), CurrentFall, 1.f, Stats.HitSlot, true) * Stats.RagdollAt;
	}
	else if (DeathAnims.Num() > 0)
	{
		// each enemy starts to die its own way, at a slightly different pace
		UAnimSequenceBase* Death = DeathAnims[FMath::RandRange(0, DeathAnims.Num() - 1)];
		LimpAt = UEldoriaSettings::PlayAnim(GetMesh(), Death, FMath::FRandRange(0.85f, 1.1f), Stats.HitSlot, true) * Stats.RagdollAt;
	}
	GetWorldTimerManager().SetTimer(StaggerTimer, this, &AEnemyCharacter::GoLimp, FMath::Max(0.05f, LimpAt), false);
	// the killing blow sprays, and once the body has settled a pool spreads under it
	UEldoriaSettings::SpillBlood(this, GetAimPoint(), DeathPush, 120.f * Stats.Scale);
	FTimerHandle PoolTimer;
	GetWorldTimerManager().SetTimer(PoolTimer, [this]()
	{
		UEldoriaSettings::SpillBlood(this, GetAimPoint(), FVector::ZeroVector, 150.f * Stats.Scale * (bBoss ? 1.6f : 1.f), true);
	}, LimpAt + 1.6f, false);
	// melt only once the body has dropped and settled, plus five seconds lying still
	MeltDelay = LimpAt + 2.f + 5.f;
	Placeholder->SetVisibility(false);

	// scatter coins
	for (int32 i = 0; i < Stats.CoinDrop; ++i)
	{
		const FVector Offset(FMath::FRandRange(-120.f, 120.f), FMath::FRandRange(-120.f, 120.f), 40.f);
		GetWorld()->SpawnActor<ACoinPickup>(ACoinPickup::StaticClass(), GetActorLocation() + Offset, FRotator::ZeroRotator);
	}
	if (!bBoss && FMath::FRand() < 0.2f)
	{
		GetWorld()->SpawnActor<AHealthPickup>(AHealthPickup::StaticClass(), GetActorLocation() + FVector(0.f, 0.f, 40.f), FRotator::ZeroRotator);
	}

	if (AEldoriaGameMode* GM = GetWorld()->GetAuthGameMode<AEldoriaGameMode>())
	{
		GM->OnEnemyKilled(this);
	}
	// safety net; normally the body melts away and is removed in UpdateDeath
	SetLifeSpan(MeltDelay + 8.f);
}

void AEnemyCharacter::GoLimp()
{
	if (bLimp) return;
	USkeletalMeshComponent* Body = GetMesh();
	if (UPhysicsAsset* Override = Stats.RagdollPhysics.LoadSynchronous())
	{
		Body->SetPhysicsAsset(Override, true);
	}
	// knocked down, not dead: the weapon stays in the fist; only a corpse lets go of it
	bLimp = UEldoriaSettings::StartRagdoll(Body, !IsDead());
	if (!bLimp) return;
	// carry on with the momentum of the blow
	Body->SetAllPhysicsLinearVelocity(DeathPush * 160.f * Stats.Scale + FVector(0.f, 0.f, -60.f));
}

UAnimSequenceBase* AEnemyCharacter::RecoverFromRagdoll(float& OutDuration)
{
	USkeletalMeshComponent* Body = GetMesh();
	UEldoriaAnimInstance* Anim = Cast<UEldoriaAnimInstance>(Body->GetAnimInstance());
	UAnimSequenceBase* Rise = GetUpAnim ? GetUpAnim : (CurrentFall ? CurrentFall : KnockdownAnim);
	if (!Anim || !Rise) return nullptr;
	const bool bReverse = GetUpAnim ? Stats.bGetUpReverse : true;

	// where every bone of the body lies right now
	const int32 NumBones = Body->GetNumBones();
	TArray<FTransform> World;
	World.SetNum(NumBones);
	for (int32 i = 0; i < NumBones; ++i) World[i] = Body->GetBoneTransform(i);
	const FVector Pelvis = Body->GetSocketLocation(TEXT("pelvis"));
	const FVector HeadDir = (Body->GetSocketLocation(TEXT("head")) - Pelvis).GetSafeNormal2D();

	// back under animation, attached to the capsule as usual
	bLimp = false;
	Body->SetSimulatePhysics(false);
	Body->SetAllBodiesSimulatePhysics(false);
	Body->bBlendPhysics = false;
	Body->SetCollisionProfileName(TEXT("CharacterMesh"));
	// back from a ragdoll: the body must never stop the camera again (the profile resets that)
	Body->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	// bring back any bodies removed while it was down (the weapon), ready for the next fall
	Body->RecreatePhysicsState();
	Body->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	Body->SetRelativeLocationAndRotation(FVector(0.f, 0.f, Stats.MeshZ), FRotator(0.f, Stats.MeshYaw, 0.f));

	// it pushes itself up where it lies, facing the way its head points
	const float Yaw = HeadDir.IsNearlyZero() ? GetActorRotation().Yaw : HeadDir.Rotation().Yaw;
	SetActorLocationAndRotation(FVector(Pelvis.X, Pelvis.Y, GetActorLocation().Z), FRotator(0.f, Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	if (AController* C = GetController()) C->SetControlRotation(GetActorRotation());

	// the ragdoll's pose in the new component space, as parent-relative transforms
	const FTransform Component = Body->GetComponentTransform();
	const FReferenceSkeleton& Ref = Body->GetSkeletalMeshAsset()->GetRefSkeleton();
	TArray<FTransform> CS, Local;
	CS.SetNum(NumBones);
	Local.SetNum(NumBones);
	for (int32 i = 0; i < NumBones; ++i)
	{
		const int32 Parent = Ref.GetParentIndex(i);
		// the root stays where the animation keeps it
		CS[i] = Parent == INDEX_NONE ? Ref.GetRefBonePose()[i] : World[i].GetRelativeTransform(Component);
		Local[i] = Parent == INDEX_NONE ? CS[i] : CS[i].GetRelativeTransform(CS[Parent]);
		Local[i].NormalizeRotation();
	}

	// the limp pose fades into the low first frame of the rise (sitting up, onto a knee), then it stands
	const float From = GetUpAnim ? Stats.GetUpFrom : -1.f;
	const float Span = From >= 0.f ? (bReverse ? From : Rise->GetPlayLength() - From) : Rise->GetPlayLength();
	const float Rate = Span / FMath::FRandRange(1.5f, 1.9f);
	OutDuration = Anim->PlayAction(Rise, Rate, false, bReverse, From);
	Anim->BlendFromPose(Local, 0.8f);
	return Rise;
}

void AEnemyCharacter::SetMaterialScalar(FName Param, float Value)
{
	for (UMaterialInstanceDynamic* M : DynamicMaterials)
	{
		if (M) M->SetScalarParameterValue(Param, Value);
	}
}

void AEnemyCharacter::UpdateDeath(float DeltaSeconds)
{
	// lie still for a moment, then evaporate: Paragon's own fade/ash material plus drifting smoke
	const float Linger = MeltDelay;
	constexpr float Dissolve = 2.6f;
	DeathTime += DeltaSeconds;
	if (DeathTime < Linger) return;

	if (!bDissolving)
	{
		bDissolving = true;
		if (bLimp)
		{
			// the settled body sinks as one piece: no gravity, no ground under it, a slow steady drift down
			GetMesh()->SetEnableGravity(false);
			GetMesh()->SetCollisionResponseToAllChannels(ECR_Ignore);
		}
		else
		{
			GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		MeltStartZ = GetMesh()->GetRelativeLocation().Z;
	}
	// melt into the ground, fading out only at the very end
	const float T = FMath::Clamp((DeathTime - Linger) / Dissolve, 0.f, 1.f);
	const float Ease = T * T * (3.f - 2.f * T);
	const float Depth = (bLimp ? 90.f : 230.f) * Stats.Scale * (bBoss ? 1.6f : 1.f);
	if (bLimp)
	{
		// speed of the eased sink at this moment
		const float Speed = Depth * 6.f * T * (1.f - T) / Dissolve;
		GetMesh()->SetAllPhysicsLinearVelocity(FVector(0.f, 0.f, -Speed));
		GetMesh()->SetAllPhysicsAngularVelocityInRadians(FVector::ZeroVector);
	}
	else
	{
		FVector Rel = GetMesh()->GetRelativeLocation();
		Rel.Z = MeltStartZ - Depth * Ease;
		GetMesh()->SetRelativeLocation(Rel);
		const FVector Base = GetMesh()->GetRelativeScale3D();
		GetMesh()->SetRelativeScale3D(FVector(Base.X, Base.Y, FMath::Max(0.2f, 1.f - 0.6f * Ease)));
	}
	SetMaterialScalar(TEXT("FadeOut"), FMath::Clamp((T - 0.7f) / 0.3f, 0.f, 1.f));
	if (T >= 1.f) Destroy();
}

void AEnemyCharacter::Summon()
{
	++Summons;
	for (int32 i = 0; i < 3; ++i)
	{
		const float Angle = (i - 1) * 1.2f + PI;
		const FVector Pos = GetActorLocation() + GetActorRotation().RotateVector(FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * 500.f);
		AEnemyCharacter* Minion = GetWorld()->SpawnActorDeferred<AEnemyCharacter>(AEnemyCharacter::StaticClass(), FTransform(GetActorRotation(), Pos), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (Minion)
		{
			Minion->Archetype = TEXT("Grunt");
			Minion->Group = Group;
			Minion->bDormant = false;
			UGameplayStatics::FinishSpawningActor(Minion, FTransform(GetActorRotation(), Pos));
			Minion->Alert();
		}
	}
}
