#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "IndicatorAttackVFX.h"
#include "BossGroundTelegraph.h"
#include "EnemyAttackCue.h"
#include "EnemyBase.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#include "RenderingThread.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIndicatorAttackVFXTest, "HeavensDivide.Combat.IndicatorAttackVFX",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)
bool FIndicatorAttackVFXTest::RunTest(const FString&)
{
    auto* W = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
    W->InitializeActorsForPlay(FURL()); W->BeginPlay();
    ON_SCOPE_EXIT { W->DestroyWorld(false); GEngine->DestroyWorldContext(W); };
    auto* Source = W->SpawnActor<AActor>();
    auto* SourceRoot = NewObject<USceneComponent>(Source); Source->SetRootComponent(SourceRoot); Source->AddInstanceComponent(SourceRoot); SourceRoot->RegisterComponent();
    auto Count = [W]() { int32 N = 0; for (TActorIterator<AIndicatorAttackVFX> It(W); It; ++It) if (!It->IsActorBeingDestroyed()) ++N; return N; };
    auto* Hazard = W->SpawnActor<AEncounterHazard>();
    Hazard->Initialize(Source, EEncounterShape::Circle, 250, 0, 1, 10, .2f);
    Hazard->Tick(.9f); TestEqual(TEXT("No attack VFX during charge"), Count(), 0);
    Hazard->Tick(.11f); TestEqual(TEXT("Exactly one execution effect"), Count(), 1);
    Hazard->Tick(.02f); TestEqual(TEXT("No repeat burst or change to damage cadence"), Count(), 1);
    Hazard->Destroy(); TestEqual(TEXT("Warning cleanup preserves particle tail"), Count(), 1);
    auto* Cancelled = W->SpawnActor<AEncounterHazard>();
    Cancelled->Initialize(Source, EEncounterShape::Circle, 250, 0, 1, 10); Cancelled->Destroy();
    TestEqual(TEXT("Cancelled warning emits nothing"), Count(), 1);
    auto* Skipped = W->SpawnActor<AEncounterHazard>();
    Skipped->Initialize(Source, EEncounterShape::Circle, 250, 0, 1, 10); Skipped->Tick(2);
    TestEqual(TEXT("Skipped active window emits nothing"), Count(), 1);
    auto* ChargeWarning = W->SpawnActor<AEncounterHazard>();
    ChargeWarning->Initialize(Source, EEncounterShape::Lane, 800, 120, 1, 0); ChargeWarning->Tick(1.01f);
    TestEqual(TEXT("A charge lane does not pretend to damage its entire length"), Count(), 1);
    auto* Telegraph = W->SpawnActor<ABossGroundTelegraph>();
    Telegraph->InitializePersistentSurfaceCircle(320, nullptr);
    TestEqual(TEXT("Circle records radius rather than diameter"), Telegraph->ImpactSize, 320.f);
    TestTrue(TEXT("Surface circle remains a circle"), Telegraph->ImpactShape == EEncounterShape::Circle);
    Telegraph->PlayImpact(); Telegraph->PlayImpact();
    TestEqual(TEXT("Repeated notify emits once"), Count(), 2);
    auto* CancelTelegraph = W->SpawnActor<ABossGroundTelegraph>(); CancelTelegraph->CancelTelegraph(); CancelTelegraph->PlayImpact();
    TestEqual(TEXT("Cancelled montage cannot emit late VFX"), Count(), 2);
    auto* Moving = AIndicatorAttackVFX::BeginCharge(Source, 160, .5f);
    if (!TestNotNull(TEXT("Moving charge effect"), Moving)) return false;
    Source->SetActorLocation(FVector(500, 0, 0)); Moving->Tick(.13f);
    TestTrue(TEXT("Charge emits at its actual moving hitbox"), Moving->Bursts.Num() == 2 && Moving->Bursts.Last()->GetComponentLocation().Equals(FVector(500, 0, 12)));
    Moving->StopCharge(); const int32 BurstCount = Moving->Bursts.Num(); Moving->Tick(.2f);
    TestEqual(TEXT("Charge stop prevents new emission"), Moving->Bursts.Num(), BurstCount);
    TestFalse(TEXT("Charge stop leaves no timer running"), Moving->IsActorTickEnabled());
    for (const auto& Sample : AIndicatorAttackVFX::Samples(EEncounterShape::Ring, 410, 160, FVector::ZeroVector))
    {
        TestTrue(TEXT("Ring particles outside safe centre"), Sample.Offset.Size2D() - Sample.Radius >= 160);
        TestTrue(TEXT("Ring particles inside outer boundary"), Sample.Offset.Size2D() + Sample.Radius <= 410);
    }
    const auto Huge = AIndicatorAttackVFX::Samples(EEncounterShape::Ring, 10000, 700, FVector(1600, 0, 0));
    TestTrue(TEXT("Arena-wide VFX have a bounded budget"), !Huge.IsEmpty() && Huge.Num() <= 49);
    for (const auto& Sample : Huge) TestTrue(TEXT("Arena-wide safe hole remains empty"), Sample.Offset.Size2D() - Sample.Radius >= 700);
    for (TActorIterator<AIndicatorAttackVFX> It(W); It; ++It) It->Destroy();
    for (TActorIterator<AEncounterHazard> It(W); It; ++It) It->Destroy();
    for (TActorIterator<ABossGroundTelegraph> It(W); It; ++It) It->Destroy();

    // Render real copied systems, with the same transforms used by attacks.
    auto* Camera = W->SpawnActor<AActor>();
    auto* Capture = NewObject<USceneCaptureComponent2D>(Camera); Camera->AddInstanceComponent(Capture); Capture->RegisterComponent();
    Capture->SetWorldLocation(FVector(0, 0, 1400)); Capture->SetWorldRotation(FRotator(-90, 0, 0));
    Capture->ProjectionType = ECameraProjectionMode::Orthographic; Capture->OrthoWidth = 1200; Capture->ShowFlags.SetLighting(false);
    Capture->bCaptureEveryFrame = false; Capture->bCaptureOnMovement = false;
    auto* Target = NewObject<UTextureRenderTarget2D>(Camera); Target->RenderTargetFormat = RTF_RGBA8;
    Target->ClearColor = FLinearColor(.04f, .06f, .075f, 1); Target->InitAutoFormat(600, 600); Target->UpdateResourceImmediate(); Capture->TextureTarget = Target;
    const TCHAR* Names[] = { TEXT("Impact"), TEXT("Slash"), TEXT("Fire"), TEXT("Frost"), TEXT("Poison"), TEXT("Flash"), TEXT("Lane"), TEXT("Ring") };
    for (int32 Pass = 0; Pass < 2; ++Pass)
    for (int32 I = 0; I < 8; ++I)
    {
        auto* Footprint = W->SpawnActor<AEncounterHazard>(FVector(0, 0, -2), FRotator(0, I == 6 ? 30 : 0, 0));
        Footprint->Initialize(Source, I == 7 ? EEncounterShape::Ring : I == 6 ? EEncounterShape::Lane : EEncounterShape::Circle,
            I == 7 ? 450 : I == 6 ? 900 : 250, I == 7 ? 200 : 140, 10, 0);
        Footprint->SetActorTickEnabled(false);
        if (auto* MID = Cast<UMaterialInstanceDynamic>(Footprint->GroundSurface->GetMaterial(0)))
        { MID->SetScalarParameterValue(TEXT("FillAmount"), 1); MID->SetScalarParameterValue(TEXT("Impact"), 1); }
        auto* Visual = I == 5 ? AIndicatorAttackVFX::Release(Source, FVector::ZeroVector)
            : AIndicatorAttackVFX::Spawn(Source, FVector::ZeroVector, I == 7 ? EEncounterShape::Ring : I == 6 ? EEncounterShape::Lane : EEncounterShape::Circle,
                I == 7 ? 450 : I == 6 ? 900 : 250, I == 7 ? 200 : 140, I == 6 ? 30 : 0,
                I < 5 ? static_cast<EIndicatorVFX>(I) : EIndicatorVFX::Impact);
        if (!TestNotNull(TEXT("Execution VFX actor"), Visual)) continue;
        TestFalse(TEXT("VFX actor cannot deal damage through collision"), Visual->GetActorEnableCollision());
        for (const auto& Burst : Visual->Bursts)
        {
            auto* FX = Burst.Get();
            TestNotNull(TEXT("Cookable saved system reference"), FX->GetAsset());
            TestTrue(TEXT("Cosmetics do not consume gameplay random numbers"), FX->GetAsset()->NeedsDeterminism());
#if WITH_EDITORONLY_DATA
            FX->GetAsset()->WaitForCompilationComplete(true, false);
            for (const auto& Handle : FX->GetAsset()->GetEmitterHandles())
                if (auto* Data = Handle.GetEmitterData()) TestTrue(TEXT("Copied emitters honor component scale"), Data->bLocalSpace);
#endif
            FX->SetForceSolo(true); FX->Activate(true); FX->AdvanceSimulation(8, .02f);
            FX->UpdateBounds(); FX->MarkRenderStateDirty();
            if (Pass == 1) UE_LOG(LogTemp, Display, TEXT("[ChargedAttackVFX] %s bounds=%s"), Names[I], *FX->Bounds.ToString());
        }
        FAssetCompilingManager::Get().FinishAllCompilation(); if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
        for (int32 Frame = 0; Frame < 4; ++Frame)
        {
            ++GFrameCounter; W->Tick(LEVELTICK_All, .001f);
            for (const auto& Burst : Visual->Bursts) Burst->MarkRenderDynamicDataDirty();
            W->SendAllEndOfFrameUpdates(); FlushRenderingCommands(); Capture->CaptureScene(); FlushRenderingCommands();
        }
        if (Pass == 1) UKismetRenderingLibrary::ExportRenderTarget(W, Target,
            FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("IndicatorAttackVFX/Previews")), FString(Names[I]) + TEXT(".png"));
        for (const auto& Burst : Visual->Bursts)
        {
            auto* FX = Burst.Get();
            FX->AdvanceSimulation(150, .02f);
            TestTrue(FString::Printf(TEXT("%s burst finishes before cosmetic actor cleanup"), Names[I]), FX->IsComplete());
        }
        Visual->Destroy(); Footprint->Destroy(); FlushRenderingCommands();
    }
    return true;
}
#endif
