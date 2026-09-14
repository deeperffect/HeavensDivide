#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "SwapPresentationComponent.h"
#include "AutoAttackComponent.h"
#include "NinjaCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimMontage.h"
#include "SamuraiCharacter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSwapFreezeTest,"HeavensDivide.ImpactFeedback.SwapFreeze",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSwapFreezeTest::RunTest(const FString&)
{
    auto* World=UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto Class=LoadClass<ANinjaCharacter>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Ninja.BP_Ninja_C"));
    auto* Ninja=World->SpawnActor<ANinjaCharacter>(Class,FVector::ZeroVector,FRotator::ZeroRotator,Params);
    if(!TestNotNull(TEXT("Saved Ninja spawns"),Ninja))
    { World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return false; }
    auto* Feedback=Ninja->SwapPresentation.Get();
    Feedback->BeginPlay(); // Match the component lifecycle before testing EndPlay.
    auto* Attack=Ninja->FindComponentByClass<UAutoAttackComponent>();
    Attack->OwnerCharacter=Ninja;
    Ninja->SetCharacterMode(ECharacterMode::Active);
    Feedback->bEnableSound=false;Feedback->bUseFallbackArrivalRing=false;
    // Fixed timing fixture; preserve the user's saved Blueprint tuning.
    Feedback->SwapFreezeDuration=.5f;
    Feedback->FreezeEaseInDuration=.12f;
    Feedback->bEnableNinjaArrivalDrop=true;
    Feedback->ArrivalDropHeight=180;
    Feedback->ArrivalDropDuration=.35f;
    Feedback->EntranceMontage=LoadObject<UAnimMontage>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/PlayerCharacters/Montages/Ninja/AM_NinjaSwapArrival"));
    if(Feedback->EntranceMontage)
    {
        Feedback->EntranceMontage=DuplicateObject<UAnimMontage>(Feedback->EntranceMontage,Feedback);
        Feedback->EntranceMontage->RateScale=1;
    }
    Feedback->EntrancePlayRate=1;
    auto* Settings=World->GetWorldSettings();
    Settings->SetTimeDilation(.75f);
    Ninja->GetMesh()->GlobalAnimRateScale=1.2f;
    Attack->NextAttackReadyTime=0;
    Feedback->PrepareArrival();
    TestFalse(TEXT("Activation cannot start an attack before arrival"),Attack->CanStartAttackNow());
    const FVector VisualBase=Ninja->GetVisualRoot()->GetRelativeLocation();
    const FVector ActorBase=Ninja->GetActorLocation();
    Feedback->PlayArrival();
    TestTrue(TEXT("Default swap freeze starts"),Feedback->IsSwapFreezeActive());
    TestEqual(TEXT("Ease-in begins at the previous world speed"),Settings->TimeDilation,.75f);
    TestTrue(TEXT("Ninja starts above its normal visual position"),Ninja->GetVisualRoot()->GetRelativeLocation().Z>VisualBase.Z+100.f);
    TestEqual(TEXT("Drop does not move collision actor"),Ninja->GetActorLocation(),ActorBase);
    Feedback->UpdateArrivalMovement(.175f);
    const float MidHeight=Ninja->GetVisualRoot()->GetRelativeLocation().Z;
    TestTrue(TEXT("Ninja descends toward the ground"),MidHeight>VisualBase.Z && MidHeight<VisualBase.Z+Feedback->ArrivalDropHeight);
    Feedback->UpdateArrivalMovement(.175f);
    TestEqual(TEXT("Ninja lands at its exact original visual offset"),Ninja->GetVisualRoot()->GetRelativeLocation(),VisualBase);
    Feedback->UpdateFreezeAnimationRate(.1f,.1f);
    TestEqual(TEXT("Mid-frame swap does not fast-forward animation"),Ninja->GetMesh()->GlobalAnimRateScale,1.2f);
    Feedback->UpdateSwapFreeze(.06f);
    TestTrue(TEXT("World passes through slow motion"),Settings->TimeDilation>.001f && Settings->TimeDilation<.75f);
    Feedback->UpdateSwapFreeze(.061f);
    Feedback->UpdateFreezeAnimationRate(.1f*Settings->GetEffectiveTimeDilation(),.1f);
    auto* Anim=Ninja->GetMesh()->GetSingleNodeInstance();
    if(TestNotNull(TEXT("Ninja animation instance"),Anim) && TestNotNull(TEXT("Actual entrance montage"),Feedback->EntranceMontage.Get()))
    {
        TestEqual(TEXT("Entrance montage owns the pose"),Anim->GetAnimationAsset(),static_cast<UAnimationAsset*>(Feedback->EntranceMontage));
        Feedback->UpdateEntrance(.1f);
        TestTrue(TEXT("Entrance advances while world is frozen"),Anim->GetCurrentTime()>.05f);
    }
    TestTrue(TEXT("World is nearly stopped"),Settings->TimeDilation<=.001f);
    TestFalse(TEXT("Normal attack cannot interrupt entrance"),Attack->CanStartAttackNow());
    TestFalse(TEXT("Direct/assist montage cannot interrupt entrance"),Attack->PlayAttackMontage(false));
    TestTrue(TEXT("Animation compensates for world dilation"),FMath::IsNearlyEqual(
        Ninja->GetMesh()->GlobalAnimRateScale*Settings->GetEffectiveTimeDilation(),1.2f));
    Feedback->UpdateSwapFreeze(.25f);
    TestTrue(TEXT("Freeze lasts half a real second"),Feedback->IsSwapFreezeActive());
    Feedback->UpdateSwapFreeze(.14f);
    TestFalse(TEXT("Freeze expires using real time"),Feedback->IsSwapFreezeActive());
    TestEqual(TEXT("Previous world speed restored"),Settings->TimeDilation,.75f);
    TestEqual(TEXT("Mesh animation rate restored"),Ninja->GetMesh()->GlobalAnimRateScale,1.2f);
    TestFalse(TEXT("Freeze expiry cannot cut off unfinished entrance"),Attack->CanStartAttackNow());
    Feedback->UpdateEntrance(Feedback->EntranceRemaining+1.f);
    TestTrue(TEXT("Final montage frame reached"),FMath::IsNearlyEqual(Anim->GetCurrentTime(),Feedback->EntranceMontage->GetPlayLength()));
    TestFalse(TEXT("Final frame remains protected"),Attack->CanStartAttackNow());
    Feedback->UpdateEntrance(0);
    TestEqual(TEXT("Animation Blueprint restored"),Ninja->GetMesh()->GetAnimationMode(),EAnimationMode::AnimationBlueprint);
    TestTrue(TEXT("Ready attack may resume"),Attack->CanStartAttackNow());
    TestEqual(TEXT("Cooldown was not reset"),Attack->NextAttackReadyTime,0.0);
    Feedback->PlayArrival();
    Ninja->SetCharacterMode(ECharacterMode::Inactive);
    TestFalse(TEXT("Mode changes cancel freeze"),Feedback->IsSwapFreezeActive());
    TestEqual(TEXT("Interrupted drop restores visual position"),Ninja->GetVisualRoot()->GetRelativeLocation(),VisualBase);
    TestEqual(TEXT("Mode cancellation restores time"),Settings->TimeDilation,.75f);
    Ninja->SetCharacterMode(ECharacterMode::Active);
    Feedback->PlayArrival();
    Feedback->EndPlay(EEndPlayReason::EndPlayInEditor);
    TestEqual(TEXT("Teardown restores time"),Settings->TimeDilation,.75f);
    Feedback->SwapFreezeDuration=0;
    Feedback->PlayArrival();
    TestFalse(TEXT("Zero duration disables freeze"),Feedback->IsSwapFreezeActive());
    Feedback->FinishSwapFreeze();
    TestEqual(TEXT("Cancellation restores drop even with freeze disabled"),Ninja->GetVisualRoot()->GetRelativeLocation(),VisualBase);
    Feedback->StopEntrance();
    auto* Samurai=World->SpawnActor<ASamuraiCharacter>(FVector(1000,0,0),FRotator::ZeroRotator,Params);
    const FVector SamuraiBase=Samurai->GetVisualRoot()->GetRelativeLocation();
    const FVector SamuraiActorBase=Samurai->GetActorLocation();
    auto* SamuraiFeedback=Samurai->SwapPresentation.Get();
    Samurai->SetVisualFacingRotation(FRotator(0,90,0));
    SamuraiFeedback->ArrivalWalkDistance=240;
    SamuraiFeedback->ArrivalWalkDuration=.4f;
    SamuraiFeedback->StartArrivalMovement();
    TestTrue(TEXT("Samurai starts behind its facing direction at ground height"),Samurai->GetVisualRoot()->GetRelativeLocation().Equals(SamuraiBase+FVector(0,-240,0),.001f));
    Samurai->SetCharacterMode(ECharacterMode::Active);
    Samurai->SetFacingTarget(SamuraiActorBase+FVector(1000,0,0));
    Samurai->Tick(.1f);
    TestTrue(TEXT("Cursor cannot turn Samurai sideways during walk-in"),Samurai->GetVisualForwardVector().Equals(FVector::RightVector,.001f));
    Samurai->SetFacingOverrideTarget(SamuraiActorBase+FVector(-1000,0,0));
    Samurai->Tick(.1f);
    TestTrue(TEXT("Auto-target cannot turn Samurai sideways during walk-in"),Samurai->GetVisualForwardVector().Equals(FVector::RightVector,.001f));
    SamuraiFeedback->UpdateArrivalMovement(.2f);
    TestTrue(TEXT("Samurai walks forward at constant speed"),Samurai->GetVisualRoot()->GetRelativeLocation().Equals(SamuraiBase+FVector(0,-120,0),.001f));
    TestEqual(TEXT("Samurai walk leaves collision actor in place"),Samurai->GetActorLocation(),SamuraiActorBase);
    SamuraiFeedback->UpdateArrivalMovement(.2f);
    TestEqual(TEXT("Samurai finishes at original visual offset"),Samurai->GetVisualRoot()->GetRelativeLocation(),SamuraiBase);
    Samurai->ClearFacingOverride();
    Samurai->Tick(.1f);
    TestTrue(TEXT("Normal aiming resumes after walk-in"),Samurai->GetVisualForwardVector().Equals(FVector::ForwardVector,.001f));
    SamuraiFeedback->StartArrivalMovement();
    SamuraiFeedback->FinishSwapFreeze();
    TestEqual(TEXT("Samurai cancellation restores original offset"),Samurai->GetVisualRoot()->GetRelativeLocation(),SamuraiBase);
    SamuraiFeedback->bEnableSamuraiWalkIn=false;
    SamuraiFeedback->StartArrivalMovement();
    TestEqual(TEXT("Disabling Samurai walk does not fall back to a drop"),Samurai->GetVisualRoot()->GetRelativeLocation(),SamuraiBase);
    TestTrue(TEXT("Samurai setting does not disable Ninja drop"),Feedback->bEnableNinjaArrivalDrop);
    World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
    return true;
}
#endif
