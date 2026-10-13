#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ExperiencePickupDropSettings.generated.h"

/** Consolidate rewards without changing enemy XP or rolling away any XP. */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Experience Pickup Drops"))
class HEAVENSDIVIDE_API UExperiencePickupDropSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    virtual FName GetCategoryName() const override { return TEXT("Game"); }
    UPROPERTY(Config, EditAnywhere, Category="Grouping", meta=(ClampMin="1",ClampMax="16"))
    int32 RewardsPerPickup=4;
    UPROPERTY(Config, EditAnywhere, Category="Grouping", meta=(ClampMin="0",ClampMax="2000",Units="cm"))
    float MergeRadius=500.f;
};
