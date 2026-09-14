#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Containers/Ticker.h"
#include "CharacterBase.h"
#include "SwapPresentationComponent.generated.h"
class UNiagaraSystem;
class UNiagaraComponent;
class USoundBase;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UMeshComponent;
class UAutoAttackComponent;

UCLASS(ClassGroup=(Presentation), meta=(BlueprintSpawnableComponent))
class HEAVENSDIVIDE_API USwapPresentationComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USwapPresentationComponent();
    void PlayDeparture();
    void PrepareArrival() { bArrivalPending = true; }
    void PlayArrival();
    void FinishSwapFreeze(bool bCancelEntrance = true);
    bool IsBlockingAttacks() const { return bArrivalPending || bSwapFreezeActive || EntranceRemaining > 0 || bArrivalMovementActive; }
    bool IsSwapFreezeActive() const { return bSwapFreezeActive; }
    void HandleModeChanged(ECharacterMode Mode);
    virtual void TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* Function) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swap|General") bool bEnabled = true;
    /** Real seconds. Zero disables the freeze; the entrance still has attack priority. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swap|Timing", meta=(ClampMin="0", ClampMax="1", Units="s")) float SwapFreezeDuration = .5f;
    /** Included in Swap Freeze Duration, not added to it. Zero gives an instant freeze. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swap|Timing", meta=(ClampMin="0", ClampMax="1", Units="s")) float FreezeEaseInDuration = .12f;
    // Keep the serialized name so existing Blueprint enable/disable choices survive.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swap|Arrival Drop", meta=(DisplayName="Enable Arrival Drop", ToolTip="Ninja's visual arrival drop. Samurai uses the separate Walk In settings.")) bool bEnableNinjaArrivalDrop = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swap|Arrival Drop", meta=(ClampMin="0", Units="cm")) float ArrivalDropHeight = 180;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swap|Arrival Drop", meta=(ClampMin="0.01", Units="s")) float ArrivalDropDuration = .35f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swap|Samurai Walk In") bool bEnableSamuraiWalkIn = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swap|Samurai Walk In", meta=(ClampMin="0", Units="cm", ToolTip="Start this far behind the character's facing direction and walk visually into position.")) float ArrivalWalkDistance = 180;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swap|Samurai Walk In", meta=(ClampMin="0.01", Units="s", ToolTip="Used when no entrance montage plays. With a montage, walking lasts its full playback duration.")) float ArrivalWalkDuration = .6f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swap|Samurai Weapon Draw") bool bEnableArrivalWeaponDraw = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swap|Samurai Weapon Draw") FName WeaponBackSocket = TEXT("WeaponBackSocket");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swap|Samurai Weapon Draw", meta=(ToolTip="Optional component override. Empty uses the autoattack weapon component.")) FName ArrivalWeaponComponentName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swap|Samurai Weapon Draw", meta=(ClampMin="0", Units="s", ToolTip="Fallback time on the montage timeline. A montage notify named SwapDrawWeapon takes precedence. Playback rate changes automatically preserve timing.")) float ArrivalWeaponDrawTime = .5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swap|Samurai Weapon Draw", meta=(ToolTip="Weapon offset relative to the back socket. Scale multiplies the weapon's normal relative scale.")) FTransform WeaponBackOffset = FTransform::Identity;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swap|General") bool bUseCharacterColor = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swap|General") FLinearColor CustomColor = FLinearColor(1,.12f,.08f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swap|Afterimage", meta=(ClampMin="0.01", Units="s")) float AfterimageDuration = .2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swap|Afterimage") bool bEnableAfterimage = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swap|Afterimage") TObjectPtr<UMaterialInterface> GhostMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swap|VFX") TObjectPtr<UNiagaraSystem> DepartureVFX;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swap|VFX") TObjectPtr<UNiagaraSystem> ArrivalVFX;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swap|VFX") FVector VFXOffset = FVector(0,0,-80);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swap|VFX", meta=(ClampMin="0.01")) float VFXScale = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swap|VFX") bool bUseFallbackArrivalRing = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swap|Sound") bool bEnableSound = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swap|Sound") TObjectPtr<USoundBase> SwapWhoosh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swap|Sound") TObjectPtr<USoundBase> ArrivalSound;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swap|Sound", meta=(ClampMin="0", ClampMax="2")) float SoundVolume = .65f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swap|Animation", meta=(ToolTip="Optional full-body, in-place montage. Plays exclusively to its final frame; gameplay notifies and root motion are not used.")) TObjectPtr<UAnimMontage> EntranceMontage;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swap|Animation", meta=(ClampMin="0.01", ToolTip="Real-time playback multiplier, combined with the montage's Rate Scale. The freeze duration does not accelerate or truncate the entrance.")) float EntrancePlayRate = 1;
    // Retained only for loading existing assets; no longer limits playback.
    UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Entrance duration follows montage length and play rate.")) float EntranceMaxDuration = .25f;
    /** Cosmetic departure played on a temporary visual copy, independently of Tag Team. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swap|Animation") TObjectPtr<UAnimMontage> DepartureMontage;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swap|Animation", meta=(ClampMin="0.01", ToolTip="Real-time playback multiplier, combined with the montage Rate Scale. Independent of freeze duration.")) float DeparturePlayRate = 1;
    // Retained only for loading existing assets; no longer limits playback.
    UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Departure duration follows montage length and play rate.")) float DepartureMaxDuration = .5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swap|Grand Entrance") bool bShowReadyGlow = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swap|Grand Entrance") TObjectPtr<UNiagaraSystem> ReadyVFX;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Swap|Grand Entrance", meta=(ToolTip="Optional weapon mesh component name; empty uses the autoattack weapon visual.")) FName WeaponComponentName;
    UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Assists preserve original materials.")) bool bGhostAssist = false;
    FLinearColor GetPresentationColor() const;
private:
    friend class FSwapFreezeTest;
    friend class FSwapEntranceTest;
    friend class FSwapWeaponDrawTest;
    void StartEntranceWeaponDraw();
    void UpdateEntranceWeaponDraw();
    void RestoreEntranceWeapon();
    TWeakObjectPtr<USceneComponent> EntranceWeapon;
    TWeakObjectPtr<USceneComponent> OriginalWeaponParent;
    FName OriginalWeaponSocket;
    FTransform OriginalWeaponTransform = FTransform::Identity;
    float ActiveWeaponDrawTime = 0;
    void StartEntrance();
    void UpdateEntrance(float RealDelta);
    bool bEntranceOwnsPose = false;
    bool bPreviousMeshTickEnabled = true;
    uint8 PreviousAnimationMode = 0;
    float EntrancePosition = 0;
    float EntranceRate = 1;
    void StartArrivalMovement();
    void UpdateArrivalMovement(float Delta);
    void ResetArrivalMovement();
    FVector ArrivalBaseLocation = FVector::ZeroVector;
    FVector ArrivalStartOffset = FVector::ZeroVector;
    bool bArrivalWalking = false;
    float ArrivalMovementElapsed = 0;
    float ActiveMovementDuration = 0;
    bool bArrivalMovementActive = false;
    float FreezeTotalDuration = 0;
    float ActiveFreezeEaseInDuration = 0;
    void StartSwapFreeze();
    bool UpdateSwapFreeze(float RealDelta);
    void UpdateFreezeAnimationRate(float GameDelta, float RealDelta);
    FTSTicker::FDelegateHandle FreezeTicker;
    float FreezeRemaining = 0;
    float PreviousTimeDilation = 1;
    float AppliedTimeDilation = 1;
    float PreviousAnimationRate = 1;
    bool bSwapFreezeActive = false;
    bool bArrivalPending = false;
    TArray<TWeakObjectPtr<UNiagaraComponent>> FreezeEffects;
    UPROPERTY() TObjectPtr<USoundBase> DefaultSamuraiSound;
    UPROPERTY() TObjectPtr<USoundBase> DefaultNinjaSound;
    void ClearReady();
    void StopEntrance();
    void SpawnEffect(UNiagaraSystem* System);
    UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> ReadyEffect;
    TWeakObjectPtr<UAutoAttackComponent> Attack;
    float EntranceRemaining = 0;
    bool bReadyVisible = false;
};
