#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Pickups.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class AEldoriaCharacter;

// Base for anything the hero collects by walking into it. Spins, bobs and glides to the hero when close.
UCLASS(Abstract)
class ELDORIA_API APickup : public AActor
{
	GENERATED_BODY()

public:
	APickup();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere) USphereComponent* Trigger;
	UPROPERTY(VisibleAnywhere) UStaticMeshComponent* Mesh;
	UPROPERTY(VisibleAnywhere) UPointLightComponent* Glow;

	UPROPERTY(EditAnywhere, Category = "Pickup") float MagnetRadius = 0.f;
	// tilt kept while spinning (coins stand on edge)
	float SpinPitch = 0.f;
	// float and turn in the air; when false the pickup lies still on the ground until collected
	bool bHover = true;
	float RestHeight = 70.f;

	virtual bool CanCollect(AEldoriaCharacter* Hero) const { return true; }
	virtual void Collect(AEldoriaCharacter* Hero) {}
	void SetLook(class UStaticMesh* Override, const FLinearColor& Color, float Scale, class UMaterialInterface* Material = nullptr);

private:
	UFUNCTION() void OnOverlap(UPrimitiveComponent* Comp, AActor* Other, UPrimitiveComponent* OtherComp, int32 Index, bool bSweep, const FHitResult& Hit);
	void TryCollect(AActor* Other);

	FVector BaseLocation;
	float Phase = 0.f;
	float Age = 0.f;
	bool bCollected = false;
};

UCLASS()
class ELDORIA_API ACoinPickup : public APickup
{
	GENERATED_BODY()

public:
	ACoinPickup();
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, Category = "Pickup") int32 Value = 1;

protected:
	virtual void Collect(AEldoriaCharacter* Hero) override;
};

UCLASS()
class ELDORIA_API AHealthPickup : public APickup
{
	GENERATED_BODY()

public:
	AHealthPickup();
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, Category = "Pickup") float HealAmount = 50.f;

protected:
	virtual bool CanCollect(AEldoriaCharacter* Hero) const override;
	virtual void Collect(AEldoriaCharacter* Hero) override;
};
