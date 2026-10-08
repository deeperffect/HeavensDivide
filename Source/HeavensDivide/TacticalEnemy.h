#pragma once
#include "CoreMinimal.h"
#include "EnemyBase.h"
#include "TacticalEnemy.generated.h"
class UAnimMontage;
class AEncounterHazard;
class UStaticMeshComponent;
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
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> RoleRing;
protected:
    virtual void UpdateEnemyBehavior(float Delta) override;
    virtual bool ShouldSkipMovement() const override;
    virtual bool ShouldForceHighAnimationBudgetSignificance() const override;
    virtual void StopEnemyBehavior() override;
    virtual void HandleDeath() override;
private:
    friend class FEncounterExpansionTest;
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
};
