#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraSystemInstanceController.h"
#include "NiagaraDataSet.h"
#include "NiagaraDataSetAccessor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSamuraiSmokeScaleTest, "HeavensDivide.Presentation.SamuraiSmokeScale",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSamuraiSmokeScaleTest::RunTest(const FString&)
{
    for (const TCHAR* Name : {TEXT("OverkillBurst"), TEXT("SamuraiActive")})
    {
    auto* Asset = LoadObject<UNiagaraSystem>(nullptr,
        *FString::Printf(TEXT("/Game/HeavensDivide/VFX/Upgrades/NS_%s.NS_%s"),Name,Name));
    if (!TestNotNull(TEXT("Samurai smoke exists"), Asset)) return false;
    auto* System = DuplicateObject<UNiagaraSystem>(Asset, GetTransientPackage());
    for (const auto& Handle : System->GetEmitterHandles())
        if (auto* Data = Handle.GetEmitterData()) Data->bDeterminism = true;
    System->RequestCompile(false);
    System->WaitForCompilationComplete(true, false);

    auto* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    World->BeginPlay();
    TArray<UNiagaraComponent*> Bursts;
    const FNiagaraVariable ScaleParameter(FNiagaraTypeDefinition::GetFloatDef(),TEXT("User._Scale"));
    const float AuthoredScale=System->GetExposedParameters().GetParameterValue<float>(ScaleParameter);
    TestTrue(TEXT("Preserved small authored scale"),AuthoredScale>0.f && AuthoredScale<1.f);
    for (float Scale : {.5f, 1.f})
    {
        auto* Burst = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, System,
            FVector::ZeroVector, FRotator::ZeroRotator, FVector::OneVector, false, false,
            ENCPoolMethod::None, false);
        if (Burst)
        {
            Burst->SetVariableFloat(TEXT("User._Scale"),AuthoredScale*Scale);
            Burst->SetForceSolo(true);
            Burst->Activate();
        }
        Bursts.Add(Burst);
    }
    // Check early and later frames to catch size curves overriding scale and
    // accidental repeated multiplication of the size every frame.
    int32 VerifiedFrames = 0;
    for (int32 Frame = 0; Frame < 120; ++Frame)
    {
        for (auto* Burst : Bursts) if (Burst) Burst->AdvanceSimulation(1, 1.f / 60.f);
        auto* A = Bursts[0] && Bursts[0]->GetSystemInstanceController()
            ? Bursts[0]->GetSystemInstanceController()->GetSystemInstance_Unsafe() : nullptr;
        auto* B = Bursts[1] && Bursts[1]->GetSystemInstanceController()
            ? Bursts[1]->GetSystemInstanceController()->GetSystemInstance_Unsafe() : nullptr;
        if (!TestTrue(TEXT("Both scaled bursts simulate"), A && B)) break;
        if (Frame == 119) AddInfo(FString::Printf(TEXT("Component scales: %s / %s; simulation owner scales: %s / %s"),
            *Bursts[0]->GetComponentScale().ToString(), *Bursts[1]->GetComponentScale().ToString(),
            *A->GetOwnerParameters().EngineScale.ToString(), *B->GetOwnerParameters().EngineScale.ToString()));
        int32 Checked = 0;
        for (int32 Index = 0; Index < A->GetEmitters().Num(); ++Index)
        {
            auto& DataA = A->GetEmitters()[Index]->GetParticleData();
            auto& DataB = B->GetEmitters()[Index]->GetParticleData();
            auto SizesA = FNiagaraDataSetAccessor<FVector3f>::CreateReader(DataA, TEXT("Scale"));
            auto SizesB = FNiagaraDataSetAccessor<FVector3f>::CreateReader(DataB, TEXT("Scale"));
            const int32 Count = DataA.GetCurrentData() ? DataA.GetCurrentData()->GetNumInstances() : 0;
            if (Frame == 119) AddInfo(FString::Printf(TEXT("Emitter %d particles=%d mesh attribute=%d state=%d"),
                Index, Count, SizesA.IsValid(), int32(A->GetEmitters()[Index]->GetExecutionState())));
            if (!SizesA.IsValid() || !SizesB.IsValid() || Count == 0) continue;
            const FVector3f SizeA = SizesA.GetSafe(0, FVector3f::ZeroVector);
            const FVector3f SizeB = SizesB.GetSafe(0, FVector3f::ZeroVector);
            if (SizeA.IsNearlyZero()) continue;
            TestTrue(TEXT("Doubling art scale doubles live mesh size"), SizeB.Equals(SizeA * 2.f, 0.01f));
            ++Checked;
        }
        if (Checked > 0) ++VerifiedFrames;
    }
    TestTrue(TEXT("Measured live mesh particles across multiple frames"), VerifiedFrames > 1);
    for (auto* Burst : Bursts) if (Burst) Burst->DestroyComponent();
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    }
    return !HasAnyErrors();
}
#endif
