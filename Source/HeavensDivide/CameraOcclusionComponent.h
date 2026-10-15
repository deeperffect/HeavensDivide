#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CameraOcclusionComponent.generated.h"

class UPrimitiveComponent;
class UCameraComponent;

/** Dither only prepared scenery; no material instances or per-frame scene scans. */
UCLASS(ClassGroup=Camera)
class HEAVENSDIVIDE_API UCameraOcclusionComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UCameraOcclusionComponent();
    virtual void TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* Tick) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera Occlusion") bool bEnabled = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera Occlusion", meta=(ClampMin="0.05")) float CheckInterval = 0.1f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera Occlusion", meta=(ClampMin="1")) float ProbeRadius = 65.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera Occlusion", meta=(ClampMin="0.01")) float FadeOutSeconds = 0.18f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera Occlusion", meta=(ClampMin="0.01")) float RestoreSeconds = 0.3f;
    UPROPERTY(BlueprintReadOnly, Category="Camera Occlusion") int32 FadingObjectCount = 0;
    UPROPERTY(BlueprintReadOnly, Category="Camera Occlusion") int32 OcclusionChecks = 0;
    static constexpr int32 FadeDataIndex = 31;
private:
    struct FFadeState { float Amount = 0.f; double LastSeen = -1.0; };
    TMap<TWeakObjectPtr<UPrimitiveComponent>, FFadeState> Fades;
    TArray<FHitResult> Hits;
    float UntilNextCheck = 0.f;
    double Elapsed = 0.0;
    void RestoreAll();
};
