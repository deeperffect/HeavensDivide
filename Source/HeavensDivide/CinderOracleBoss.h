#pragma once
#include "CoreMinimal.h"
#include "FinalBossBase.h"
#include "CinderOracleBoss.generated.h"
class ATacticalEnemy;
class UStaticMeshComponent;

UCLASS(Blueprintable)
class HEAVENSDIVIDE_API ACinderOracleBoss : public AFinalBossBase
{
    GENERATED_BODY()
public:
    ACinderOracleBoss(const FObjectInitializer& Init = FObjectInitializer::Get());
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
    virtual void StartBossCombat() override;
    virtual void StopBossCombat() override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle") TSubclassOf<ATacticalEnemy> GuardianClass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle") TObjectPtr<UAnimMontage> OracleCastMontage;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle") float SpellDamage = 22;
    UFUNCTION(BlueprintCallable, Category="Oracle|Testing") void CastAbility(int32 Ability);
protected:
    virtual void UpdateEnemyBehavior(float Delta) override;
    virtual bool ShouldSkipMovement() const override { return !bOracleActive || CastRecovery>0; }
    virtual void HandleDeath() override;
private:
    friend class FEncounterExpansionTest;
    bool bOracleActive = false;
    float NextCast = 2, CastRecovery = 0;
    int32 SpellIndex = 0;
    TArray<TWeakObjectPtr<ATacticalEnemy>> Guardians;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Embers;
};
