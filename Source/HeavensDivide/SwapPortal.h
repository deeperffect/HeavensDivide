#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SwapPortal.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;
class USoundBase;

/** Cosmetic portal with a real-time lifetime, including during swap slowdown. */
UCLASS(NotBlueprintable, Transient)
class ASwapPortal : public AActor
{
    GENERATED_BODY()
public:
    ASwapPortal();
    void Initialize(UNiagaraSystem* System, float Duration, float OpenDuration = .15f,
        float CloseDuration = .18f, EAxis::Type SqueezeAxis = EAxis::Y, USoundBase* Sound = nullptr, float Volume = 1.f);
    virtual void Tick(float DeltaSeconds) override;
    void ConfigurePolish(float Expansion, UNiagaraSystem* Burst, float SoundDelay, float BurstTimeOffset = 0);
private:
    UPROPERTY() TObjectPtr<UNiagaraSystem> CloseBurst;
    UPROPERTY() TObjectPtr<USoundBase> PendingSound;
    float CloseExpansion = 0, SoundDelaySeconds = 0, SoundVolume = 1;
    float BurstOffset = 0;
    bool bCloseBurstPlayed = false;
    void PlayPendingSound();
    friend class FSwapPortalAnimationTest;
    void AdvancePresentation(float Delta);
    UPROPERTY() TObjectPtr<UNiagaraComponent> Effect;
    float Age = 0, HoldDuration = 0, OpeningDuration = 0, ClosingDuration = 0;
    FVector FullScale = FVector(1);
    EAxis::Type WidthAxis = EAxis::Y;
};
