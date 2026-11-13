#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Chest.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class AEldoriaCharacter;

// Treasure chest opened with the interact key. One of them holds the Royal Key.
UCLASS()
class ELDORIA_API AChest : public AActor
{
	GENERATED_BODY()

public:
	AChest();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "Chest") int32 Coins = 10;
	UPROPERTY(EditAnywhere, Category = "Chest") bool bHasPotion = false;
	UPROPERTY(EditAnywhere, Category = "Chest") bool bHasRoyalKey = false;

	UPROPERTY(VisibleAnywhere) UStaticMeshComponent* Body;
	UPROPERTY(VisibleAnywhere) UStaticMeshComponent* Lid;
	UPROPERTY(VisibleAnywhere) UPointLightComponent* Glow;

	bool IsOpened() const { return bOpened; }
	void Open(AEldoriaCharacter* By);
	// marks the chest as already looted (loading a save)
	void RestoreOpened();

private:
	bool bOpened = false;
	float OpenAlpha = 0.f;
};

// The portcullis that seals the castle until the hero brings the Royal Key.
UCLASS()
class ELDORIA_API ACastleGate : public AActor
{
	GENERATED_BODY()

public:
	ACastleGate();
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere) UStaticMeshComponent* Bars;
	UPROPERTY(EditAnywhere, Category = "Gate") float OpenDistance = 1400.f;
	UPROPERTY(EditAnywhere, Category = "Gate") float LiftHeight = 650.f;

	bool IsOpen() const { return bOpening; }

private:
	bool bOpening = false;
	float Lift = 0.f;
};
