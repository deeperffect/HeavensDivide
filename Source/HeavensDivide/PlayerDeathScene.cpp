#include "PlayerDeathScene.h"
#include "CharacterBase.h"
#include "SwapAfterimage.h"
#include "Animation/AnimMontage.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/MeshComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "SceneView.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

APlayerDeathScene::APlayerDeathScene()
{
    Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("SilhouetteCapture"));
    SetRootComponent(Capture);
    Capture->bCaptureEveryFrame = false; Capture->bCaptureOnMovement = false;
    Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
    Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
    Capture->ShowFlags.SetAtmosphere(false); Capture->ShowFlags.SetFog(false);
    Capture->ShowFlags.SetSkyLighting(false); Capture->ShowFlags.SetLighting(false);
    Capture->ShowFlags.SetPostProcessing(false); Capture->ShowFlags.SetTonemapper(false);
    Capture->ShowFlags.SetAntiAliasing(false);
    Capture->PostProcessBlendWeight = 0.f;
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Game/HeavensDivide/VFX/Death/M_DeathSilhouette"));
    SilhouetteMaterial = Material.Object;
}
bool APlayerDeathScene::Initialize(ACharacterBase* Character, FLinearColor Color, float PlayRate,
    const FMinimalViewInfo& View, const FSceneViewProjectionData& Projection, float FadeSeconds)
{
    if (!Character || !Character->GetDeathMontage() || !SilhouetteMaterial) return false;
    FrozenView = View;
    FadeDuration = FMath::Max(.01f, FadeSeconds);
    AnimationSeconds = Character->GetDeathMontage()->GetPlayLength() / FMath::Max(.1f, PlayRate * Character->GetDeathMontage()->RateScale);
    FActorSpawnParameters Params; Params.Owner = this;
    Silhouette = GetWorld()->SpawnActor<ASwapAfterimage>(Character->GetActorLocation(), FRotator::ZeroRotator, Params);
    if (!Silhouette || !Silhouette->InitializeJointCast(Character, Character->GetDeathMontage(), Character->GetVisualForwardVector(), GetDuration())) return false;
    Silhouette->SetLifeSpan(0.f);
    auto* Material = UMaterialInstanceDynamic::Create(SilhouetteMaterial, this);
    Material->SetVectorParameterValue(TEXT("SilhouetteColor"), Color);
    TInlineComponentArray<UMeshComponent*> Meshes(Silhouette);
    for (auto* Mesh : Meshes) {
        Mesh->SetVisibleInSceneCaptureOnly(true);
        for (int32 I=0; I<Mesh->GetNumMaterials(); ++I) Mesh->SetMaterial(I, Material);
    }
    Capture->ShowOnlyActorComponents(Silhouette);
    // Match the actual rendered gameplay view, including aspect constraints and
    // cinematic/shake offsets. There is no portrait camera or character turn.
    SetActorLocationAndRotation(Projection.ViewOrigin, View.Rotation);
    Capture->ProjectionType = View.ProjectionMode;
    Capture->FOVAngle = View.FOV;
    Capture->OrthoWidth = View.OrthoWidth;
    Capture->bUseCustomProjectionMatrix = true;
    Capture->CustomProjectionMatrix = Projection.ProjectionMatrix;
    const FIntRect FullRect = Projection.GetViewRect();
    const FIntRect ViewRect = Projection.GetConstrainedViewRect();
    const FVector2D FullSize(FullRect.Width(), FullRect.Height());
    NormalizedViewRect = FBox2D(FVector2D(ViewRect.Min - FullRect.Min) / FullSize,
        FVector2D(ViewRect.Max - FullRect.Min) / FullSize);
    const float ResolutionScale = FMath::Min(1.f, FMath::Min(1920.f / ViewRect.Width(), 1080.f / ViewRect.Height()));
    Texture = NewObject<UTextureRenderTarget2D>(this);
    Texture->ClearColor = FLinearColor::Black;
    Texture->InitCustomFormat(FMath::Max(1, FMath::RoundToInt(ViewRect.Width() * ResolutionScale)),
        FMath::Max(1, FMath::RoundToInt(ViewRect.Height() * ResolutionScale)), PF_B8G8R8A8, false);
    Texture->UpdateResourceImmediate(true);
    Capture->TextureTarget = Texture;
    SetElapsed(0.f);
    return true;
}
void APlayerDeathScene::SetElapsed(float RealSeconds)
{
    if (!Silhouette) return;
    Silhouette->SetJointPose(FMath::Clamp((RealSeconds-FadeDuration)/AnimationSeconds,0.f,.99f));
    Capture->CaptureScene();
}
void APlayerDeathScene::EndPlay(const EEndPlayReason::Type Reason)
{
    if (Silhouette) { Silhouette->Destroy(); Silhouette = nullptr; }
    Capture->TextureTarget = nullptr;
    Super::EndPlay(Reason);
}

void APlayerDeathScene::Destroyed()
{
    if (Silhouette) { Silhouette->Destroy(); Silhouette = nullptr; }
    Capture->TextureTarget = nullptr;
    Super::Destroyed();
}
