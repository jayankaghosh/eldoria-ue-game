#include "Pickups.h"
#include "EldoriaCharacter.h"
#include "EldoriaGameMode.h"
#include "EldoriaSettings.h"
#include "HealthComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

APickup::APickup()
{
	PrimaryActorTick.bCanEverTick = true;

	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	Trigger->InitSphereRadius(90.f);
	Trigger->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	RootComponent = Trigger;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Trigger);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(true);

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Trigger);
	Glow->SetIntensity(800.f);
	Glow->SetAttenuationRadius(220.f);
	Glow->SetCastShadows(false);
	// pickups spin and bob every frame; as navigation obstacles each move would queue a navmesh update
	Trigger->SetCanEverAffectNavigation(false);
	Mesh->SetCanEverAffectNavigation(false);

	// engine primitives stand in until real props are configured
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cylinder.Succeeded()) Mesh->SetStaticMesh(Cylinder.Object);
}

void APickup::BeginPlay()
{
	Super::BeginPlay();
	BaseLocation = GetActorLocation();
	Phase = FMath::FRandRange(0.f, 6.f);
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &APickup::OnOverlap);

	// drop onto whatever is below so scattered loot rests on the ground
	FHitResult Hit;
	const FVector Start = GetActorLocation() + FVector(0.f, 0.f, 100.f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PickupGround), false, this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, Start - FVector(0.f, 0.f, 2000.f), ECC_WorldStatic, Params))
	{
		BaseLocation = Hit.Location + FVector(0.f, 0.f, RestHeight);
	}
	if (!bHover)
	{
		// lies where it fell, at a random turn
		SetActorLocation(BaseLocation);
		Mesh->SetRelativeRotation(FRotator(SpinPitch, FMath::FRandRange(0.f, 360.f), 0.f));
	}
}

void APickup::SetLook(UStaticMesh* Override, const FLinearColor& Color, float Scale, UMaterialInterface* Material)
{
	if (Override)
	{
		Mesh->SetStaticMesh(Override);
		Mesh->SetRelativeScale3D(FVector(Scale));
		Mesh->SetRelativeRotation(FRotator::ZeroRotator);
	}
	if (Material)
	{
		Mesh->SetMaterial(0, Material);
	}
	else if (!Override)
	{
		if (UMaterialInterface* Base = Mesh->GetMaterial(0))
		{
			UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(Base, this);
			Mat->SetVectorParameterValue(TEXT("Color"), Color);
			Mesh->SetMaterial(0, Mat);
		}
	}
	Glow->SetLightColor(Color);
}

void APickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	const float T = GetWorld()->GetTimeSeconds() + Phase;
	FVector Loc = bHover ? BaseLocation + FVector(0.f, 0.f, FMath::Sin(T * 2.4f) * 12.f) : BaseLocation;
	bool bPulled = false;

	if (MagnetRadius > 0.f && Age > 0.5f)
	{
		if (AEldoriaCharacter* Hero = Cast<AEldoriaCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))
		{
			const FVector Target = Hero->GetActorLocation();
			if (!Hero->IsDead() && FVector::Dist(Target, GetActorLocation()) < MagnetRadius)
			{
				BaseLocation = FMath::VInterpTo(GetActorLocation(), Target, DeltaSeconds, 9.f);
				Loc = BaseLocation;
				bPulled = true;
				if (FVector::Dist(Target, Loc) < 90.f) TryCollect(Hero);
			}
		}
	}
	if (!bHover && !bPulled) return;
	SetActorLocation(Loc);
	if (bHover) Mesh->SetRelativeRotation(FRotator(SpinPitch, T * 160.f, 0.f));
}

void APickup::OnOverlap(UPrimitiveComponent* Comp, AActor* Other, UPrimitiveComponent* OtherComp, int32 Index, bool bSweep, const FHitResult& Hit)
{
	TryCollect(Other);
}

void APickup::TryCollect(AActor* Other)
{
	AEldoriaCharacter* Hero = Cast<AEldoriaCharacter>(Other);
	if (bCollected || !Hero || Hero->IsDead() || Age < 0.4f || !CanCollect(Hero)) return;
	bCollected = true;
	if (AEldoriaGameMode* GM = GetWorld()->GetAuthGameMode<AEldoriaGameMode>()) GM->OnPickupCollected(this);
	Collect(Hero);
	Destroy();
}

// ---------- coin ----------

ACoinPickup::ACoinPickup()
{
	MagnetRadius = 320.f;
	Mesh->SetRelativeScale3D(FVector(0.32f, 0.32f, 0.05f));
	// coins are loot from the fallen: they lie flat on the ground where they scattered
	SpinPitch = 0.f;
	bHover = false;
	RestHeight = 3.f;
}

void ACoinPickup::BeginPlay()
{
	Super::BeginPlay();
	const UEldoriaSettings* S = UEldoriaSettings::Get();
	SetLook(S->CoinMesh.LoadSynchronous(), FLinearColor(1.f, 0.72f, 0.18f), 1.f, S->CoinMaterial.LoadSynchronous());
}

void ACoinPickup::Collect(AEldoriaCharacter* Hero)
{
	Hero->AddCoins(Value);
	UEldoriaSettings::PlaySoundRandom(this, UEldoriaSettings::Get()->CoinSounds, Hero->GetActorLocation(), 0.6f, 0.08f);
	if (AEldoriaGameMode* GM = GetWorld()->GetAuthGameMode<AEldoriaGameMode>()) GM->OnCoinCollected(Value);
}

// ---------- potion ----------

AHealthPickup::AHealthPickup()
{
	Mesh->SetRelativeScale3D(FVector(0.3f, 0.3f, 0.45f));
}

void AHealthPickup::BeginPlay()
{
	Super::BeginPlay();
	SetLook(UEldoriaSettings::Get()->PotionMesh.LoadSynchronous(), FLinearColor(0.2f, 1.f, 0.35f), 1.6f);
	Glow->SetIntensity(2500.f);
}

bool AHealthPickup::CanCollect(AEldoriaCharacter* Hero) const
{
	return Hero->Health->Health < Hero->Health->MaxHealth;
}

void AHealthPickup::Collect(AEldoriaCharacter* Hero)
{
	Hero->Heal(HealAmount);
	UEldoriaSettings::PlaySound2D(this, UEldoriaSettings::Get()->PotionSound, 0.8f);
	if (AEldoriaGameMode* GM = GetWorld()->GetAuthGameMode<AEldoriaGameMode>()) GM->ShowMessage(FText::FromString(TEXT("+50 Health")), 1.5f);
}
