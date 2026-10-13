#pragma once
#include "CoreMinimal.h"
#include "EnemyBase.h"
#include "TacticalEnemy.generated.h"
class UAnimMontage;
class AEncounterHazard;
class UStaticMeshComponent;
class AEnemyAttackCue;
UENUM(BlueprintType)
enum class ETacticalEnemyRole : uint8 { AshSeer, HexSniper, MireWeaver, HornLancer, FangStalker, GraveCantor, WarDrummer, OgreWarden, StormGorilla, FrostOracle };

UCLASS(Blueprintable)
class HEAVENSDIVIDE_API ATacticalEnemy : public AEnemyBase
{
    GENERATED_BODY()
public:
    ATacticalEnemy(const FObjectInitializer& Init = FObjectInitializer::Get());
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
    virtual FVector GetVelocity() const override;
    virtual FVector GetEnemyMovementVelocity() const override { return GetVelocity(); }
    virtual void ApplySpawnDifficultyScaling(float Health, float Damage) override;
    virtual void ApplySpawnInstanceModifiers(float Health, float Damage, float Speed) override;
    void InitializeMarch(FVector Direction, float Speed, float Distance, float Delay);
    void GrantHaste(float Duration);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics") ETacticalEnemyRole TacticalRole = ETacticalEnemyRole::AshSeer;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics") FText EnemyName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics") float AbilityDamage = 14;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics") float AbilityCooldown = 4.8f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics") float CastRange = 800;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics") float Windup = 1.05f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics") FLinearColor RoleColor = FLinearColor(1,.25f,.05f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics") TObjectPtr<UAnimMontage> CastMontage;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics|Timing", meta=(ClampMin="0"))
    float InitialCooldownMin = 1.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics|Timing", meta=(ClampMin="0"))
    float InitialCooldownMax = 2.8f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics|Timing", meta=(ClampMin="0.01"))
    float MinimumWindup = 0.75f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics|Timing", meta=(ClampMin="0"))
    float CastRecoverySeconds = 0.45f;
    // Retain serialized values for older assets without exposing obsolete tuning.
    UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Enemies now hold their attack range instead of retreating."))
    float RetreatDistance = 280.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics|Movement", meta=(ClampMin="1"))
    float ChargeSpeed = 950.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics|Charge", meta=(ClampMin="1"))
    float ChargeDistance = 900.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics|Charge", meta=(ClampMin="1"))
    float ChargeWidth = 135.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics|Charge", meta=(ClampMin="0"))
    float ChargeRecoverySeconds = 0.75f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics|Charge", meta=(ClampMin="0.01"))
    float FollowupCooldown = 0.9f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics|Support", meta=(ClampMin="1", ClampMax="64"))
    int32 SupportTargets = 8;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics|Support", meta=(ClampMin="1"))
    float SupportRadius = 420.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics|Support", meta=(ClampMin="0", ClampMax="1"))
    float HealFraction = 0.05f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics|Support", meta=(ClampMin="0"))
    float HasteDuration = 4.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics|Support", meta=(ClampMin="0"))
    float HasteMultiplier = 1.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics|Ash Seer", meta=(ClampMin="1"))
    float SeerRadius = 155.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics|Mire Weaver", meta=(ClampMin="1"))
    float MireRadius = 180.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics|Mire Weaver", meta=(ClampMin="0.01"))
    float MireDuration = 3.4f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics|Frost Oracle", meta=(ClampMin="1", ClampMax="32"))
    int32 FrostCount = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics|Frost Oracle", meta=(ClampMin="0"))
    float FrostSpacing = 210.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics|Frost Oracle", meta=(ClampMin="1"))
    float FrostRadius = 115.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics|Frost Oracle", meta=(ClampMin="0"))
    float FrostStagger = 0.25f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics|Hex Sniper", meta=(ClampMin="1"))
    float SniperLength = 1300.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics|Hex Sniper", meta=(ClampMin="1"))
    float SniperWidth = 110.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics|Hex Sniper", meta=(ClampMin="0.01"))
    float SniperActiveTime = 0.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics|Ogre Warden", meta=(ClampMin="1"))
    float WardenOuterRadius = 410.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics|Ogre Warden", meta=(ClampMin="0"))
    float WardenInnerRadius = 160.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics|Storm Gorilla", meta=(ClampMin="1"))
    float StormLength = 1050.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics|Storm Gorilla", meta=(ClampMin="1"))
    float StormWidth = 155.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics|Storm Gorilla", meta=(ClampMin="0", ClampMax="180"))
    float StormCrossAngle = 90.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tactics|Timing", meta=(ClampMin="0.01"))
    float BurstActiveTime = 0.18f;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> RoleRing;
protected:
    virtual void UpdateEnemyBehavior(float Delta) override;
    virtual bool ShouldSkipMovement() const override;
    virtual bool ShouldForceHighAnimationBudgetSignificance() const override;
    virtual void StopEnemyBehavior() override;
    virtual void HandleDeath() override;
private:
    friend class FEncounterExpansionTest;
    friend class FBossSpellPressureTest;
    float GetEngagementRange() const;
    void BeginAbility();
    void FinishAbility();
    void HitAlongMovement(FVector Before, FVector After);
    float Cooldown = 0, CastRemaining = 0, Recovery = 0, ChargeRemaining = 0, HasteUntil = 0;
    FVector LockedDirection = FVector::ForwardVector;
    FVector LockedTarget = FVector::ZeroVector;
    int32 FollowupCharges = 0;
    bool bChargeHit = false, bMarching = false, bSecondDash = false;
    float MarchDelay = 0, MarchRemaining = 0, MarchSpeed = 300;
    TWeakObjectPtr<AEncounterHazard> ChargeWarning;
    TWeakObjectPtr<AEnemyAttackCue> CastCue;
};
