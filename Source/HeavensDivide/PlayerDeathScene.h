#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Camera/CameraTypes.h"
#include "PlayerDeathScene.generated.h"

/** Isolated capture of a cosmetic actor; never moves or recolors the actual character. */
UCLASS(Transient, NotBlueprintable)
class HEAVENSDIVIDE_API APlayerDeathScene : public AActor
{
    GENERATED_BODY()
public:
    APlayerDeathScene();
    bool Initialize(class ACharacterBase* Character, FLinearColor Color, float PlayRate,
        const FMinimalViewInfo& View, const struct FSceneViewProjectionData& Projection, float FadeSeconds);
    void SetElapsed(float RealSeconds);
    float GetDuration() const { return AnimationSeconds + FadeDuration + .8f; }
    class UTextureRenderTarget2D* GetTexture() const { return Texture; }
    class ASwapAfterimage* GetSilhouette() const { return Silhouette; }
    const FMinimalViewInfo& GetFrozenView() const { return FrozenView; }
    FBox2D GetViewRect() const { return NormalizedViewRect; }
    float GetFadeDuration() const { return FadeDuration; }
    virtual void Destroyed() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY() TObjectPtr<class USceneCaptureComponent2D> Capture;
    UPROPERTY() TObjectPtr<class UMaterialInterface> SilhouetteMaterial;
    UPROPERTY() TObjectPtr<class UTextureRenderTarget2D> Texture;
    UPROPERTY() TObjectPtr<class ASwapAfterimage> Silhouette;
    FMinimalViewInfo FrozenView;
    FBox2D NormalizedViewRect = FBox2D(FVector2D::ZeroVector, FVector2D(1));
    float FadeDuration = .28f;
    float AnimationSeconds = 3.f;
};
