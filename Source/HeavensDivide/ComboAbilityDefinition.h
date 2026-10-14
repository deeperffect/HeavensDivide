#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UpgradeDefinition.h"
#include "ComboAbilityDefinition.generated.h"

class UNiagaraSystem;
class UTexture2D;
class UStaticMesh;

UENUM(BlueprintType)
enum class EComboTechnique : uint8
{
    None = 0 UMETA(Hidden),
    // Keep saved numeric values stable after removing the retired solo techniques.
    CrimsonTempest = 10, EclipseSeverance = 11, LunarTempest = 12
};

/** One combined attack. Stable IDs also identify unlocks and damage reports. */
UCLASS(BlueprintType)
class HEAVENSDIVIDE_API UComboAbilityDefinition : public UDataAsset
{
    GENERATED_BODY()
public:
    bool IsJoint() const { return Technique == EComboTechnique::CrimsonTempest || Technique == EComboTechnique::EclipseSeverance || Technique == EComboTechnique::LunarTempest; }
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName AbilityId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(MultiLine=true)) FText Description;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText StanceHint;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EUpgradeStatTarget Character = EUpgradeStatTarget::SharedPlayer;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) EComboTechnique Technique = EComboTechnique::CrimsonTempest;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Targeting", meta=(ClampMin="1", Units="cm")) float PackSearchRadius = 1200.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Targeting", meta=(ClampMin="1", Units="cm")) float PackRadius = 350.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Balance", meta=(ClampMin="1")) float Radius = 450.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Balance", meta=(ClampMin="1")) float Range = 1100.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Balance", meta=(ClampMin="0.05")) float Duration = 3.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Balance", meta=(ClampMin="1", ClampMax="16")) int32 Count = 8;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation") TObjectPtr<UNiagaraSystem> VFX;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation") TObjectPtr<UTexture2D> Icon;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation") TObjectPtr<UStaticMesh> WeaponMesh;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation") FLinearColor Color = FLinearColor(1.f,.65f,.15f);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation", meta=(ClampMin="0.01")) float VFXScale = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation") FRotator VFXRotation;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation", meta=(ToolTip="Offset in the cast's forward/right/up axes, relative to the effect's ground anchor.")) FVector VFXOffset = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation", meta=(ClampMin="0.05")) float VFXSeconds = .6f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation", meta=(ToolTip="Override exposed Niagara color parameters. Disable to keep authored Niagara colors.")) bool bTintVFX = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation", meta=(ClampMin="0", Units="s")) float VFXCompletionSeconds = 2.f;
    /** Ground footprint of the authored slash at Niagara component scale 1. Component VFX scale and area upgrades multiply both. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo|Blood Slash", meta=(ClampMin="1", Units="cm")) float BloodSlashVFXRadius = 234.f;
    /** Shared anchor for the effect and damage, in the cast's forward/right/up axes. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo|Blood Slash") FVector BloodSlashAnchor = FVector(160,0,0);
    /** Multipliers on the corresponding native attack, rather than a second copy of route balance. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo", meta=(ClampMin="0")) float SamuraiPower = 4.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo", meta=(ClampMin="0")) float NinjaPower = 3.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo", meta=(ClampMin="1")) float AreaMultiplier = 1.6f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo", meta=(ClampMin="0.1")) float Windup = .65f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo", meta=(ClampMin="0.1")) float WaveInterval = .65f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo", meta=(ClampMin="1", ToolTip="Lunar wheel size multiplier, applied on top of native Great Shuriken size upgrades. Scales both the visible wheel and its hit radius.")) float ShurikenSizeMultiplier = 1.5f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo", meta=(ClampMin="1")) float FangSpeedMultiplier = 4.f;
    /** Animated follow-through after the release; no idle hold at the end of the montage. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo", meta=(ClampMin="0.2", ClampMax="0.65")) float ReleaseRecovery = .42f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo", meta=(ClampMin="0.08", ClampMax="0.5")) float FangThrowInterval = .14f;
    /** World speed during charge; eases back to normal during release. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo", meta=(ClampMin="0.03", ClampMax="1")) float CinematicWorldSpeed = .1f;
    /** Attack animation, native projectiles and Niagara share this release acceleration. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo|Timing", meta=(ClampMin="0.1", ClampMax="1")) float ReleaseStartSpeed = .35f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo|Timing", meta=(ClampMin="0.01", Units="s")) float ReleaseRampSeconds = .28f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo|Timing", meta=(ClampMin="0.1", ClampMax="0.7", ToolTip="Fraction of charge used by the camera push. The helper appears in smoke after this point.")) float HelperArrivalFraction = .35f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo|Presentation|Helper", meta=(ToolTip="Optional override. Empty uses the helping character's Character Smoke VFX on SwapPresentation (red Samurai / purple Ninja).")) TObjectPtr<UNiagaraSystem> HelperArrivalVFX;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo|Presentation|Helper", meta=(ToolTip="Optional override. Empty uses the helping character's Character Smoke VFX on SwapPresentation (red Samurai / purple Ninja).")) TObjectPtr<UNiagaraSystem> HelperDepartureVFX;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo|Presentation|Helper", meta=(ClampMin="0.01")) float HelperSmokeScale = .125f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo|Presentation|Helper") FVector HelperSmokeOffset = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo|Presentation|Helper") FRotator HelperSmokeRotation;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo|Presentation|Helper", meta=(ClampMin="0.05", Units="s")) float HelperSmokeSeconds = .65f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo|Presentation|Impact") TObjectPtr<UNiagaraSystem> HitVFX;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo|Presentation|Impact", meta=(ClampMin="0.01")) float HitVFXScale = .8f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo|Presentation|Impact") FVector HitVFXOffset = FVector(0,0,25);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo|Presentation|Impact") FRotator HitVFXRotation;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo|Presentation|Impact", meta=(ClampMin="0.05", Units="s")) float HitVFXSeconds = .12f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo|Presentation|Impact", meta=(ClampMin="0", Units="s", ToolTip="Minimum spacing of cosmetic bursts per enemy; never limits damage.")) float HitVFXInterval = .08f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo|Presentation") TObjectPtr<class UAnimMontage> SamuraiMontage;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo|Presentation") TObjectPtr<class UAnimMontage> NinjaMontage;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo|Presentation", meta=(ToolTip="Eclipse alternates this with Ninja Montage on each successful throw. Falls back to Ninja's Alternate Attack Montage.")) TObjectPtr<class UAnimMontage> NinjaAlternateMontage;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo|Presentation") TObjectPtr<UNiagaraSystem> ChargeVFX;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo|Presentation|Charge", meta=(ClampMin="0.01")) float ChargeVFXScale = .22f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo|Presentation|Charge") FVector ChargeVFXOffset = FVector(70,0,0);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo|Presentation|Charge") FRotator ChargeVFXRotation;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo|Presentation|Charge") FLinearColor ChargeVFXColor = FLinearColor(.3f,.65f,2.f);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo|Presentation|Charge", meta=(ClampMin="0", ToolTip="Zero emits until charge completes; positive values cap emission within windup.")) float ChargeVFXSeconds = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo|Presentation") TObjectPtr<UNiagaraSystem> SecondaryVFX;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo|Presentation|Secondary", meta=(ClampMin="0.01")) float SecondaryVFXScale = .45f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo|Presentation|Secondary") FVector SecondaryVFXOffset = FVector(100,0,0);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo|Presentation|Secondary") FRotator SecondaryVFXRotation;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo|Presentation|Secondary") FLinearColor SecondaryVFXColor = FLinearColor(.15f,2.f,.7f);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo|Presentation|Secondary", meta=(ClampMin="0.05", Units="s")) float SecondaryVFXSeconds = .3f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo|Presentation", meta=(ClampMin="0.01", ToolTip="Eclipse Fang visual scale only; does not change damage or collision.")) float FangPresentationScale = .3f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo|Presentation", meta=(ToolTip="Optional native slash accent at Lunar's infusion point, in addition to its assigned VFX.")) bool bEnableInfusionAccent = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo|Presentation") TObjectPtr<class USoundBase> ReleaseSound;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Joint Combo|Presentation", meta=(ClampMin="0")) float ReleaseSoundVolume = .8f;
};

namespace ComboCatalog
{
    struct FEntry { FName Id; const TCHAR* Name; bool bSamurai; const TCHAR* Description; const TCHAR* Stance; };
    HEAVENSDIVIDE_API const TArray<FEntry>& Entries();
    HEAVENSDIVIDE_API const FEntry* Find(FName Id);
    HEAVENSDIVIDE_API FName UnlockId(FName Id);
    HEAVENSDIVIDE_API UComboAbilityDefinition* Definition(FName Id);
}
