#pragma once
#include "CoreMinimal.h"
#include "BuildFamilyCatalog.h"
#include "Components/ActorComponent.h"
#include "EnemyBase.h"
#include "GameFramework/Actor.h"
#include "UpgradePresentation.h"
#include "SurvivorAbilityComponent.generated.h"
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class ACharacterBase;
class ASurvivorPlayerController;
class UPlayerUpgradeComponent;
class UUpgradeDefinition;
class UNiagaraComponent;

/** Short-lived, collision-free ability rings and connecting streaks. */
UCLASS()
class HEAVENSDIVIDE_API AAbilityAccent : public AActor
{
    GENERATED_BODY()
  public:
    AAbilityAccent();
    virtual void Tick(float Delta) override;
    void MoveAnchor(FVector Position)
    {
        SetActorLocation(Position + PresentationOffset);
    }
    void SetRemainingLifetime(float Seconds)
    {
        Lifetime = Age + FMath::Max(0.05f, Seconds);
        SetLifeSpan(Seconds + 0.1f);
    }
    void Initialize(FVector End, float Radius, FLinearColor Color, float Duration, bool bBeam,
                    const FUpgradePresentation *Settings = nullptr, int32 Stage = 0);

  private:
    UPROPERTY() TObjectPtr<UNiagaraComponent> Niagara;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Visual;
    UPROPERTY() TObjectPtr<UMaterialInterface> RingMaterial;
    UPROPERTY() TObjectPtr<UMaterialInterface> BeamMaterial;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Material;
    float Age = 0, Lifetime = 0.3f;
    FName NiagaraStartParameter, NiagaraEndParameter;
    FVector NiagaraLocalEnd = FVector::ZeroVector;
    FVector PresentationOffset = FVector::ZeroVector;
    FVector InitialScale = FVector::OneVector;
    FName FallbackIntensityParameter = TEXT("Intensity");
    bool bFadeFallback = true;
    bool bIsBeam = false;
};

/** Blade Wave hit effects, preparation and Tag Team assists. */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class HEAVENSDIVIDE_API USurvivorAbilityComponent : public UActorComponent
{
    friend class FMetaSkillTreeTest;
    GENERATED_BODY()
    friend class FSurvivorAbilitiesTest;
    friend class FBuildFamiliesTest;
    friend class FUpgradeTuningTest;

  public:
    USurvivorAbilityComponent();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    // Called by the existing attack notify path, never by swap or passive pulses.
    bool ExecuteSetupAssist(ACharacterBase *Character);
    bool HasTriggerablePreparation(EPlayerAttackSource Source, const AEnemyBase *Enemy) const;
    void PrioritizePreparedTargets(EPlayerAttackSource Source, TArray<AEnemyBase *> &Targets) const;
    void NotifyPartnerHit(EPlayerAttackSource Source, AEnemyBase *Enemy, bool bAssistHit = false,
                          uint8 StatusBeforeHit = 255);
    void RegisterFamilyHit(int32 Family, AEnemyBase *Enemy, float Damage);
    void BladeWaveImpact(AEnemyBase *Enemy, float Damage, bool bSplinter);
    void GrantBuildPreview(FString FamilyId, int32 Branch = 0);
    float Tuning(int32 Family, FName Key, float Fallback, int32 Slot = -1) const;
    UUpgradeDefinition *TuningDefinition(int32 Family, int32 Slot = -1) const;
    FBuildFamilySpec FamilySpec(int32 Family) const;
    void ApplyConfiguredStatus(int32 Family, AEnemyBase *Enemy, EEnemyStatusEffect Status, EPlayerAttackSource Source);
    const FUpgradePresentation *FamilyPresentation(int32 Family, int32 Slot = -1) const;
    AAbilityAccent *FamilyAccent(int32 Family, FVector Position, FVector End, float Radius, FLinearColor Color,
                                 float Duration = 0.35f, bool bBeam = false, int32 Slot = -1, int32 Stage = 0);

    UPROPERTY(EditAnywhere, Category = "Synergies|Prepare", meta = (ClampMin = "0.1"))
    float PreparationDuration = 6.0f;
    UPROPERTY(EditAnywhere, Category = "Synergies|Prepare", meta = (ClampMin = "0.0"))
    float PreparationDamageMultiplier = 0.6f;
    UPROPERTY(EditAnywhere, Category = "Synergies|Prepare", meta = (ClampMin = "0.0"))
    float PreparationSpreadRadius = 300.0f;

  private:
    struct FBuildMark
    {
        TWeakObjectPtr<AEnemyBase> Enemy;
        EPlayerAttackSource Source = EPlayerAttackSource::Other;
        float Damage = 0, Remaining = 6;
    };
    void UpdateBuildFamilies(ACharacterBase *Character);
    bool Branch(int32 Family, int32 Index) const;
    float BuildMagnitude(int32 Family, const TCHAR *Suffix) const;
    void ClearBuildFamilies();
    TArray<FBuildMark> BuildMarks;
    bool bResolvingReaction = false;
    void UpdateAbilities();
    TArray<AEnemyBase *> FindEnemies(FVector Position, float Radius, EPlayerAttackSource Source) const;
    AAbilityAccent *Accent(FVector Position, FVector End, float Radius, FLinearColor Color, float Duration = 0.35f,
                           bool bBeam = false, const FUpgradePresentation *Settings = nullptr, int32 Stage = 0);
    float Power(ACharacterBase *Character) const;
    mutable TMap<FName, TWeakObjectPtr<UUpgradeDefinition>> TuningCache;
    TMap<FName, double> LastVisualSpawn;
    UPROPERTY() TObjectPtr<ASurvivorPlayerController> Controller;
    UPROPERTY() TObjectPtr<UPlayerUpgradeComponent> Upgrades;
    TArray<TWeakObjectPtr<AAbilityAccent>> ActiveAccents;
    FTimerHandle Scheduler;
};
