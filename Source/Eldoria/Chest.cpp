#include "Chest.h"
#include "EldoriaCharacter.h"
#include "EldoriaGameMode.h"
#include "EldoriaSettings.h"
#include "Pickups.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

AChest::AChest()
{
	PrimaryActorTick.bCanEverTick = true;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	RootComponent = Body;
	Body->SetMobility(EComponentMobility::Movable);
	Lid = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Lid"));
	Lid->SetupAttachment(Body);
	if (Cube.Succeeded())
	{
		Body->SetStaticMesh(Cube.Object);
		Lid->SetStaticMesh(Cube.Object);
	}
	Body->SetRelativeScale3D(FVector(1.f, 0.7f, 0.6f));
	Lid->SetRelativeLocation(FVector(0.f, -35.f, 60.f));
	Lid->SetRelativeScale3D(FVector(1.f, 1.f, 0.3f));

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Body);
	Glow->SetRelativeLocation(FVector(0.f, 0.f, 150.f));
	Glow->SetLightColor(FLinearColor(1.f, 0.75f, 0.35f));
	Glow->SetIntensity(1500.f);
	Glow->SetAttenuationRadius(400.f);
}

void AChest::BeginPlay()
{
	Super::BeginPlay();
	if (UStaticMesh* M = UEldoriaSettings::Get()->ChestMesh.LoadSynchronous())
	{
		Body->SetStaticMesh(M);
		Body->SetRelativeScale3D(FVector(1.f));
		Lid->SetVisibility(false);
	}
	Glow->SetVisibility(bHasRoyalKey);
}

void AChest::Open(AEldoriaCharacter* By)
{
	if (bOpened) return;
	bOpened = true;
	if (AEldoriaGameMode* SaveGM = GetWorld()->GetAuthGameMode<AEldoriaGameMode>()) SaveGM->OnChestOpened(this);
	// without a hinged lid on the photoscanned chest, a burst of golden light sells the opening
	Glow->SetVisibility(true);
	Glow->SetIntensity(9000.f);
	UEldoriaSettings::PlaySound(this, UEldoriaSettings::Get()->ChestSound, GetActorLocation(), 1.f, 0.f);
	AEldoriaGameMode* GM = GetWorld()->GetAuthGameMode<AEldoriaGameMode>();

	const FVector Top = GetActorLocation() + FVector(0.f, 0.f, 120.f);
	for (int32 i = 0; i < Coins; ++i)
	{
		const FVector Offset(FMath::FRandRange(-160.f, 160.f), FMath::FRandRange(-160.f, 160.f), 0.f);
		GetWorld()->SpawnActor<ACoinPickup>(ACoinPickup::StaticClass(), Top + Offset, FRotator::ZeroRotator);
	}
	if (bHasPotion)
	{
		GetWorld()->SpawnActor<AHealthPickup>(AHealthPickup::StaticClass(), Top + GetActorForwardVector() * 150.f, FRotator::ZeroRotator);
	}
	if (GM)
	{
		if (bHasRoyalKey) GM->OnRoyalKeyFound();
		else GM->ShowMessage(FText::FromString(TEXT("Treasure!")), 1.5f);
	}
}

void AChest::RestoreOpened()
{
	bOpened = true;
	OpenAlpha = 1.f;
	Glow->SetVisibility(false);
}

void AChest::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bOpened && OpenAlpha < 1.f)
	{
		OpenAlpha = FMath::Min(1.f, OpenAlpha + DeltaSeconds * 2.f);
		const float Ease = 1.f - FMath::Pow(1.f - OpenAlpha, 3.f);
		Lid->SetRelativeRotation(FRotator(0.f, 0.f, -110.f * Ease));
		Glow->SetIntensity((bHasRoyalKey ? 9000.f : 6000.f) * (1.f - Ease));
	}
}

// ---------- castle gate ----------

ACastleGate::ACastleGate()
{
	PrimaryActorTick.bCanEverTick = true;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Bars = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Bars"));
	RootComponent = Bars;
	if (Cube.Succeeded()) Bars->SetStaticMesh(Cube.Object);
	Bars->SetRelativeScale3D(FVector(5.f, 0.4f, 6.5f));
	Bars->SetMobility(EComponentMobility::Movable);
}

void ACastleGate::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AEldoriaGameMode* GM = GetWorld()->GetAuthGameMode<AEldoriaGameMode>();
	if (!bOpening && GM && GM->HasRoyalKey())
	{
		ACharacter* Hero = UGameplayStatics::GetPlayerCharacter(this, 0);
		if (Hero && FVector::Dist2D(Hero->GetActorLocation(), GetActorLocation()) < OpenDistance)
		{
			bOpening = true;
			GM->ShowMessage(FText::FromString(TEXT("The Royal Key turns. The gate is open!")), 3.f);
		}
	}
	if (bOpening && Lift < LiftHeight)
	{
		const float Step = FMath::Min(LiftHeight - Lift, DeltaSeconds * 160.f);
		Lift += Step;
		AddActorWorldOffset(FVector(0.f, 0.f, Step));
	}
}
