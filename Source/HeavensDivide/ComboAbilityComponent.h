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

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FComboMeterChanged, float, Current, float, Maximum, float, Percent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FComboAbilityActivated, ACharacterBase*, Character);

/** Shared run resource for the three combined attacks. */
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
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combo|Meter", meta=(ClampMin="0", ToolTip="Meter gained once per committed inactive-character assist, independent of target count.")) float AssistGain = 10.f;
    UPROPERTY(BlueprintAssignable, Category="Combo") FComboMeterChanged OnComboChanged;
    UPROPERTY(BlueprintAssignable, Category="Combo") FComboAbilityActivated OnAbilityActivated;
    UFUNCTION(BlueprintPure, Category="Combo") float GetCombo() const { return CurrentCombo; }
    UFUNCTION(BlueprintPure, Category="Combo") float GetMaxCombo() const;
    UFUNCTION(BlueprintPure, Category="Combo") float GetComboPercent() const;
    UFUNCTION(BlueprintPure, Category="Combo") bool IsFull() const;
    UFUNCTION(BlueprintPure, Category="Combo") bool IsComboUnlocked() const;
    UFUNCTION(BlueprintPure, Category="Combo") bool IsComboEquipped() const;
    UFUNCTION(BlueprintPure, Category="Combo") bool IsAbilityActive() const { return AbilityCharacter.IsValid(); }
    UFUNCTION(BlueprintPure, Category="Combo") bool CanActivateAbility() const;
    FName GetActiveAbilityId() const;
    FText GetActiveAbilityName() const;
    UFUNCTION(BlueprintCallable, Category="Combo") bool TryActivateAbility();
    // Future synergy attacks call this once when successfully triggered, never once per target hit.
    UFUNCTION(BlueprintCallable, Category="Combo") void NotifySynergyAttack();
    UFUNCTION(BlueprintCallable, Category="Combo") void AddCombo(float Amount);
    void RestoreCombo(float Amount);
private:
    friend class FComboAbilityTest;
    friend class FComboAbilityVFXTest;
    friend class FComboTechniquesTest;
    bool CanRun() const;
    void SetCombo(float Amount);
    bool FaceEnemyPack(ACharacterBase* Character);
    void FinishAbility(bool bCancelled);
    UFUNCTION() void HandleSwap(ACharacterBase* OldCharacter, ACharacterBase* NewCharacter);
    UFUNCTION() void HandleAssist(ACharacterBase* Assistant, ACharacterBase* Active, AEnemyBase* Target);
    UPROPERTY(Transient) float CurrentCombo = 0.f;
    UPROPERTY(Transient) TObjectPtr<class UComboAbilityDefinition> ActiveTechnique;
    TWeakObjectPtr<ACharacterBase> AbilityCharacter;
    TWeakObjectPtr<class AJointComboActor> ExecutingJoint;
    friend class FJointComboTest;
    float AbilityDuration = 0.f;
    bool bOwnsFacingOverride = false;
};
