#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MetaSkillTree.h"
#include "BloodshiftBalanceData.generated.h"

/** Shared rules without an actor Blueprint owner. Loaded once and explicitly cooked. */
UCLASS(BlueprintType)
class HEAVENSDIVIDE_API UBloodshiftBalanceData : public UDataAsset
{
    GENERATED_BODY()
public:
    UBloodshiftBalanceData();
    static const UBloodshiftBalanceData* Get();
    UPROPERTY(EditAnywhere, EditFixedSize, BlueprintReadOnly, Category="Permanent Skill Tree", meta=(TitleProperty="Name"))
    TArray<FMetaSkillNode> Skills;
    /** Additional ceiling for attack-cadence upgrades. Each card's Max Level also applies. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade Limits", meta=(ClampMin="1",ClampMax="100"))
    int32 CadenceUpgradeRankCap=3;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter Hazards", meta=(ClampMin="1",ClampMax="256"))
    int32 MaxActiveHazards=36;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter Hazards", meta=(ClampMin="0.01",Units="s"))
    float MinimumWarning=.6f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter Hazards", meta=(ClampMin="0.01",Units="s"))
    float PersistentDamageInterval=.8f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter Hazards", meta=(ClampMin="0",Units="cm"))
    float HazardVerticalTolerance=240.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Encounter Hazards", meta=(ClampMin="0",ClampMax="1"))
    float PlayerCollisionPaddingFraction=.5f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Soul Ember Rewards", meta=(ClampMin="1",Units="s"))
    float SecondsPerEmber=30.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Soul Ember Rewards", meta=(ClampMin="0",Units="s"))
    float RewardTimeCap=3600.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Soul Ember Rewards", meta=(ClampMin="0"))
    int32 VictoryEmbers=20;
};
