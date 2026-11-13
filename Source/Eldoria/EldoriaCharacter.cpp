#include "EldoriaCharacter.h"
#include "EldoriaAnimInstance.h"
#include "EldoriaSettings.h"
#include "EldoriaGameMode.h"
#include "EnemyCharacter.h"
#include "HealthComponent.h"
#include "Chest.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Kismet/GameplayStatics.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/DamageEvents.h"
#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "TimerManager.h"
#include "EldoriaHUD.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/App.h"
#include "RHI.h"
#include "RenderCore.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Containers/Ticker.h"

AEldoriaCharacter::AEldoriaCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.f, 720.f, 0.f);
	Move->MaxWalkSpeed = WalkSpeed;
	Move->JumpZVelocity = 720.f;
	Move->AirControl = 0.45f;
	Move->GravityScale = 1.6f;
	Move->BrakingDecelerationWalking = 2400.f;
	Move->MaxAcceleration = 2600.f;
	JumpMaxCount = 2;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = TargetArmLength;
	CameraBoom->SocketOffset = FVector(0.f, 55.f, 70.f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 12.f;
	CameraBoom->bDoCollisionTest = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	FollowCamera->SetFieldOfView(75.f);

	Health = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Capsule(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	Placeholder = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Placeholder"));
	Placeholder->SetupAttachment(RootComponent);
	Placeholder->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Placeholder->SetRelativeScale3D(FVector(0.8f, 0.8f, 1.9f));
	if (Capsule.Succeeded()) Placeholder->SetStaticMesh(Capsule.Object);
	Health->MaxHealth = 150.f;

	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -96.f), FRotator(0.f, -90.f, 0.f));
}

void AEldoriaCharacter::BeginPlay()
{
	Super::BeginPlay();
	LoadArt();
	Health->MaxHealth = AEldoriaGameMode::Tuning(this).HeroHealth;
	Health->Reset(Health->MaxHealth);
#if !UE_BUILD_SHIPPING
	// development: launching with -eldoriacombattest runs the scripted fight a few seconds in
	if (FParse::Param(FCommandLine::Get(), TEXT("eldoriacombattest")))
	{
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, this, &AEldoriaCharacter::EldoriaCombatTest, 6.f, false);
	}
#endif
}

void AEldoriaCharacter::LoadArt()
{
	const UEldoriaSettings* S = UEldoriaSettings::Get();
	if (USkeletalMesh* Mesh = S->HeroMesh.LoadSynchronous())
	{
		Placeholder->SetVisibility(false);
		GetMesh()->SetSkeletalMesh(Mesh);
		GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, S->HeroMeshZ), FRotator(0.f, S->HeroMeshYaw, 0.f));
		DynamicMaterials.Reset();
		for (int32 i = 0; i < GetMesh()->GetNumMaterials(); ++i)
		{
			if (UMaterialInstanceDynamic* M = GetMesh()->CreateAndSetMaterialInstanceDynamic(i)) DynamicMaterials.Add(M);
		}
	}
	if (UAnimSequenceBase* Walk = S->HeroWalkAnim.LoadSynchronous())
	{
		// animated from code: real walk and run cycles blended by speed
		GetMesh()->SetAnimInstanceClass(UEldoriaAnimInstance::StaticClass());
		if (UEldoriaAnimInstance* Native = Cast<UEldoriaAnimInstance>(GetMesh()->GetAnimInstance()))
		{
			Native->Setup(S->HeroIdleAnim.LoadSynchronous(), S->HeroRunAnim.LoadSynchronous(), S->HeroRunAnimSpeed);
			Native->SetupWalkAndFall(Walk, S->HeroWalkAnimSpeed, S->HeroFallAnim.LoadSynchronous());
		}
		JumpAnim = S->HeroJumpAnim.LoadSynchronous();
		LandAnim = S->HeroLandAnim.LoadSynchronous();
	}
	else if (UClass* Anim = S->HeroAnimClass.LoadSynchronous())
	{
		GetMesh()->SetAnimInstanceClass(Anim);
	}
	MoveAnims.Reset();
	for (const FHeroMove& M : S->HeroMoves)
	{
		MoveAnims.Add(M.Name, M.Anim.LoadSynchronous());
	}
	DodgeAnim = S->HeroDodgeAnim.LoadSynchronous();
	HitAnim = S->HeroHitAnim.LoadSynchronous();
	DeathAnim = S->HeroDeathAnim.LoadSynchronous();
	ExecutionAnim = S->HeroExecutionAnim.LoadSynchronous();
	BlockAnim = S->HeroBlockAnim.LoadSynchronous();
}

// ---------- input ----------

void AEldoriaCharacter::BuildInput()
{
	if (Mapping) return;

	auto MakeAction = [this](EInputActionValueType Type)
	{
		UInputAction* A = NewObject<UInputAction>(this);
		A->ValueType = Type;
		return A;
	};
	MoveAction = MakeAction(EInputActionValueType::Axis2D);
	LookAction = MakeAction(EInputActionValueType::Axis2D);
	JumpAction = MakeAction(EInputActionValueType::Boolean);
	RunAction = MakeAction(EInputActionValueType::Boolean);
	LightAction = MakeAction(EInputActionValueType::Boolean);
	HeavyAction = MakeAction(EInputActionValueType::Boolean);
	BlockAction = MakeAction(EInputActionValueType::Boolean);
	DodgeAction = MakeAction(EInputActionValueType::Boolean);
	InteractAction = MakeAction(EInputActionValueType::Boolean);
	HelpAction = MakeAction(EInputActionValueType::Boolean);
	PauseAction = MakeAction(EInputActionValueType::Boolean);

	Mapping = NewObject<UInputMappingContext>(this);

	// WASD and the arrow keys pack into one 2D axis: up/down drive Y via a swizzle, left/down are negated
	auto MapMove = [this](const FKey& Key, bool bSwizzle, bool bNegate)
	{
		FEnhancedActionKeyMapping& M = Mapping->MapKey(MoveAction, Key);
		if (bSwizzle) M.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(this));
		if (bNegate) M.Modifiers.Add(NewObject<UInputModifierNegate>(this));
	};
	MapMove(EKeys::W, true, false);
	MapMove(EKeys::S, true, true);
	MapMove(EKeys::D, false, false);
	MapMove(EKeys::A, false, true);
	MapMove(EKeys::Up, true, false);
	MapMove(EKeys::Down, true, true);
	MapMove(EKeys::Right, false, false);
	MapMove(EKeys::Left, false, true);
	MapMove(EKeys::Gamepad_Left2D, false, false);

	Mapping->MapKey(LookAction, EKeys::Mouse2D);
	Mapping->MapKey(LookAction, EKeys::Gamepad_Right2D);
	Mapping->MapKey(JumpAction, EKeys::SpaceBar);
	Mapping->MapKey(JumpAction, EKeys::Gamepad_FaceButton_Bottom);
	Mapping->MapKey(RunAction, EKeys::LeftShift);
	Mapping->MapKey(RunAction, EKeys::RightShift);
	Mapping->MapKey(RunAction, EKeys::Gamepad_LeftThumbstick);
	// combat keys for both hands: J K L (+F block) next to WASD, Z X C (+V block) next to the arrows
	for (const FKey& K : { EKeys::J, EKeys::Z, EKeys::LeftMouseButton, EKeys::Gamepad_FaceButton_Left }) Mapping->MapKey(LightAction, K);
	for (const FKey& K : { EKeys::K, EKeys::X, EKeys::Gamepad_RightTrigger }) Mapping->MapKey(HeavyAction, K);
	for (const FKey& K : { EKeys::F, EKeys::V, EKeys::RightMouseButton, EKeys::Gamepad_LeftTrigger }) Mapping->MapKey(BlockAction, K);
	// dodge: C / L next to the attack keys, plus Q and Left Alt
	for (const FKey& K : { EKeys::C, EKeys::L, EKeys::Q, EKeys::LeftAlt, EKeys::Gamepad_RightShoulder }) Mapping->MapKey(DodgeAction, K);
	Mapping->MapKey(DodgeAction, EKeys::Gamepad_FaceButton_Right);
	Mapping->MapKey(InteractAction, EKeys::E);
	Mapping->MapKey(HelpAction, EKeys::H);
	Mapping->MapKey(PauseAction, EKeys::Escape);
	Mapping->MapKey(PauseAction, EKeys::P);
	Mapping->MapKey(PauseAction, EKeys::Gamepad_Special_Right);
	Mapping->MapKey(HelpAction, EKeys::Gamepad_Special_Left);
	Mapping->MapKey(InteractAction, EKeys::Gamepad_FaceButton_Top);
}

void AEldoriaCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();
	BuildInput();
	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (auto* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Sub->AddMappingContext(Mapping, 0);
		}
	}
}

void AEldoriaCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	BuildInput();
	if (UEnhancedInputComponent* In = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		In->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AEldoriaCharacter::Move);
		In->BindAction(LookAction, ETriggerEvent::Triggered, this, &AEldoriaCharacter::Look);
		In->BindAction(JumpAction, ETriggerEvent::Started, this, &AEldoriaCharacter::JumpPressed);
		In->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		In->BindAction(RunAction, ETriggerEvent::Started, this, &AEldoriaCharacter::StartRun);
		In->BindAction(RunAction, ETriggerEvent::Completed, this, &AEldoriaCharacter::StopRun);
		In->BindAction(LightAction, ETriggerEvent::Started, this, &AEldoriaCharacter::LightAttack);
		In->BindAction(HeavyAction, ETriggerEvent::Started, this, &AEldoriaCharacter::HeavyAttack);
		In->BindAction(BlockAction, ETriggerEvent::Started, this, &AEldoriaCharacter::StartBlock);
		In->BindAction(BlockAction, ETriggerEvent::Completed, this, &AEldoriaCharacter::StopBlock);
		In->BindAction(DodgeAction, ETriggerEvent::Started, this, &AEldoriaCharacter::Dodge);
		In->BindAction(InteractAction, ETriggerEvent::Started, this, &AEldoriaCharacter::Interact);
		In->BindAction(HelpAction, ETriggerEvent::Started, this, &AEldoriaCharacter::ToggleHelp);
		In->BindAction(PauseAction, ETriggerEvent::Started, this, &AEldoriaCharacter::OpenPauseMenu);
	}
}

void AEldoriaCharacter::Move(const FInputActionValue& Value)
{
	if (IsDead() || bKnockedDown || bDodging) return;
	const FVector2D Axis = Value.Get<FVector2D>();
	const FRotator Yaw(0.f, Controller ? Controller->GetControlRotation().Yaw : 0.f, 0.f);
	const FVector Dir = FRotationMatrix(Yaw).GetUnitAxis(EAxis::X) * Axis.Y + FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y) * Axis.X;
	if (!Dir.IsNearlyZero())
	{
		LastInputDir = Dir.GetSafeNormal();
		LastInputTime = GetWorld()->GetTimeSeconds();
	}
	AddMovementInput(Dir, bAttacking ? 0.15f : 1.f);
}

void AEldoriaCharacter::ResetCamera()
{
	if (Controller) Controller->SetControlRotation(FRotator(-18.f, GetActorRotation().Yaw, 0.f));
	// when the window takes the mouse, the cursor's jump to the centre arrives as one huge movement
	// that would tip the camera straight down; ignore the mouse for a moment
	IgnoreLookUntil = FPlatformTime::Seconds() + 0.5;
	bSkipNextLook = true;
}

void AEldoriaCharacter::Look(const FInputActionValue& Value)
{
	if (FPlatformTime::Seconds() < IgnoreLookUntil) return;
	const FVector2D Axis = Value.Get<FVector2D>();
	// the capture jump can come later than the guard (the window may only take the mouse once the
	// loading screen is gone): drop the first movement after a reset, and any impossible leap
	if (bSkipNextLook || Axis.GetAbsMax() > 120.f)
	{
		bSkipNextLook = false;
		return;
	}
	AddControllerYawInput(Axis.X * 0.5f);
	AddControllerPitchInput(-Axis.Y * 0.5f);
}

void AEldoriaCharacter::JumpPressed()
{
	if (IsDead() || bKnockedDown || bAttacking || bDodging) return;
	if (JumpCurrentCount < JumpMaxCount)
	{
		if (JumpAnim && !bAttacking) UEldoriaSettings::PlayAnim(GetMesh(), JumpAnim, 1.3f, UEldoriaSettings::Get()->HeroAnimSlot);
		UEldoriaSettings::PlaySoundRandom(this, UEldoriaSettings::Get()->JumpSounds, GetActorLocation() - FVector(0.f, 0.f, 80.f), 0.8f, 0.06f);
	}
	Jump();
}

// ---------- combat ----------

bool AEldoriaCharacter::IsDead() const
{
	return Health && Health->IsDead();
}

float AEldoriaCharacter::PlayOrDefault(UAnimSequenceBase* Anim, float Rate, float DefaultLength)
{
	const float Played = UEldoriaSettings::PlayAnim(GetMesh(), Anim, Rate, UEldoriaSettings::Get()->HeroAnimSlot);
	return Played > 0.f ? Played : DefaultLength;
}

void AEldoriaCharacter::OnAttackInput(EAttackInput Input)
{
	if (IsDead() || bKnockedDown || bDodging || bExecuting) return;
	if (!bAttacking)
	{
		if (bBlocking) StopBlock();
		StartMove(ChooseMove(Input, NAME_None));
	}
	else if (GetWorld()->GetTimeSeconds() >= ComboWindowStart)
	{
		bQueued = true;
		QueuedInput = Input;
	}
}

FName AEldoriaCharacter::ChooseMove(EAttackInput Input, FName From) const
{
	static const FName Light1("Light1"), Light2("Light2"), Light3("Light3");
	static const FName Heavy1("Heavy1"), Heavy2("Heavy2"), Finisher("Finisher"), AirSlam("AirSlam");

	if (!GetCharacterMovement()->IsMovingOnGround()) return AirSlam;
	if (Input == EAttackInput::Light)
	{
		if (From == Light1) return Light2;
		if (From == Light2) return Light3;
		return Light1;
	}
	// heavy: after two light hits it becomes the finisher, after a heavy it chains into a second cleave
	if (From == Light2 || From == Light3) return Finisher;
	if (From == Heavy1) return Heavy2;
	return Heavy1;
}

bool AEldoriaCharacter::StartMove(FName MoveName)
{
	const FHeroMove* Def = UEldoriaSettings::Get()->FindMove(MoveName);
	if (!Def) return false;
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < MoveReadyAt.FindRef(MoveName)) return false;
	if (Def->Cooldown > 0.f) MoveReadyAt.Add(MoveName, Now + Def->Cooldown);

	CurrentMove = *Def;
	bAttacking = true;
	bQueued = false;
	FaceAttackTarget();

	UAnimSequenceBase* Anim = MoveAnims.FindRef(MoveName);
	const float Duration = PlayOrDefault(Anim, Def->Rate, 0.7f);
	if (Def->bWindUpGlow)
	{
		// the blade gathers light as he draws back for a full-force swing
		if (UParticleSystem* FX = UEldoriaSettings::Get()->WindUpEffect.LoadSynchronous())
		{
			UGameplayStatics::SpawnEmitterAttached(FX, GetMesh(), FName("hand_r"), FVector::ZeroVector, FRotator::ZeroRotator, FVector(0.6f), EAttachLocation::SnapToTarget, true);
		}
		FovPunch = -3.f;
	}

	bAirSlam = MoveName == FName("AirSlam");
	if (bAirSlam)
	{
		// dive to the ground; the hit lands in Landed()
		LaunchCharacter(FVector(0.f, 0.f, -1800.f), false, true);
	}
	else
	{
		GetWorldTimerManager().SetTimer(HitTimer, this, &AEldoriaCharacter::MoveHit, FMath::Max(0.05f, Duration * Def->HitTime), false);
	}
	ComboWindowStart = Now + Duration * Def->HitTime * 0.7f;
	GetWorldTimerManager().SetTimer(EndTimer, this, &AEldoriaCharacter::MoveEnd, FMath::Max(0.1f, Duration * Def->EndTime), false);

	const UEldoriaSettings* S = UEldoriaSettings::Get();
	const bool bHeavyMove = Def->Damage > 30.f;
	UEldoriaSettings::PlaySoundRandom(this, bHeavyMove ? S->HeavySwingSounds : S->SwingSounds, GetActorLocation(), bHeavyMove ? 1.f : 0.85f, 0.05f);
	if (FMath::FRand() < (Def->Damage > 30.f ? 0.8f : 0.4f)) UEldoriaSettings::PlaySound(this, S->HeroAttackVoice, GetActorLocation(), 0.7f, 0.05f);
	return true;
}

void AEldoriaCharacter::FaceAttackTarget()
{
	// the hero swings where he is facing, or where the player is steering;
	// an enemy close to that direction pulls the swing onto it
	FVector Want = GetActorForwardVector();
	if (GetWorld()->GetTimeSeconds() - LastInputTime < 0.25f && !LastInputDir.IsNearlyZero()) Want = LastInputDir;

	AEnemyCharacter* Best = nullptr;
	float BestScore = 0.f;
	for (TActorIterator<AEnemyCharacter> It(GetWorld()); It; ++It)
	{
		if (It->IsDead()) continue;
		const FVector To = It->GetAimPoint() - GetActorLocation();
		const float Dist = To.Size2D();
		if (Dist > AssistRange || FMath::Abs(To.Z) > 250.f) continue;
		const float Dot = FVector::DotProduct(Want.GetSafeNormal2D(), To.GetSafeNormal2D());
		if (Dot < 0.35f) continue;
		const float Score = Dot * 2.f - Dist / AssistRange;
		if (!Best || Score > BestScore)
		{
			Best = *It;
			BestScore = Score;
		}
	}
	if (Best) Want = Best->GetActorLocation() - GetActorLocation();
	SetActorRotation(FRotator(0.f, Want.Rotation().Yaw, 0.f));
}

void AEldoriaCharacter::MoveHit()
{
	if (IsDead() || !bAttacking) return;
	DealDamage(CurrentMove, GetActorLocation(), CurrentMove.bAllAround);
}

void AEldoriaCharacter::DealDamage(const FHeroMove& Def, const FVector& Origin, bool bAllAround)
{
	TArray<AEnemyCharacter*> Struck;
	for (TActorIterator<AEnemyCharacter> It(GetWorld()); It; ++It)
	{
		AEnemyCharacter* Enemy = *It;
		if (Enemy->IsDead()) continue;
		// a fallen enemy is struck where its body lies, not where its capsule stands
		const FVector To = Enemy->GetAimPoint() - Origin;
		if (FMath::Abs(To.Z) > 220.f) continue;
		const float Reach = Def.Reach + Def.Radius * 0.5f;
		if (To.Size2D() > Reach) continue;
		if (!bAllAround && FVector::DotProduct(GetActorForwardVector(), To.GetSafeNormal2D()) < 0.2f && To.Size2D() > 120.f) continue;
		Struck.Add(Enemy);
	}

	// the enemy the blade meets first takes the full blow; anyone else caught in the arc only reels
	Struck.Sort([&Origin](const AEnemyCharacter& A, const AEnemyCharacter& B)
	{
		return FVector::DistSquared(A.GetAimPoint(), Origin) < FVector::DistSquared(B.GetAimPoint(), Origin);
	});
	for (int32 i = 0; i < Struck.Num(); ++i)
	{
		AEnemyCharacter* Enemy = Struck[i];
		const bool bPrimary = i == 0;
		Enemy->PendingKnockback = Def.Knockback * (bPrimary ? 1.f : 0.6f);
		Enemy->PendingStance = Def.StanceDamage * (bPrimary ? 1.f : 0.5f);
		Enemy->bPendingKnockdown = Def.bKnockdown && bPrimary;
		UGameplayStatics::ApplyDamage(Enemy, Def.Damage, Controller, this, nullptr);
		UEldoriaSettings::PlayImpact(this, Enemy->GetActorLocation(), Def.Damage > 30.f ? 1.f : 0.8f);
	}
	if (Struck.Num() > 0)
	{
		const float Now = GetWorld()->GetTimeSeconds();
		HitCombo = (Now - LastHitTime < 2.5f) ? HitCombo + Struck.Num() : Struck.Num();
		LastHitTime = Now;
		ImpactFeedback(FMath::GetMappedRangeValueClamped(FVector2D(18.f, 62.f), FVector2D(0.35f, 1.f), Def.Damage) + 0.1f * (Struck.Num() - 1));
		if (!Def.Callout.IsEmpty())
		{
			Callout = Def.Callout;
			CalloutUntil = Now + 1.6f;
		}
	}
}

void AEldoriaCharacter::MoveEnd()
{
	if (bQueued && !IsDead())
	{
		const FName Next = ChooseMove(QueuedInput, CurrentMove.Name);
		if (StartMove(Next)) return;
	}
	bAttacking = false;
	bQueued = false;
	CurrentMove = FHeroMove();
}

void AEldoriaCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	// heavier landings sound heavier
	const float FallSpeed = FMath::Abs(GetCharacterMovement()->Velocity.Z);
	UEldoriaSettings::PlaySoundRandom(this, UEldoriaSettings::Get()->LandSounds, GetActorLocation() - FVector(0.f, 0.f, 90.f), FMath::GetMappedRangeValueClamped(FVector2D(300.f, 1400.f), FVector2D(0.5f, 1.2f), FallSpeed), 0.05f);
	// absorb the landing, unless an attack or a knockdown already owns the body
	if (LandAnim && !bAttacking && !bAirSlam && !bKnockedDown && !bDodging) UEldoriaSettings::PlayAnim(GetMesh(), LandAnim, 1.4f, UEldoriaSettings::Get()->HeroAnimSlot);
	if (bAirSlam)
	{
		bAirSlam = false;
		DealDamage(CurrentMove, GetActorLocation(), true);
		if (UParticleSystem* FX = UEldoriaSettings::Get()->GroundSmashEffect.LoadSynchronous())
		{
			UGameplayStatics::SpawnEmitterAtLocation(this, FX, GetActorLocation() - FVector(0.f, 0.f, 90.f));
		}
		UEldoriaSettings::PlaySound(this, UEldoriaSettings::Get()->EnemyDeathSound, GetActorLocation(), 1.f, 0.1f);
	}
}

void AEldoriaCharacter::ImpactFeedback(float Strength)
{
	Strength = FMath::Clamp(Strength, 0.f, 1.2f);

	// hit-stop: the world almost freezes on contact (80 ms for a jab, ~200 ms for a finisher).
	// Timed in real seconds on the core ticker so the time dilation itself can't stretch it.
	const double Now = FPlatformTime::Seconds();
	const bool bAlreadyFrozen = Now < HitStopUntil;
	HitStopUntil = FMath::Max(HitStopUntil, Now + 0.08 + 0.12 * Strength);
	UGameplayStatics::SetGlobalTimeDilation(this, 0.02f);
	if (!bAlreadyFrozen)
	{
		TWeakObjectPtr<AEldoriaCharacter> Self(this);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self](float)
		{
			if (!Self.IsValid()) return false;
			if (FPlatformTime::Seconds() < Self->HitStopUntil) return true;
			UGameplayStatics::SetGlobalTimeDilation(Self.Get(), 1.f);
			return false;
		}));
	}

	// camera: a jolt in the direction of the swing, a punch toward the impact, a quick zoom and lens pulse
	ShakeDuration = 0.18f + 0.22f * Strength;
	ShakeTime = ShakeDuration;
	ShakeStrength = 5.f + 18.f * Strength;
	const FVector CamRight = FollowCamera->GetRightVector();
	const float Side = FVector::DotProduct(GetActorForwardVector(), CamRight) >= 0.f ? 1.f : -1.f;
	ShakeDirection = FVector2D(Side, -0.4f);
	CameraPunch = 20.f + 45.f * Strength;
	FovPunch = 2.f + 6.f * Strength;
	FringePulse = Strength > 0.6f ? 3.f * Strength : 0.f;

	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		PC->PlayDynamicForceFeedback(0.3f + 0.6f * Strength, 0.1f + 0.15f * Strength, true, true, true, true);
	}
}

void AEldoriaCharacter::StartBlock()
{
	if (IsDead() || bKnockedDown || bDodging) return;
	if (bExecuting) return;
	bBlocking = true;
	BlockStartTime = GetWorld()->GetTimeSeconds();
	bAttacking = false;
	if (!BlockGlow)
	{
		if (UParticleSystem* FX = UEldoriaSettings::Get()->BlockEffect.LoadSynchronous())
		{
			BlockGlow = UGameplayStatics::SpawnEmitterAttached(FX, GetRootComponent(), NAME_None, FVector(60.f, 0.f, 20.f), FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true);
		}
	}
	UAnimInstance* Anim = GetMesh()->GetAnimInstance();
	if (UEldoriaAnimInstance* Native = Cast<UEldoriaAnimInstance>(Anim); Native && BlockAnim)
	{
		Native->PlayAction(BlockAnim, 1.2f, true);
		// guard up with the arms, still walking on the legs
		Native->SetActionUpperBodyOnly(true);
		// freeze the deflect motion with the guard raised until the button is released
		GetWorldTimerManager().SetTimer(BlockHoldTimer, [this]()
		{
			if (UEldoriaAnimInstance* A = Cast<UEldoriaAnimInstance>(GetMesh()->GetAnimInstance()); A && bBlocking) A->SetActionRate(0.f);
		}, 0.22f, false);
	}
	else if (BlockAnim && Anim)
	{
		BlockMontage = Anim->PlaySlotAnimationAsDynamicMontage(BlockAnim, UEldoriaSettings::Get()->HeroAnimSlot, 0.12f, 0.2f, 1.2f);
		// freeze the deflect motion with the shield raised until the button is released
		GetWorldTimerManager().SetTimer(BlockHoldTimer, [this]()
		{
			UAnimInstance* A = GetMesh()->GetAnimInstance();
			if (bBlocking && BlockMontage && A) A->Montage_Pause(BlockMontage);
		}, 0.22f, false);
	}
}

void AEldoriaCharacter::StopBlock()
{
	bBlocking = false;
	if (BlockGlow)
	{
		BlockGlow->DeactivateSystem();
		BlockGlow = nullptr;
	}
	GetWorldTimerManager().ClearTimer(BlockHoldTimer);
	UAnimInstance* Anim = GetMesh()->GetAnimInstance();
	if (BlockMontage && Anim) Anim->Montage_Stop(0.2f, BlockMontage);
	if (UEldoriaAnimInstance* Native = Cast<UEldoriaAnimInstance>(Anim)) Native->StopAction();
	BlockMontage = nullptr;
}

void AEldoriaCharacter::Dodge()
{
	if (IsDead() || bKnockedDown || bDodging || bExecuting || !GetCharacterMovement()->IsMovingOnGround()) return;
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - DodgeStartTime < 0.55f) return;
	bDodging = true;
	bAttacking = false;
	bQueued = false;
	bAirSlam = false;
	bPerfectDodgeUsed = false;
	DodgeStartTime = Now;
	if (bBlocking) StopBlock();
	GetWorldTimerManager().ClearTimer(HitTimer);
	GetWorldTimerManager().ClearTimer(EndTimer);

	// dash the way the player is steering; with no direction held, step back out of danger
	const bool bSteering = Now - LastInputTime < 0.2f && !LastInputDir.IsNearlyZero();
	const FVector Dir = bSteering ? LastInputDir.GetSafeNormal2D() : -GetActorForwardVector();
	if (bSteering) SetActorRotation(FRotator(0.f, Dir.Rotation().Yaw, 0.f));
	LaunchCharacter(Dir * (bSteering ? 1500.f : 1100.f) + FVector(0.f, 0.f, 60.f), true, false);
	// a sharp duck out of the way, timed to the dash
	PlayOrDefault(DodgeAnim, 1.7f, 0.f);

	if (UParticleSystem* FX = UEldoriaSettings::Get()->DodgeTrailEffect.LoadSynchronous())
	{
		DodgeTrail = UGameplayStatics::SpawnEmitterAttached(FX, GetRootComponent(), NAME_None, FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true);
	}
	UEldoriaSettings::PlaySoundRandom(this, UEldoriaSettings::Get()->SwingSounds, GetActorLocation(), 0.6f, 0.1f);
	FovPunch = -5.f;

	const float Duration = 0.42f;
	InvulnerableUntil = Now + 0.32f;
	GetWorldTimerManager().SetTimer(DodgeTimer, [this]()
	{
		bDodging = false;
		if (DodgeTrail)
		{
			DodgeTrail->DeactivateSystem();
			DodgeTrail = nullptr;
		}
	}, Duration, false);
}

void AEldoriaCharacter::EldoriaCombatTest()
{
#if !UE_BUILD_SHIPPING
	AEnemyCharacter* Best = nullptr;
	float BestDist = TNumericLimits<float>::Max();
	for (TActorIterator<AEnemyCharacter> It(GetWorld()); It; ++It)
	{
		if (It->IsDead() || It->bBoss) continue;
		const float D = FVector::Dist(It->GetActorLocation(), GetActorLocation());
		if (D < BestDist)
		{
			BestDist = D;
			Best = *It;
		}
	}
	if (!Best) return;
	// stand three and a half metres in front of it, facing it, camera behind the hero
	const FVector EnemyLoc = Best->GetActorLocation();
	const FVector Dir = Best->GetActorForwardVector().GetSafeNormal2D();
	SetActorLocation(EnemyLoc + Dir * 350.f + FVector(0.f, 0.f, 30.f), false, nullptr, ETeleportType::TeleportPhysics);
	SetActorRotation(FRotator(0.f, (-Dir).Rotation().Yaw, 0.f));
	if (Controller) Controller->SetControlRotation(FRotator(-12.f, (-Dir).Rotation().Yaw, 0.f));
	Best->Alert();
	CombatTestTarget = Best;
	CombatTestTime = 0.f;
	CombatTestStep = 0;
	UE_LOG(LogTemp, Display, TEXT("COMBATTEST start vs %s"), *Best->GetName());
	// keep the screen clear for the captures
	ToggleHelp();
	if (FParse::Param(FCommandLine::Get(), TEXT("eldoriasidecam")))
	{
		CombatTestCamera = GetWorld()->SpawnActor<ACameraActor>();
		if (APlayerController* PC = Cast<APlayerController>(Controller)) PC->SetViewTarget(CombatTestCamera.Get());
	}
#endif
}

void AEldoriaCharacter::TickCombatTest(float DeltaSeconds)
{
#if !UE_BUILD_SHIPPING
	if (CombatTestTime < 0.f) return;
	CombatTestTime += DeltaSeconds;
	// the hero's own knockdown: thrown, down, back up
	static bool bHeroThrown = false;
	if (CombatTestTime > 13.f && !bHeroThrown)
	{
		bHeroThrown = true;
		KnockDown(-GetActorForwardVector());
	}
	if (bHeroThrown && CombatTestTime < 19.f)
	{
		UE_LOG(LogTemp, Display, TEXT("HEROTEST t=%.2f down=%d limp=%d blend=%.2f pelvis=%.0f drift=%.0f"), CombatTestTime, bKnockedDown ? 1 : 0, bLimp ? 1 : 0, RagdollBlend,
			GetMesh()->GetSocketLocation(TEXT("pelvis")).Z - (GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight()),
			FVector::Dist2D(GetMesh()->GetSocketLocation(TEXT("pelvis")), GetActorLocation()));
	}
	static bool bDumped = false;
	if (CombatTestTime > 5.f && !bDumped)
	{
		bDumped = true;
		GEngine->Exec(GetWorld(), TEXT("stat dumpframe -ms=0.4 -root=GameThread"));
	}
	// where each frame's time goes, once a second
	static float LastPerf = 0.f;
	if (CombatTestTime - LastPerf > 1.f)
	{
		LastPerf = CombatTestTime;
		UE_LOG(LogTemp, Display, TEXT("PERF frame=%.1fms game=%.1fms render=%.1fms gpu=%.1fms"), FApp::GetDeltaTime() * 1000.f,
			FPlatformTime::ToMilliseconds(GGameThreadTime), FPlatformTime::ToMilliseconds(GRenderThreadTime), FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()));
	}
	AEnemyCharacter* E = CombatTestTarget.Get();
	if (CombatTestTime > 20.f || (!E && CombatTestTime > 19.f))
	{
		UE_LOG(LogTemp, Display, TEXT("COMBATTEST end (%s)"), E ? TEXT("time") : TEXT("enemy gone"));
		CombatTestTime = -1.f;
		return;
	}
	if (!E) return;
	if (ACameraActor* Cam = CombatTestCamera.Get())
	{
		// side-on, level with the fighters, far enough back to see both whole
		const FVector Mid = (GetActorLocation() + E->GetActorLocation()) * 0.5f;
		const FVector Across = FVector::CrossProduct((E->GetActorLocation() - GetActorLocation()).GetSafeNormal2D(), FVector::UpVector);
		const FVector Eye = Mid + Across * 750.f + FVector(0.f, 0.f, 120.f);
		const FVector At = Cam->GetActorLocation().IsNearlyZero() ? Eye : FMath::VInterpTo(Cam->GetActorLocation(), Eye, DeltaSeconds, 3.f);
		Cam->SetActorLocationAndRotation(At, (Mid - At).Rotation());
	}
	// a guaranteed knockdown, so getting up is always exercised
	static bool bEnemyThrown = false;
	if (CombatTestTime > 1.f && !bEnemyThrown && !E->IsDown())
	{
		bEnemyThrown = true;
		E->KnockdownForTest((E->GetActorLocation() - GetActorLocation()).GetSafeNormal2D());
	}
	// scripted inputs: light combo, heavy, combo into finisher, dodge, execute when dazed
	struct FStep { float At; int32 Action; };
	static const FStep Steps[] = {
		{ 1.0f, 0 }, { 1.25f, 0 }, { 1.5f, 0 }, { 3.0f, 1 }, { 5.0f, 0 }, { 5.25f, 0 }, { 5.6f, 1 },
		{ 7.5f, 2 }, { 8.2f, 1 }, { 10.0f, 0 }, { 10.25f, 0 }, { 10.5f, 0 }, { 11.8f, 1 }, { 13.5f, 3 },
		{ 14.0f, 1 }, { 15.5f, 0 }, { 15.75f, 1 }, { 17.5f, 3 }, { 18.0f, 1 } };
	while (CombatTestStep < UE_ARRAY_COUNT(Steps) && CombatTestTime >= Steps[CombatTestStep].At)
	{
		// keep facing the enemy like a player would
		LastInputDir = (E->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		LastInputTime = GetWorld()->GetTimeSeconds();
		switch (Steps[CombatTestStep].Action)
		{
		case 0: LightAttack(); break;
		case 1: HeavyAttack(); break;
		case 2: Dodge(); break;
		case 3: Interact(); break;
		}
		++CombatTestStep;
	}
	// detector: a body crushed to a point (an additive clip played as a full pose) is drawn but invisible
	for (const ACharacter* Body : { static_cast<const ACharacter*>(this), static_cast<const ACharacter*>(E) })
	{
		const float Span = FVector::Dist(Body->GetMesh()->GetSocketLocation(TEXT("head")), Body->GetMesh()->GetSocketLocation(TEXT("pelvis")));
		if (Span < 25.f) UE_LOG(LogTemp, Display, TEXT("COLLAPSED t=%.2f %s head-pelvis=%.0f"), CombatTestTime, *Body->GetName(), Span);
	}
	// detector: the camera inside or touching an enemy (then the body can't be seen, only its health bar)
	if (APlayerController* CamPC = Cast<APlayerController>(Controller); CamPC && CamPC->PlayerCameraManager)
	{
		const FVector Cam = CamPC->PlayerCameraManager->GetCameraLocation();
		for (TActorIterator<AEnemyCharacter> It(GetWorld()); It; ++It)
		{
			if (It->IsDead()) continue;
			const float Flat = FVector::Dist2D(Cam, It->GetActorLocation());
			const float Up = FMath::Abs(Cam.Z - It->GetActorLocation().Z);
			const float R = It->GetCapsuleComponent()->GetScaledCapsuleRadius() + 45.f;
			if (Flat < R && Up < It->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 40.f)
			{
				{
					// what the camera arm runs into
					const FVector Pivot = CameraBoom->GetComponentLocation();
					const FVector Want = Pivot - CamPC->GetControlRotation().Vector() * CameraBoom->TargetArmLength;
					FHitResult Hit;
					FCollisionQueryParams Q(SCENE_QUERY_STAT(CamDebug), false, this);
					if (GetWorld()->SweepSingleByChannel(Hit, Pivot, Want, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(CameraBoom->ProbeSize), Q))
					{
						UE_LOG(LogTemp, Display, TEXT("CAMBLOCK by %s / %s start-penetrating=%d dist=%.0f"), *GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()), Hit.bStartPenetrating ? 1 : 0, Hit.Distance);
					}
				}
				UE_LOG(LogTemp, Display, TEXT("CAMINSIDE t=%.2f %s flat=%.0f up=%.0f heroToEnemy=%.0f camToHero=%.0f arm=%.0f compCamToHero=%.0f viewtarget=%s dil=%.2f"), CombatTestTime, *It->GetName(), Flat, Up, FVector::Dist2D(GetActorLocation(), It->GetActorLocation()), FVector::Dist(Cam, GetActorLocation()), CameraBoom->TargetArmLength, FVector::Dist(FollowCamera->GetComponentLocation(), GetActorLocation()), *GetNameSafe(CamPC->GetViewTarget()), UGameplayStatics::GetGlobalTimeDilation(this));
			}
		}
	}
	// detector: on screen but not drawn = the "vanishing" bug; also report the mesh drifting off its capsule
	const FVector MeshLoc = E->GetMesh()->GetComponentLocation();
	const float Drift = FVector::Dist2D(MeshLoc, E->GetActorLocation());
	const bool bRendered = E->GetMesh()->WasRecentlyRendered(0.15f);
	// only meaningful when the enemy is inside the camera's view
	bool bOnScreen = false;
	bool bBehindHero = false;
	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		// is the enemy's whole box hidden behind the hero's silhouette box? then not drawing it is correct
		FBox2D HeroRect(ForceInit), EnemyRect(ForceInit);
		auto Rect = [PC](const FBoxSphereBounds& B, FBox2D& Out)
		{
			for (int32 c = 0; c < 8; ++c)
			{
				const FVector Corner = B.Origin + B.BoxExtent * FVector(c & 1 ? 1 : -1, c & 2 ? 1 : -1, c & 4 ? 1 : -1);
				FVector2D P;
				if (PC->ProjectWorldLocationToScreen(Corner, P)) Out += P;
			}
		};
		Rect(GetMesh()->Bounds, HeroRect);
		Rect(FBoxSphereBounds(E->GetMesh()->Bounds.Origin, E->GetMesh()->Bounds.BoxExtent / 1.6f, 0.f), EnemyRect);
		bBehindHero = HeroRect.bIsValid && EnemyRect.bIsValid && HeroRect.IsInside(EnemyRect.Min) && HeroRect.IsInside(EnemyRect.Max);
		FVector2D ScreenPos;
		int32 SX = 0, SY = 0;
		PC->GetViewportSize(SX, SY);
		bOnScreen = PC->ProjectWorldLocationToScreen(E->GetActorLocation(), ScreenPos) && ScreenPos.X > 0 && ScreenPos.Y > 0 && ScreenPos.X < SX && ScreenPos.Y < SY;
	}
	static float LastLog = 0.f;
	const bool bVanished = bOnScreen && !bRendered;
	if (bVanished || Drift > 60.f || CombatTestTime - LastLog > 1.f)
	{
		LastLog = CombatTestTime;
		UE_LOG(LogTemp, Display, TEXT("COMBATTEST t=%.2f state=%d hp=%.0f stance=%.2f onscreen=%d rendered=%d %s drift=%.0f dist=%.0f bounds=%s ext=%s vis=%d behindhero=%d pelvis=%.0f"),
			CombatTestTime, (int32)E->GetStateForTest(), E->Health->Health, E->GetStancePercent(), bOnScreen ? 1 : 0, bRendered ? 1 : 0,
			bVanished ? TEXT("VANISHED") : TEXT("ok"), Drift, FVector::Dist(E->GetActorLocation(), GetActorLocation()),
			*(E->GetMesh()->Bounds.Origin - E->GetActorLocation()).ToCompactString(), *E->GetMesh()->Bounds.BoxExtent.ToCompactString(),
			E->GetMesh()->IsVisible() ? 1 : 0, bBehindHero ? 1 : 0,
			// pelvis height above the floor the capsule stood on: about 10-20 when lying flat
			E->GetAimPoint().Z - (E->GetActorLocation().Z - E->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
	}
#endif
}

void AEldoriaCharacter::SlowMotion(float Dilation, float RealSeconds)
{
	const double Until = FPlatformTime::Seconds() + RealSeconds;
	HitStopUntil = FMath::Max(HitStopUntil, Until);
	UGameplayStatics::SetGlobalTimeDilation(this, Dilation);
	TWeakObjectPtr<AEldoriaCharacter> Self(this);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self](float)
	{
		if (!Self.IsValid()) return false;
		if (FPlatformTime::Seconds() < Self->HitStopUntil) return true;
		UGameplayStatics::SetGlobalTimeDilation(Self.Get(), 1.f);
		return false;
	}));
}

AEnemyCharacter* AEldoriaCharacter::StunnedEnemyInReach() const
{
	AEnemyCharacter* Best = nullptr;
	float BestDist = 330.f;
	for (TActorIterator<AEnemyCharacter> It(GetWorld()); It; ++It)
	{
		if (!It->IsStunned()) continue;
		const float D = FVector::Dist2D(It->GetActorLocation(), GetActorLocation());
		if (D < BestDist)
		{
			BestDist = D;
			Best = *It;
		}
	}
	return Best;
}

void AEldoriaCharacter::TryExecute(AEnemyCharacter* Target)
{
	if (!Target || bExecuting) return;
	bExecuting = true;
	bAttacking = true;
	if (bBlocking) StopBlock();
	SetActorRotation(FRotator(0.f, (Target->GetActorLocation() - GetActorLocation()).Rotation().Yaw, 0.f));
	PlayOrDefault(ExecutionAnim, 1.f, 0.9f);
	UEldoriaSettings::PlaySoundRandom(this, UEldoriaSettings::Get()->HeavySwingSounds, GetActorLocation(), 1.f, 0.f);
	UEldoriaSettings::PlaySound(this, UEldoriaSettings::Get()->HeroAttackVoice, GetActorLocation(), 1.f, 0.f);
	// slow motion and a close camera for the killing blow
	SlowMotion(0.35f, 0.9f);
	CameraPunch = 140.f;
	FovPunch = 12.f;

	TWeakObjectPtr<AEldoriaCharacter> Self(this);
	TWeakObjectPtr<AEnemyCharacter> Victim(Target);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self, Victim](float)
	{
		if (Self.IsValid() && Victim.IsValid())
		{
			UEldoriaSettings::PlayImpact(Self.Get(), Victim->GetActorLocation(), 1.2f);
			Victim->Execute(Self.Get());
			Self->ImpactFeedback(1.2f);
			if (UParticleSystem* FX = UEldoriaSettings::Get()->GroundSmashEffect.LoadSynchronous())
			{
				UGameplayStatics::SpawnEmitterAtLocation(Self.Get(), FX, Victim->GetActorLocation() - FVector(0.f, 0.f, 90.f), FRotator::ZeroRotator, FVector(0.6f));
			}
			Self->Callout = FText::FromString(TEXT("Execution"));
			Self->CalloutUntil = Self->GetWorld()->GetTimeSeconds() + 1.6f;
		}
		if (Self.IsValid())
		{
			Self->bExecuting = false;
			Self->bAttacking = false;
		}
		return false;
	}), 0.45f);
}


void AEldoriaCharacter::OpenPauseMenu()
{
	if (AEldoriaGameMode* GM = GetWorld()->GetAuthGameMode<AEldoriaGameMode>())
	{
		if (!GM->IsMenuOpen()) GM->ShowMenu(EMenuPage::Pause);
	}
}

void AEldoriaCharacter::ToggleHelp()
{
	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (AEldoriaHUD* HUD = Cast<AEldoriaHUD>(PC->GetHUD())) HUD->ToggleHelp();
	}
}

void AEldoriaCharacter::Interact()
{
	if (IsDead() || bKnockedDown) return;
	if (AEnemyCharacter* Stunned = StunnedEnemyInReach())
	{
		TryExecute(Stunned);
		return;
	}
	AChest* Best = nullptr;
	float BestDist = 260.f;
	for (TActorIterator<AChest> It(GetWorld()); It; ++It)
	{
		const float D = FVector::Dist(It->GetActorLocation(), GetActorLocation());
		if (!It->IsOpened() && D < BestDist)
		{
			BestDist = D;
			Best = *It;
		}
	}
	if (Best) Best->Open(this);
}

float AEldoriaCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// on the ground nothing more lands: no being beaten while helpless
	if (IsDead() || bExecuting || bKnockedDown) return 0.f;
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < InvulnerableUntil)
	{
		// dodged right as the blow came in: time slows for a counter-attack
		if (bDodging && !bPerfectDodgeUsed && Now - DodgeStartTime < 0.3f)
		{
			bPerfectDodgeUsed = true;
			SlowMotion(0.3f, 0.9f);
			Callout = FText::FromString(TEXT("Perfect Dodge"));
			CalloutUntil = Now + 1.4f;
		}
		return 0.f;
	}

	AEnemyCharacter* Attacker = Cast<AEnemyCharacter>(DamageCauser);
	const bool bUnblockable = Attacker && Attacker->IsAttackUnblockable();
	const FVector ToAttacker = DamageCauser ? (DamageCauser->GetActorLocation() - GetActorLocation()).GetSafeNormal2D() : -GetActorForwardVector();
	const bool bFacing = FVector::DotProduct(GetActorForwardVector(), ToAttacker) > 0.3f;

	// shield raised at the last moment: the blow is deflected and the attacker left open
	if (bBlocking && bFacing && !bUnblockable && Now - BlockStartTime < 0.3f && Attacker)
	{
		Attacker->OnParried(this);
		UEldoriaSettings::PlaySound(this, UEldoriaSettings::Get()->BlockSound, GetActorLocation(), 1.2f, 0.05f);
		if (UParticleSystem* FX = UEldoriaSettings::Get()->ParryEffect.LoadSynchronous())
		{
			UGameplayStatics::SpawnEmitterAtLocation(this, FX, GetActorLocation() + GetActorForwardVector() * 70.f + FVector(0.f, 0.f, 40.f));
		}
		ImpactFeedback(0.8f);
		SlowMotion(0.25f, 0.35f);
		Callout = FText::FromString(TEXT("Parry"));
		CalloutUntil = Now + 1.2f;
		return 0.f;
	}

	// A crushing blow (a fifth of the hero's health in one hit) or the one that leaves him critically
	// hurt throws him to the ground, though not again and again in quick succession.
	const float MaxHealth = Health->MaxHealth;
	const bool bCrushing = DamageAmount >= MaxHealth * 0.2f;
	const bool bCritical = Health->Health > MaxHealth * 0.25f && Health->Health - DamageAmount <= MaxHealth * 0.25f;
	bool bThrown = false;

	bool bBlocked = false;
	if (bBlocking && bFacing && !bUnblockable)
	{
		bBlocked = true;
		DamageAmount *= 0.15f;
		LaunchCharacter(-ToAttacker * 450.f, true, false);
		UEldoriaSettings::PlaySound(this, UEldoriaSettings::Get()->BlockSound, GetActorLocation(), 1.f, 0.1f);
		ImpactFeedback(0.35f);
	}
	else
	{
		// a real hit: thrown back, a hard jolt, blood on the screen
		bThrown = (bCrushing || bCritical) && Now - LastKnockdownTime > 8.f;
		if (!bThrown) LaunchCharacter(-ToAttacker * 700.f + FVector(0.f, 0.f, 260.f), true, true);
		ImpactFeedback(FMath::GetMappedRangeValueClamped(FVector2D(8.f, 30.f), FVector2D(0.45f, 0.9f), DamageAmount));
		DamageFlash = 1.f;
		BloodFlash = FMath::Min(1.f, BloodFlash + FMath::GetMappedRangeValueClamped(FVector2D(5.f, 30.f), FVector2D(0.55f, 1.f), DamageAmount));
		bAttacking = false;
		bQueued = false;
		GetWorldTimerManager().ClearTimer(HitTimer);
		GetWorldTimerManager().ClearTimer(EndTimer);
	}
	AEldoriaHUD::AddNumber(this, GetActorLocation() + FVector(0.f, 0.f, 110.f), DamageAmount, bBlocked ? FLinearColor(0.7f, 0.8f, 1.f) : FLinearColor(1.f, 0.3f, 0.25f));
	if (!bBlocked) UEldoriaSettings::SpillBlood(this, GetActorLocation(), -ToAttacker, FMath::GetMappedRangeValueClamped(FVector2D(8.f, 40.f), FVector2D(40.f, 95.f), DamageAmount));

	const float Dealt = Health->ApplyDamage(DamageAmount);
	InvulnerableUntil = GetWorld()->GetTimeSeconds() + 0.4f;
	if (Health->IsDead())
	{
		Die();
	}
	else if (bThrown)
	{
		UEldoriaSettings::PlaySound(this, UEldoriaSettings::Get()->HeroPainVoice, GetActorLocation(), 1.f, 0.06f);
		UEldoriaSettings::PlayImpact(this, GetActorLocation(), 1.f);
		KnockDown(-ToAttacker);
	}
	else if (DamageAmount > 5.f)
	{
		UEldoriaSettings::PlaySound(this, UEldoriaSettings::Get()->HeroPainVoice, GetActorLocation(), 0.9f, 0.06f);
		UEldoriaSettings::PlayImpact(this, GetActorLocation(), 0.7f);
		bAttacking = false;
		PlayOrDefault(HitAnim, 1.4f, 0.f);
	}
	return Dealt;
}

void AEldoriaCharacter::Heal(float Amount)
{
	Health->Heal(Amount);
}

void AEldoriaCharacter::Die()
{
	bAttacking = bBlocking = bDodging = false;
	GetCharacterMovement()->DisableMovement();
	UEldoriaSettings::PlaySound(this, UEldoriaSettings::Get()->HeroDeathVoice, GetActorLocation(), 1.f, 0.f);
	GetWorldTimerManager().ClearTimer(KnockdownTimer);
	// the clip starts the fall, the ragdoll finishes it: he ends up flat wherever he drops, and lets go of the sword
	const float Fall = UEldoriaSettings::PlayAnim(GetMesh(), DeathAnim, 1.f, UEldoriaSettings::Get()->HeroAnimSlot);
	GetWorldTimerManager().SetTimer(KnockdownTimer, [this]()
	{
		bLimp = UEldoriaSettings::StartRagdoll(GetMesh(), false);
	}, FMath::Max(0.05f, Fall * 0.55f), false);
	if (AEldoriaGameMode* GM = GetWorld()->GetAuthGameMode<AEldoriaGameMode>())
	{
		GM->OnHeroDied();
	}
}

void AEldoriaCharacter::KnockDown(const FVector& Push)
{
	if (bBlocking) StopBlock();
	bKnockedDown = true;
	bAttacking = bQueued = bDodging = false;
	LastKnockdownTime = GetWorld()->GetTimeSeconds();
	GetWorldTimerManager().ClearTimer(HitTimer);
	GetWorldTimerManager().ClearTimer(EndTimer);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	if (UAnimInstance* Anim = GetMesh()->GetAnimInstance()) Anim->StopAllMontages(0.1f);
	if (UEldoriaAnimInstance* Native = Cast<UEldoriaAnimInstance>(GetMesh()->GetAnimInstance())) Native->StopAction();
	ImpactFeedback(1.f);
	DamageFlash = 1.f;
	BloodFlash = 1.f;
	// thrown by the blow and sprawled where he lands, sword still in his fist
	bLimp = UEldoriaSettings::StartRagdoll(GetMesh(), true);
	if (bLimp) GetMesh()->SetAllPhysicsLinearVelocity(Push * 260.f + FVector(0.f, 0.f, 160.f));
	// a moment on the ground before he can push himself up
	GetWorldTimerManager().SetTimer(KnockdownTimer, this, &AEldoriaCharacter::GetUpFromRagdoll, bLimp ? 2.f : 1.2f, false);
}

void AEldoriaCharacter::GetUpFromRagdoll()
{
	if (IsDead()) return;
	USkeletalMeshComponent* Body = GetMesh();
	const UEldoriaSettings* S = UEldoriaSettings::Get();
	UAnimSequenceBase* Rise = S->HeroGetUpAnim.LoadSynchronous();
	UAnimInstance* Anim = Body->GetAnimInstance();
	if (!bLimp || !Rise || !Anim)
	{
		// nothing to rise with: hand the body straight back to animation
		RagdollBlend = bLimp ? 0.01f : 0.f;
		FinishGetUp();
		return;
	}
	const FVector Pelvis = Body->GetSocketLocation(TEXT("pelvis"));
	const FVector HeadDir = (Body->GetSocketLocation(TEXT("head")) - Pelvis).GetSafeNormal2D();

	// He pushes himself up where he lies, facing the way his head points: the ragdoll's pose fades
	// into the low first frame of the rising clip (on one knee), and the clip stands him up from there.
	// A simulating mesh is detached from the capsule and can't be re-attached until physics lets go,
	// so it is placed where the capsule would hold it.
	Body->PhysicsTransformUpdateMode = EPhysicsTransformUpdateMode::ComponentTransformIsKinematic;
	Body->bBlendPhysics = false;
	const float Yaw = HeadDir.IsNearlyZero() ? GetActorRotation().Yaw : HeadDir.Rotation().Yaw;
	// the camera stays where the player left it
	SetActorLocationAndRotation(FVector(Pelvis.X, Pelvis.Y, GetActorLocation().Z), FRotator(0.f, Yaw, 0.f));
	const FTransform Held = FTransform(FRotator(0.f, S->HeroMeshYaw, 0.f), FVector(0.f, 0.f, S->HeroMeshZ)) * GetCapsuleComponent()->GetComponentTransform();
	Body->SetWorldLocationAndRotation(Held.GetLocation(), Held.GetRotation());

	const float From = FMath::Clamp(S->HeroGetUpStart, 0.f, Rise->GetPlayLength());
	const float To = S->HeroGetUpEnd > From ? FMath::Min(S->HeroGetUpEnd, Rise->GetPlayLength()) : Rise->GetPlayLength();
	constexpr float Rate = 1.05f;
	if (UEldoriaAnimInstance* Native = Cast<UEldoriaAnimInstance>(Anim))
	{
		Native->PlayAction(Rise, Rate, false, false, From);
		FTimerHandle StopTimer;
		GetWorldTimerManager().SetTimer(StopTimer, [this]()
		{
			if (UEldoriaAnimInstance* A = Cast<UEldoriaAnimInstance>(GetMesh()->GetAnimInstance())) A->StopAction();
		}, (To - From) / Rate, false);
	}
	else if (UAnimMontage* Montage = Anim->PlaySlotAnimationAsDynamicMontage(Rise, S->HeroAnimSlot, 0.f, 0.3f, Rate))
	{
		Anim->Montage_SetPosition(Montage, From);
		// let go of the clip once he is standing, before it carries on into its idle
		const float RiseTime = (To - From) / Rate;
		FTimerHandle StopTimer;
		GetWorldTimerManager().SetTimer(StopTimer, [this, Montage]()
		{
			if (UAnimInstance* A = GetMesh()->GetAnimInstance()) A->Montage_Stop(0.3f, Montage);
		}, RiseTime, false);
	}
	RagdollBlend = 1.f;
	Body->SetAllBodiesPhysicsBlendWeight(1.f);
	GetWorldTimerManager().SetTimer(KnockdownTimer, this, &AEldoriaCharacter::FinishGetUp, (To - From) / Rate, false);
}

void AEldoriaCharacter::FinishGetUp()
{
	bKnockedDown = false;
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	InvulnerableUntil = GetWorld()->GetTimeSeconds() + 0.6f;
}

void AEldoriaCharacter::Respawn(const FTransform& At)
{
	GetWorldTimerManager().ClearTimer(KnockdownTimer);
	bKnockedDown = bLimp = false;
	RagdollBlend = 0.f;
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	GetMesh()->SetAllBodiesSimulatePhysics(false);
	GetMesh()->bBlendPhysics = false;
	GetMesh()->PhysicsTransformUpdateMode = EPhysicsTransformUpdateMode::SimulationUpatesComponentTransform;
	GetMesh()->SetSimulatePhysics(false);
	GetMesh()->RecreatePhysicsState();
	GetMesh()->SetCollisionProfileName(TEXT("CharacterMesh"));
	// back from a ragdoll: the body must never stop the camera again (the profile resets that)
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	const UEldoriaSettings* S = UEldoriaSettings::Get();
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, S->HeroMesh.IsNull() ? -96.f : S->HeroMeshZ), FRotator(0.f, S->HeroMesh.IsNull() ? -90.f : S->HeroMeshYaw, 0.f));
	if (GetMesh()->GetAnimInstance()) GetMesh()->GetAnimInstance()->StopAllMontages(0.f);
	if (UEldoriaAnimInstance* Native = Cast<UEldoriaAnimInstance>(GetMesh()->GetAnimInstance())) Native->StopAction();
	SetActorTransform(At, false, nullptr, ETeleportType::TeleportPhysics);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	Health->Reset(Health->MaxHealth);
	InvulnerableUntil = GetWorld()->GetTimeSeconds() + 2.f;
}

void AEldoriaCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UCharacterMovementComponent* Move = GetCharacterMovement();

	TickCombatTest(DeltaSeconds);
#if !UE_BUILD_SHIPPING
	// development: -eldoriawalktest walks straight ahead for four seconds, then runs
	// (then walks with the guard up)
	static double WalkTestStart = -1.0;
	if (FParse::Param(FCommandLine::Get(), TEXT("eldoriawalktest")) && WalkTestStart < 0.0 && GetWorld()->GetRealTimeSeconds() > 1.0) WalkTestStart = GetWorld()->GetTimeSeconds();
	if (WalkTestStart >= 0.0 && GetWorld()->GetTimeSeconds() - WalkTestStart > 3.f && GetWorld()->GetTimeSeconds() - WalkTestStart < 16.f)
	{
		const float T = GetWorld()->GetTimeSeconds() - WalkTestStart;
		bRunning = T > 7.f && T < 12.f;
		if (T > 12.f && !bBlocking) StartBlock();
		if (T > 12.f) UE_LOG(LogTemp, Display, TEXT("WALKTEST block t=%.1f speed=%.0f"), T, GetVelocity().Size2D());
		// walk along a fixed heading, filmed from the side
		static float Heading = GetActorRotation().Yaw;
		AddMovementInput(FRotator(0.f, Heading, 0.f).Vector());
		if (Controller) Controller->SetControlRotation(FRotator(-8.f, Heading + 80.f, 0.f));
	}
#endif

	// hit feedback decays; blood on the screen lingers longer than the armour flash
	BloodFlash = FMath::Max(0.f, BloodFlash - DeltaSeconds * 0.55f);
	if (DamageFlash > 0.f)
	{
		DamageFlash = FMath::Max(0.f, DamageFlash - DeltaSeconds * 2.5f);
	}
	// camera reactions run on real time so they stay crisp during hit-stop
	const float RealDt = FMath::Min(static_cast<float>(FApp::GetDeltaTime()), 0.05f);
	// how close the nearest living enemy is decides how far into combat framing the camera goes
	{
		float Nearest = TNumericLimits<float>::Max();
		for (TActorIterator<AEnemyCharacter> It(GetWorld()); It; ++It)
		{
			if (!It->IsDead()) Nearest = FMath::Min(Nearest, FVector::Dist2D(It->GetActorLocation(), GetActorLocation()));
		}
		const float Want = FMath::GetMappedRangeValueClamped(FVector2D(900.f, 500.f), FVector2D(0.f, 1.f), Nearest);
		CombatFraming = FMath::FInterpTo(CombatFraming, Want, RealDt, 2.5f);
	}
	FVector Offset = FMath::Lerp(BaseSocketOffset, CombatSocketOffset, CombatFraming);
	if (ShakeTime > 0.f)
	{
		ShakeTime = FMath::Max(0.f, ShakeTime - RealDt);
		const float K = ShakeTime / FMath::Max(0.01f, ShakeDuration);
		const float A = ShakeStrength * K * K;
		// the first instant snaps in the swing's direction, then it rattles out
		const FVector2D Dir = K > 0.8f ? ShakeDirection * 1.4f : FVector2D(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f));
		Offset += FVector(0.f, Dir.X * A, Dir.Y * A);
	}
	CameraPunch = FMath::FInterpTo(CameraPunch, 0.f, RealDt, 9.f);
	Offset.X += CameraPunch;
	CameraBoom->SocketOffset = Offset;

	// heartbeat when badly hurt, quickening as health drops
	const float HealthPct = Health->GetPercent();
	if (!IsDead() && HealthPct < 0.3f)
	{
		HeartbeatTimer -= RealDt;
		if (HeartbeatTimer <= 0.f)
		{
			HeartbeatTimer = HealthPct < 0.15f ? 0.62f : 0.85f;
			UEldoriaSettings::PlaySoundRandom(this, UEldoriaSettings::Get()->LowHealthSounds, GetActorLocation(), 0.75f, 0.f);
		}
	}
	FovPunch = FMath::FInterpTo(FovPunch, 0.f, RealDt, 8.f);
	if (FringePulse > 0.f)
	{
		FringePulse = FMath::FInterpTo(FringePulse, 0.f, RealDt, 7.f);
		FollowCamera->PostProcessSettings.bOverride_SceneFringeIntensity = true;
		FollowCamera->PostProcessSettings.SceneFringeIntensity = FringePulse;
	}
	// standing up: the ragdoll's pose fades into the rising clip, then physics lets go of the body
	if (RagdollBlend > 0.f)
	{
		USkeletalMeshComponent* Body = GetMesh();
		RagdollBlend = FMath::Max(0.f, RagdollBlend - DeltaSeconds / 0.8f);
		Body->SetAllBodiesPhysicsBlendWeight(RagdollBlend * RagdollBlend);
		if (RagdollBlend <= 0.f)
		{
			bLimp = false;
			Body->SetAllBodiesSimulatePhysics(false);
			Body->SetSimulatePhysics(false);
			Body->PhysicsTransformUpdateMode = EPhysicsTransformUpdateMode::SimulationUpatesComponentTransform;
			Body->SetCollisionProfileName(TEXT("CharacterMesh"));
			// back from a ragdoll: the body must never stop the camera again (the profile resets that)
			Body->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
			const UEldoriaSettings* S = UEldoriaSettings::Get();
			Body->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			Body->SetRelativeLocationAndRotation(FVector(0.f, 0.f, S->HeroMeshZ), FRotator(0.f, S->HeroMeshYaw, 0.f));
			// brings back any bodies taken away for the fall, ready for the next one
			Body->RecreatePhysicsState();
		}
	}
	float Speed = bRunning ? RunSpeed : WalkSpeed;
	if (bBlocking) Speed = 200.f;
	Move->MaxWalkSpeed = FMath::FInterpTo(Move->MaxWalkSpeed, Speed, DeltaSeconds, 8.f);

	// footsteps
	const float GroundSpeed = GetVelocity().Size2D();
	if (GroundSpeed > 60.f && Move->IsMovingOnGround())
	{
		StepDistance += GroundSpeed * DeltaSeconds;
		if (StepDistance > (bRunning ? 200.f : 125.f))
		{
			StepDistance = 0.f;
			const UEldoriaSettings* S = UEldoriaSettings::Get();
			UEldoriaSettings::PlaySoundRandom(this, GroundSpeed > 380.f ? S->RunFootstepSounds : S->WalkFootstepSounds, GetActorLocation() - FVector(0.f, 0.f, 90.f), bRunning ? 0.7f : 0.5f, 0.05f);
		}
	}

	// pull the camera back a little while running
	float WantArm = (bRunning ? 450.f : TargetArmLength) + 90.f * CombatFraming;
	// Enemies don't block the camera arm (they would shove it about in every melee), so when one gets
	// between the camera and the hero (a dodge or lunge straight into him) the camera would sit inside
	// his body. Pull the camera in to just in front of any enemy on the line of sight.
	bool bEnemyInWay = false;
	{
		const FVector Pivot = CameraBoom->GetComponentLocation();
		const FVector ToCam = FollowCamera->GetComponentLocation() - Pivot;
		const float Reach = ToCam.Size();
		if (Reach > 1.f)
		{
			const FVector Dir = ToCam / Reach;
			for (TActorIterator<AEnemyCharacter> It(GetWorld()); It; ++It)
			{
				if (It->IsDead()) continue;
				const UCapsuleComponent* Cap = It->GetCapsuleComponent();
				const FVector ToEnemy = It->GetActorLocation() - Pivot;
				const float Along = FVector::DotProduct(ToEnemy, Dir);
				const FVector Off = ToEnemy - Dir * Along;
				const float Clearance = Cap->GetScaledCapsuleRadius() + 45.f;
				if (Along <= 0.f || Along > Reach + Clearance) continue;
				if (FVector2D(Off).Size() > Clearance || FMath::Abs(Off.Z) > Cap->GetScaledCapsuleHalfHeight() + 30.f) continue;
				// arm length that puts the camera in front of this enemy's body
				const float Fit = CameraBoom->TargetArmLength * FMath::Max(Along - Clearance, 0.f) / Reach;
				WantArm = FMath::Min(WantArm, FMath::Max(Fit, 120.f));
				bEnemyInWay = true;
			}
		}
	}
	// pull in quickly, ease back out slowly once the way is clear
	const float ArmSpeed = WantArm < CameraBoom->TargetArmLength ? (bEnemyInWay ? 18.f : 4.f) : 3.f;
	CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, WantArm, RealDt, ArmSpeed);
	const float BaseFov = FMath::FInterpTo(FollowCamera->FieldOfView + FovPunch, bRunning ? 79.f : 75.f, DeltaSeconds, 4.f);
	FollowCamera->SetFieldOfView(BaseFov - FovPunch);
}
