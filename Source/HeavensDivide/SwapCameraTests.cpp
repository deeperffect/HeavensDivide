#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PlayerCameraRig.h"
#include "NinjaCharacter.h"
#include "SamuraiCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "SwapPresentationComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSwapCameraTest,"HeavensDivide.ImpactFeedback.SwapCamera",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSwapCameraTest::RunTest(const FString&)
{
    auto* World=UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    auto* RigClass=LoadClass<APlayerCameraRig>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_PlayerCameraRig.BP_PlayerCameraRig_C"));
    auto* Rig=World->SpawnActor<APlayerCameraRig>(RigClass);
    for(auto* Class : {ANinjaCharacter::StaticClass(),ASamuraiCharacter::StaticClass()})
    {
        FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Character=World->SpawnActor<ACharacterBase>(Class,FVector::ZeroVector,FRotator::ZeroRotator,Params);
        Rig->SetFollowTarget(Character);
        Rig->Camera->SetFieldOfView(83);
        Rig->Camera->SetOrthoWidth(1500);
        Rig->CameraBoom->bEnableCameraLag=true;
        Rig->Camera->PostProcessSettings.VignetteIntensity=.3f;
        Rig->Camera->PostProcessSettings.bOverride_VignetteIntensity=false;
        Character->GetVisualRoot()->SetRelativeLocation(FVector(-100,0,180));
        Rig->StartSwapFocus();
        TestEqual(TEXT("Focus starts without FOV pop"),Rig->Camera->FieldOfView,83.f);
        Rig->UpdateSwapFocus(.06f);
        const float HalfFOV=Rig->Camera->FieldOfView;
        TestTrue(TEXT("Camera smoothly zooms in"),HalfFOV<83 && HalfFOV>83*(1-Rig->SwapZoomAmount));
        Rig->UpdateSwapFocus(.06f);
        TestTrue(TEXT("Peak focus tracks arriving visual"),Rig->GetActorLocation().Equals(Character->GetVisualRoot()->GetComponentLocation()));
        TestTrue(TEXT("Orthographic cameras also zoom"),Rig->Camera->OrthoWidth<1500);
        TestTrue(TEXT("Peak vignette adds a subtle focus accent"),Rig->Camera->PostProcessSettings.VignetteIntensity>.3f);
        Character->SetActorLocation(FVector(100,0,0));
        Character->GetVisualRoot()->SetRelativeLocation(FVector::ZeroVector);
        Rig->UpdateSwapFocus(.325f);
        TestTrue(TEXT("Slow zoom-out follows the moving character"),Rig->GetActorLocation().Equals(Character->GetActorLocation()));
        Rig->UpdateSwapFocus(.4f);
        TestEqual(TEXT("Original FOV restored exactly"),Rig->Camera->FieldOfView,83.f);
        TestEqual(TEXT("Original orthographic width restored exactly"),Rig->Camera->OrthoWidth,1500.f);
        TestEqual(TEXT("Vignette restored exactly"),Rig->Camera->PostProcessSettings.VignetteIntensity,.3f);
        TestFalse(TEXT("Vignette override restored"),Rig->Camera->PostProcessSettings.bOverride_VignetteIntensity);
        TestTrue(TEXT("Camera lag restored"),Rig->CameraBoom->bEnableCameraLag);
        Rig->StartSwapFocus();Rig->UpdateSwapFocus(.06f);Rig->StartSwapFocus();
        Rig->StopSwapFocus();
        TestEqual(TEXT("Repeated focus cannot accumulate zoom"),Rig->Camera->FieldOfView,83.f);
        Rig->StartSwapFocus();Rig->UpdateSwapFocus(.06f);Rig->SetFollowTarget(nullptr);
        TestEqual(TEXT("Losing target restores normal camera"),Rig->Camera->FieldOfView,83.f);
        auto* Controller=World->SpawnActor<APlayerController>();
        Controller->SetAsLocalPlayerController(); // No LocalPlayer is created by this isolated test world.
        if(!Controller->PlayerCameraManager)
        {
            Controller->PlayerCameraManager=World->SpawnActor<APlayerCameraManager>();
            Controller->PlayerCameraManager->InitializeFor(Controller);
        }
        Controller->Possess(Character);
        Rig->SetFollowTarget(Character);
        Controller->SetViewTarget(Rig);
        TestEqual(TEXT("Controller uses the camera rig"),Controller->GetViewTarget(),static_cast<AActor*>(Rig));
        Character->SetCharacterMode(ECharacterMode::Active);
        Character->SwapPresentation->bEnableSound=false;
        Character->SwapPresentation->PlayArrival();
        TestTrue(TEXT("Arrival freeze starts in the integration fixture"),Character->SwapPresentation->IsSwapFreezeActive());
        TestTrue(TEXT("Actual arrival starts camera focus"),Rig->bSwapFocusActive);
        Character->SwapPresentation->FinishSwapFreeze(false);
        TestTrue(TEXT("Natural freeze expiry allows the slow camera return"),Rig->bSwapFocusActive);
        Character->SwapPresentation->FinishSwapFreeze();
        TestFalse(TEXT("Explicit arrival cancellation restores camera"),Rig->bSwapFocusActive);
    }
    World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
    return true;
}
#endif
