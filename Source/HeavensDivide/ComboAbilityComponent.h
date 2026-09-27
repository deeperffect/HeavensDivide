#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ComboAbilityComponent.generated.h"

class ACharacterBase;
class AEnemyBase;
class UAnimMontage;
class UNiagaraSystem;
class USoundBase;
class UNiagaraComponent;
class UAudioComponent;

USTRUCT(BlueprintType)
struct FComboAbilitySettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combo Ability") FText DisplayName = FText::FromString(TEXT("Combo Ability"));
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combo Ability") bool bEnabled = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combo Ability|Freeze") bool bFreezeOnActivation = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combo Ability|Freeze", meta=(ClampMin="0", Units="s", EditCondition="bFreezeOnActivation", ToolTip="Real-time freeze duration, independent of swapping. Zero disables the freeze.")) float FreezeDuration = .5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combo Ability|Freeze", meta=(ClampMin="0", Units="s", EditCondition="bFreezeOnActivation", ToolTip="Ease into the freeze over this many real seconds, capped at Freeze Duration.")) float FreezeEaseInDuration = .12f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combo Ability|Targeting") bool bFaceEnemyPack = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combo Ability|Targeting", meta=(ClampMin="1", Units="cm", EditCondition="bFaceEnemyPack")) float PackSearchRadius = 1200.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combo Ability|Targeting", meta=(ClampMin="1", Units="cm", EditCondition="bFaceEnemyPack", ToolTip="Enemies within this distance of a candidate enemy count as one pack. Face the largest pack's center; ties prefer the closer pack.")) float PackRadius = 350.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combo Ability|Damage", meta=(ClampMin="0")) float DamagePerPulse = 12.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combo Ability|Damage", meta=(ClampMin="1", Units="cm")) float InitialRadius = 180.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combo Ability|Damage", meta=(ClampMin="1", Units="cm")) float FinalRadius = 650.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combo Ability|Timing", meta=(ClampMin="0.01", Units="s")) float EffectDuration = 3.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combo Ability|Timing", meta=(ClampMin="0.02", Units="s")) float PulseInterval = .2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combo Ability|Presentation") FLinearColor FallbackColor = FLinearColor(1.f, .55f, .08f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combo Ability|Presentation") TObjectPtr<UAnimMontage> Montage;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combo Ability|Presentation", meta=(ClampMin="0.01")) float MontagePlayRate = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combo Ability|Presentation") TObjectPtr<USoundBase> Sound;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combo Ability|Presentation", meta=(ClampMin="0")) float SoundVolume = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combo Ability|Presentation") TObjectPtr<UNiagaraSystem> VFX;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combo Ability|Presentation", meta=(DisplayName="Spawn VFX Every Pulse", ToolTip="Spawn a separate Niagara effect on every damage pulse. Each instance uses VFX Visibility Duration; zero uses Pulse Interval in this mode.")) bool bSpawnVFXEveryPulse = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combo Ability|Presentation") FVector VFXOffset = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combo Ability|Presentation") FRotator VFXRotation = FRotator::ZeroRotator;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combo Ability|Presentation") FVector VFXScale = FVector::OneVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combo Ability|Presentation") FName VFXRadiusParameter = TEXT("User.Radius");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combo Ability|Presentation", meta=(ClampMin="0.01", Units="cm", ToolTip="Radius represented by a Niagara parameter value of 1. Use 1 for a radius in centimeters, or the effect's reference radius for a scale parameter such as User.Scale_All.")) float VFXReferenceRadius = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combo Ability|Presentation") FName VFXDurationParameter = TEXT("User.Duration");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combo Ability|Presentation", meta=(ClampMin="0", Units="s", ToolTip="How long each VFX instance emits from its spawn time. Zero uses Pulse Interval when spawning every pulse, otherwise Effect Duration. Existing particles may finish afterward; cannot extend a Niagara system that ends itself earlier.")) float VFXVisibilityDuration = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combo Ability|Presentation", meta=(ClampMin="0", Units="s", ToolTip="Maximum extra time for particles to finish after emission stops. Increase for long particle lifetimes.")) float VFXCompletionTimeout = 5.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combo Ability|Timing", meta=(ClampMin="0", Units="s", ToolTip="Delay from activation until gameplay, sound and VFX execute.")) float EffectDelay = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combo Ability|Timing", meta=(ClampMin="0.01", Units="s", ToolTip="Minimum recovery. Extended to cover the effect delay and montage length.")) float RecoveryDuration = .6f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FComboMeterChanged, float, Current, float, Maximum, float, Percent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FComboAbilityActivated, ACharacterBase*, Character);

/** Shared run resource; character Blueprints provide their own ability settings and execution. */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class HEAVENSDIVIDE_API UComboAbilityComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UComboAbilityComponent();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combo|Meter", meta=(ClampMin="1")) float MaxCombo = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combo|Meter", meta=(ClampMin="0")) float SwapGain = 20.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combo|Meter", meta=(ClampMin="0")) float SynergyGain = 25.f;
    UPROPERTY(BlueprintAssignable, Category="Combo") FComboMeterChanged OnComboChanged;
    UPROPERTY(BlueprintAssignable, Category="Combo") FComboAbilityActivated OnAbilityActivated;
    UFUNCTION(BlueprintPure, Category="Combo") float GetCombo() const { return CurrentCombo; }
    UFUNCTION(BlueprintPure, Category="Combo") float GetMaxCombo() const;
    UFUNCTION(BlueprintPure, Category="Combo") float GetComboPercent() const;
    UFUNCTION(BlueprintPure, Category="Combo") bool IsFull() const;
    UFUNCTION(BlueprintPure, Category="Combo") bool IsAbilityActive() const { return AbilityCharacter.IsValid(); }
    UFUNCTION(BlueprintPure, Category="Combo") bool CanActivateAbility() const;
    UFUNCTION(BlueprintCallable, Category="Combo") bool TryActivateAbility();
    // Future synergy attacks call this once when successfully triggered, never once per target hit.
    UFUNCTION(BlueprintCallable, Category="Combo") void NotifySynergyAttack();
    UFUNCTION(BlueprintCallable, Category="Combo") void AddCombo(float Amount);
    void RestoreCombo(float Amount);
private:
    friend class FComboAbilityTest;
    friend class FComboAbilityVFXTest;
    void UpdateVFXRadius(float Radius);
    void UpdateVFXLifetime(float DeltaTime);
    struct FPendingVFX
    {
        TWeakObjectPtr<UNiagaraComponent> Effect;
        TWeakObjectPtr<ACharacterBase> Character;
        float Remaining = 0;
        float CompletionRemaining = 0;
        bool bStopping = false;
    };
    TArray<FPendingVFX> PendingVFX;
    bool CanRun() const;
    void SetCombo(float Amount);
    void ExecuteEffect();
    bool FaceEnemyPack(ACharacterBase* Character);
    void FinishAbility(bool bCancelled);
    UFUNCTION() void HandleSwap(ACharacterBase* OldCharacter, ACharacterBase* NewCharacter);
    UFUNCTION() void HandleAssist(ACharacterBase* Assistant, ACharacterBase* Active, AEnemyBase* Target);
    UPROPERTY(Transient) float CurrentCombo = 0.f;
    UPROPERTY(Transient) FComboAbilitySettings ActiveSettings;
    UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> ActiveVFX;
    UPROPERTY(Transient) TObjectPtr<UAudioComponent> ActiveAudio;
    TWeakObjectPtr<ACharacterBase> AbilityCharacter;
    float AbilityElapsed = 0.f;
    float AbilityDuration = 0.f;
    float NextPulseTime = 0.f;
    bool bOwnsFacingOverride = false;
    bool bEffectExecuted = false;
    bool bOwnsActivationFreeze = false;
};
