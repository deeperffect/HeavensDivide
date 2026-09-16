#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PlayerCameraRig.h"
#include "NinjaCharacter.h"
#include "SamuraiCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAbilityCameraTest, "HeavensDivide.ImpactFeedback.AbilityCamera",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAbilityCameraTest::RunTest(const FString&)
{
    auto* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    auto* RigClass = LoadClass<APlayerCameraRig>(nullptr, TEXT("/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_PlayerCameraRig.BP_PlayerCameraRig_C"));
    auto* Rig = World->SpawnActor<APlayerCameraRig>(RigClass);
    auto* Ninja = World->SpawnActor<ANinjaCharacter>();
    Rig->SetFollowTarget(Ninja);
    Rig->CameraBoom->TargetArmLength = 1000;
    Rig->Camera->SetOrthoWidth(1500);
    Rig->CameraBoom->bEnableCameraLag = true;
    const FVector Origin = Ninja->GetActorLocation();
    Ninja->bComboAbilityActive = true;
    TestTrue(TEXT("Ninja ability starts framing"), Rig->UpdateAbilityCamera(0));
    Ninja->SetActorLocation(Origin + FVector(50, 0, 0));
    Rig->UpdateAbilityCamera(.1f);
    TestTrue(TEXT("Small dash stays inside stationary dead zone"), Rig->GetActorLocation().Equals(Origin));
    Ninja->SetActorLocation(Origin + FVector(350, 0, 0));
    Rig->UpdateAbilityCamera(.1f);
    TestTrue(TEXT("Longer dash follows gently"), Rig->GetActorLocation().X > Origin.X && Rig->GetActorLocation().X < Origin.X + 100);
    Ninja->SetActorLocation(Origin + FVector(-1500, 0, 0));
    Rig->UpdateAbilityCamera(.05f);
    TestTrue(TEXT("Fast reverse dash stays within maximum focus distance"),
        FVector::Distance(Rig->GetActorLocation(), Ninja->GetActorLocation()) <= Rig->AbilityMaxFocusDistance + .1f);
    TestTrue(TEXT("Wider perspective framing"), Rig->CameraBoom->TargetArmLength > 1000);
    TestTrue(TEXT("Wider orthographic framing"), Rig->Camera->OrthoWidth > 1500);
    Ninja->bComboAbilityActive = false;
    const FVector BeforeReturn = Rig->GetActorLocation();
    Rig->UpdateAbilityCamera(0);
    TestTrue(TEXT("No snap at start of return"), Rig->GetActorLocation().Equals(BeforeReturn));
    Ninja->SetActorLocation(Ninja->GetActorLocation() + FVector(100, 0, 0));
    Rig->UpdateAbilityCamera(Rig->AbilityBlendOutDuration);
    TestTrue(TEXT("Return reaches moving target"), Rig->GetActorLocation().Equals(Ninja->GetActorLocation()));
    TestEqual(TEXT("Original arm restored"), Rig->CameraBoom->TargetArmLength, 1000.f);
    TestEqual(TEXT("Original ortho width restored"), Rig->Camera->OrthoWidth, 1500.f);
    TestTrue(TEXT("Original spring arm lag restored"), Rig->CameraBoom->bEnableCameraLag);
    Ninja->bComboAbilityActive = true;
    Rig->UpdateAbilityCamera(.3f);
    Rig->SetFollowTarget(nullptr);
    TestFalse(TEXT("Target loss cancels framing"), Rig->bAbilityCameraActive);
    TestEqual(TEXT("Target loss restores zoom"), Rig->CameraBoom->TargetArmLength, 1000.f);
    Rig->SetFollowTarget(Ninja);
    Rig->UpdateAbilityCamera(.3f);
    Rig->StartSwapFocus();
    TestFalse(TEXT("Swap focus cancels ability framing"), Rig->bAbilityCameraActive);
    Rig->StopSwapFocus();
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Samurai = World->SpawnActor<ASamuraiCharacter>(ASamuraiCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
    Rig->SetFollowTarget(Samurai);
    Samurai->bComboAbilityActive = true;
    TestFalse(TEXT("Samurai retains normal camera"), Rig->UpdateAbilityCamera(.3f));
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}
#endif
