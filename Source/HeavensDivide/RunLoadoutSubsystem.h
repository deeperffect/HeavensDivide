#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RunLoadoutSubsystem.generated.h"

class UPlayerUpgradeComponent;

/** Remembers the confirmed pair across map travel and retries, without storing run upgrades. */
UCLASS()
class HEAVENSDIVIDE_API URunLoadoutSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    bool SetStances(FName Samurai, FName Ninja);
    bool ApplyToFreshRun(UPlayerUpgradeComponent* Upgrades) const;
    FName GetSamuraiStance() const { return SamuraiStance; }
    FName GetNinjaStance() const { return NinjaStance; }
private:
    UPROPERTY(Transient) FName SamuraiStance = TEXT("BattleStance");
    UPROPERTY(Transient) FName NinjaStance = TEXT("BarrageStance");
};
