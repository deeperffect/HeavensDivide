#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "HealingPickupDropSettings.generated.h"

class AHealingPickup;

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Healing Pickup Drops"))
class HEAVENSDIVIDE_API UHealingPickupDropSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	UPROPERTY(Config, EditAnywhere, Category="Pickup")
	TSoftClassPtr<AHealingPickup> HealingPickupClass;

	UPROPERTY(Config, EditAnywhere, Category="Pickup", meta=(ClampMin="0.0", ClampMax="1.0"))
	float NormalDropChance = 0.015f;

	UPROPERTY(Config, EditAnywhere, Category="Pickup", meta=(ClampMin="0.0", ClampMax="1.0"))
	float EliteDropChance = 0.20f;

	UPROPERTY(Config, EditAnywhere, Category="Pickup", meta=(ClampMin="0.0", ClampMax="1.0"))
	float BossDropChance = 1.0f;

	UPROPERTY(Config, EditAnywhere, Category="Limits", meta=(ClampMin="0.0", Units="s"))
	float HealDropCooldown = 20.0f;

	UPROPERTY(Config, EditAnywhere, Category="Limits", meta=(ClampMin="0"))
	int32 MaximumActivePickups = 2;

	UPROPERTY(Config, EditAnywhere, Category="Assistance", meta=(ClampMin="0.0", ClampMax="1.0"))
	float LowHealthThreshold = 0.40f;

	UPROPERTY(Config, EditAnywhere, Category="Assistance", meta=(ClampMin="1.0"))
	float LowHealthDropMultiplier = 2.5f;

	UPROPERTY(Config, EditAnywhere, Category="Pity", meta=(ClampMin="0.0", ClampMax="1.0"))
	float HealPityHealthThreshold = 0.50f;

	UPROPERTY(Config, EditAnywhere, Category="Pity", meta=(ClampMin="0.0", Units="s"))
	float HealPityDelay = 60.0f;

	UPROPERTY(Config, EditAnywhere, Category="Pity", meta=(ClampMin="1.0"))
	float HealPityMultiplier = 4.0f;

	UPROPERTY(Config, EditAnywhere, Category="Spawning", meta=(Units="cm"))
	float SpawnVerticalOffset = 30.0f;
};
