#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeath);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDamaged, float, Amount);

UCLASS(ClassGroup = (Eldoria), meta = (BlueprintSpawnableComponent))
class ELDORIA_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHealthComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health") float MaxHealth = 100.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health") float Health = 100.f;

	UPROPERTY(BlueprintAssignable) FOnDeath OnDeath;
	UPROPERTY(BlueprintAssignable) FOnDamaged OnDamaged;

	void Reset(float NewMax);
	float ApplyDamage(float Amount);
	void Heal(float Amount);

	UFUNCTION(BlueprintPure) bool IsDead() const { return Health <= 0.f; }
	UFUNCTION(BlueprintPure) float GetPercent() const { return MaxHealth > 0.f ? Health / MaxHealth : 0.f; }
};
