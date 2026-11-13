#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "EldoriaSaveGame.generated.h"

// Everything needed to resume a run: the hero, quest progress and what has changed in the world.
UCLASS()
class ELDORIA_API UEldoriaSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	// shown in the load list
	UPROPERTY() FDateTime SavedAt;
	UPROPERTY() FString QuestTitle;
	UPROPERTY() float PlayTime = 0.f;

	// hero
	UPROPERTY() FVector HeroLocation = FVector::ZeroVector;
	UPROPERTY() FRotator HeroRotation = FRotator::ZeroRotator;
	UPROPERTY() float Health = 0.f;
	UPROPERTY() int32 Coins = 0;

	// quest
	UPROPERTY() int32 Stage = 0;
	UPROPERTY() bool bHasRoyalKey = false;
	UPROPERTY() int32 Kills = 0;
	UPROPERTY() int32 Deaths = 0;
	// EDifficulty; saves from before difficulty existed load as Medium
	UPROPERTY() uint8 Difficulty = 1;

	// world: names of level actors that are gone or used up
	UPROPERTY() TArray<FString> KilledEnemies;
	UPROPERTY() TArray<FString> OpenedChests;
	UPROPERTY() TArray<FString> CollectedPickups;
};
