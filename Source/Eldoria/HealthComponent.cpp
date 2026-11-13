#include "HealthComponent.h"

UHealthComponent::UHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHealthComponent::Reset(float NewMax)
{
	MaxHealth = NewMax;
	Health = NewMax;
}

float UHealthComponent::ApplyDamage(float Amount)
{
	if (IsDead() || Amount <= 0.f)
	{
		return 0.f;
	}
	const float Dealt = FMath::Min(Health, Amount);
	Health -= Dealt;
	OnDamaged.Broadcast(Dealt);
	if (IsDead())
	{
		OnDeath.Broadcast();
	}
	return Dealt;
}

void UHealthComponent::Heal(float Amount)
{
	if (!IsDead())
	{
		Health = FMath::Min(MaxHealth, Health + Amount);
	}
}
