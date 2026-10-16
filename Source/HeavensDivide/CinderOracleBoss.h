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
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Pacing", meta=(ClampMin="0"))
    float InitialCastDelay = 2.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Pacing", meta=(ClampMin="0", ClampMax="1"))
    float EnrageHealthFraction = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Pacing", meta=(ClampMin="0.1"))
    float PhaseOneInterval = 5.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Pacing", meta=(ClampMin="0.1"))
    float PhaseTwoInterval = 4.1f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Pacing", meta=(ClampMin="1"))
    float PreferredDistance = 650.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Pacing", meta=(ClampMin="0"))
    float CastRecoverySeconds = 1.8f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Pacing", meta=(ClampMin="0.01"))
    float PhaseOneWarning = 1.35f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Pacing", meta=(ClampMin="0.01"))
    float PhaseTwoWarning = 1.05f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Pacing", meta=(ClampMin="0.01"))
    float BurstActiveTime = 0.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Falling Embers", meta=(ClampMin="1", ClampMax="32"))
    int32 EmberCount = 10;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Falling Embers", meta=(ClampMin="0"))
    float EmberSpacing = 175.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Falling Embers", meta=(ClampMin="1"))
    float EmberRadius = 125.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Falling Embers", meta=(ClampMin="0"))
    float EmberStagger = 0.24f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Lances", meta=(ClampMin="0", ClampMax="180"))
    float LanceSideAngle = 55.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Lances", meta=(ClampMin="1"))
    float LanceLength = 2600.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Lances", meta=(ClampMin="1"))
    float LanceWidth = 340.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Nova", meta=(ClampMin="1"))
    float NovaRadius = 950.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Nova", meta=(ClampMin="0"))
    float NovaSafeRadius = 340.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Nova", meta=(ClampMin="0.01"))
    float NovaWarning = 0.85f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Cursed Ground")
    bool bGroundFillsScreen = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Cursed Ground", meta=(ClampMin="4", ClampMax="128"))
    int32 GroundMaxScreenCircles = 64;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Cursed Ground", meta=(ClampMin="2.1", ClampMax="4"))
    float GroundScreenSpacingMultiplier = 2.25f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Cursed Ground", meta=(ClampMin="0"))
    float GroundScreenSweepSeconds = 0.35f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Cursed Ground", meta=(ClampMin="1", ClampMax="32"))
    int32 GroundCount = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Cursed Ground", meta=(ClampMin="0"))
    float GroundSpreadRadius = 230.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Cursed Ground", meta=(ClampMin="1"))
    float GroundRadius = 150.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Cursed Ground", meta=(ClampMin="0"))
    float GroundStagger = 0.18f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Cursed Ground", meta=(ClampMin="0.01"))
    float GroundDuration = 3.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Summon", meta=(ClampMin="1", ClampMax="16"))
    int32 GuardiansPerCast = 2;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Summon", meta=(ClampMin="1", ClampMax="32"))
    int32 GuardianLimit = 4;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Summon", meta=(ClampMin="0"))
    float GuardianForwardOffset = 280.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Summon", meta=(ClampMin="0"))
    float GuardianSideOffset = 210.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Summon", meta=(ClampMin="1"))
    float SummonBlastRadius = 200.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Summon")
    TArray<TSubclassOf<AEnemyBase>> HordeClasses;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Summon", meta=(ClampMin="0", ClampMax="128"))
    int32 HordePerCast = 20;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Summon", meta=(ClampMin="0", ClampMax="128"))
    int32 HordeLimit = 60;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Summon", meta=(ClampMin="100"))
    float HordeSpawnRadius = 750.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Safety", meta=(ClampMin="36", ClampMax="256"))
    int32 MaxActiveSpellHazards = 128;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Spiral", meta=(ClampMin="1", ClampMax="32"))
    int32 SpiralCount = 20;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Spiral", meta=(ClampMin="0"))
    float SpiralStartRadius = 200.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Spiral", meta=(ClampMin="0"))
    float SpiralRadiusStep = 32.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Spiral", meta=(ClampMin="0", ClampMax="360"))
    float SpiralAngleStep = 55.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Spiral", meta=(ClampMin="1"))
    float SpiralBlastRadius = 100.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Spiral", meta=(ClampMin="0"))
    float SpiralStagger = 0.12f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Eclipse", meta=(ClampMin="1"))
    float EclipseOuterRadius = 10000.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Eclipse", meta=(ClampMin="0"))
    float EclipseInnerRadius = 290.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Eclipse", meta=(ClampMin="0.01"))
    float EclipseWarning = 1.7f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Eclipse", meta=(ClampMin="1"))
    float EclipseLaneLength = 1350.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Oracle|Eclipse", meta=(ClampMin="1"))
    float EclipseLaneWidth = 110.0f;
    UFUNCTION(BlueprintCallable, Category="Oracle|Testing") void CastAbility(int32 Ability);
    UPROPERTY(EditAnywhere, Category="Oracle|Charge") float ChargeSpeed = 3600.f;
    UPROPERTY(EditAnywhere, Category="Oracle|Charge") float ChargeDistance = 1800.f;
    UPROPERTY(EditAnywhere, Category="Oracle|Charge") float ChargeContactDamage = 25.f;
    UPROPERTY(EditAnywhere, Category="Oracle|Charge") float ChargePushSpeed = 900.f;
    UPROPERTY(EditAnywhere, Category="Oracle|Charge") float ChargeTrailDamage = 10.f;
    virtual FVector GetVelocity() const override;
protected:
    virtual void UpdateEnemyBehavior(float Delta) override;
    virtual bool ShouldSkipMovement() const override { return !bOracleActive || CastRecovery>0 || ChargesRemaining>0; }
    virtual void HandleDeath() override;
private:
    TWeakObjectPtr<class AEnemyAttackCue> CastCue;
    friend class FEncounterExpansionTest;
    friend class FBossSpellPressureTest;
    friend class FBossChargeTest;
    void BeginChargeSequence();
    void PrepareCharge(float WarningSeconds);
    void TickCharge(float Delta);
    void EndCharge();
    void CancelCharge();
    void HitChargePlayer(FVector Before, FVector After);
    int32 ChargesRemaining = 0;
    float ChargeWait = 0, ChargeRemainingDistance = 0, TrailDistance = 0;
    bool bCharging = false, bChargeEnraged = false, bChargeHitPlayer = false;
    FVector ChargeDirection = FVector::ForwardVector;
    TWeakObjectPtr<class AEncounterHazard> ChargeWarning;
    bool bOracleActive = false;
    float NextCast = 2, CastRecovery = 0;
    int32 SpellIndex = 0;
    TArray<TWeakObjectPtr<ATacticalEnemy>> Guardians;
    TArray<TWeakObjectPtr<AEnemyBase>> Horde;
    AEnemyBase* SpawnSummon(TSubclassOf<AEnemyBase> Class, FVector Desired);
    void CastScreenGround(FVector Player, float Warning);
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Embers;
};
