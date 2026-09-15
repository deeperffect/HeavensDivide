#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "SwapPresentationComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimNotifies/AnimNotify_PlaySound.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSwapEntranceTest,"HeavensDivide.ImpactFeedback.SwapEntrance",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSwapEntranceTest::RunTest(const FString&)
{
    auto* World=UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    for(const FString Name : {FString(TEXT("Ninja")),FString(TEXT("Samurai"))})
    {
        auto* Class=LoadClass<ACharacterBase>(nullptr,*(TEXT("/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_")+Name+TEXT(".BP_")+Name+TEXT("_C")));
        FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Character=World->SpawnActor<ACharacterBase>(Class,FVector::ZeroVector,FRotator::ZeroRotator,Params);
        if(!TestNotNull(Name+TEXT(" character"),Character)) continue;
        auto* Feedback=Character->SwapPresentation.Get();
        Feedback->SwapFreezeDuration=0;
        Feedback->EntrancePlayRate=1;
        Feedback->bEnableSound=false;
        Feedback->EntranceMontage=LoadObject<UAnimMontage>(nullptr,Name==TEXT("Ninja") ?
            TEXT("/Game/HeavensDivide/Blueprints/PlayerCharacters/Montages/Ninja/AM_NinjaSwapArrival") :
            TEXT("/Game/HeavensDivide/Blueprints/PlayerCharacters/Montages/Samurai/AM_AutoAttackSamurai"));
        Character->SetCharacterMode(ECharacterMode::Active);
        auto* Mesh=Character->GetMesh();
        auto* OriginalClass=Mesh->GetAnimClass();
        auto* Controller=World->SpawnActor<APlayerController>();
        Controller->Possess(Character);
        Character->AddMovementInput(FVector::ForwardVector,1.f,true);
        Character->GetCharacterMovement()->Velocity=FVector(200,0,0);
        Feedback->PlayArrival();
        TestTrue(TEXT("Arrival clears queued movement"),Character->GetPendingMovementInputVector().IsNearlyZero());
        TestTrue(TEXT("Arrival clears leftover movement velocity"),Character->GetVelocity().IsNearlyZero());
        Character->MoveCharacter(FVector2D(1,0));
        TestTrue(TEXT("Movement cannot start during entrance"),Character->GetPendingMovementInputVector().IsNearlyZero());
        auto* Anim=Mesh->GetSingleNodeInstance();
        if(!TestNotNull(Name+TEXT(" exclusive arrival instance"),Anim)) continue;
        TestFalse(TEXT("Idle/attack Animation Blueprint cannot evaluate over entrance"),Mesh->IsComponentTickEnabled());
        Feedback->UpdateEntrance(Feedback->EntranceRemaining*.9f);
        auto* Instance=Anim->GetActiveInstanceForMontage(Feedback->EntranceMontage);
        if(TestNotNull(TEXT("Entrance instance survives until final frame"),Instance))
            TestTrue(TEXT("No early blend back to idle"),Instance->GetWeight()>.99f);
        TestTrue(TEXT("Entrance stays protected near its end"),Feedback->IsBlockingAttacks());
        Feedback->UpdateEntrance(Feedback->EntranceRemaining+1.f); // A long frame must still evaluate the end.
        TestTrue(TEXT("Full animation evaluated"),FMath::IsNearlyEqual(Anim->GetCurrentTime(),Feedback->EntranceMontage->GetPlayLength()));
        Character->MoveCharacter(FVector2D(1,0));
        TestTrue(TEXT("Final entrance frame still blocks movement"),Character->GetPendingMovementInputVector().IsNearlyZero());
        Feedback->UpdateEntrance(0);
        TestFalse(TEXT("Protection releases after final frame"),Feedback->IsBlockingAttacks());
        TestEqual(TEXT("Original animation class restored"),Mesh->GetAnimInstance()->GetClass(),OriginalClass);
        TestTrue(TEXT("Normal mesh ticking resumes"),Mesh->IsComponentTickEnabled());
        Character->MoveCharacter(FVector2D(1,0));
        TestFalse(TEXT("Movement resumes after the full entrance"),Character->GetPendingMovementInputVector().IsNearlyZero());
        Character->ConsumeMovementInputVector();
        Feedback->PlayArrival();
        Feedback->FinishSwapFreeze();
        TestEqual(TEXT("Explicit cancellation restores the Animation Blueprint"),Mesh->GetAnimationMode(),EAnimationMode::AnimationBlueprint);
        TestFalse(TEXT("Cancellation clears entrance protection"),Feedback->IsBlockingAttacks());

        // Duplicate the fixture so asset-rate tests never modify saved tuning.
        Feedback->EntranceMontage=DuplicateObject<UAnimMontage>(Feedback->EntranceMontage,Feedback);
        for(float AssetRate : {1.f,.5f})
        for(float PlayRate : {1.f,.5f})
        for(float FreezeDuration : {0.f,.05f,.5f})
        {
            Feedback->EntranceMontage->RateScale=AssetRate;
            Feedback->EntrancePlayRate=PlayRate;
            Feedback->SwapFreezeDuration=FreezeDuration;
            Feedback->PlayArrival();
            auto* RateAnim=Mesh->GetSingleNodeInstance();
            if(!TestNotNull(TEXT("Rate fixture starts"),RateAnim)) continue;
            if(Name==TEXT("Samurai"))
                TestTrue(TEXT("Samurai walk matches full montage duration regardless of freeze or play rate"),FMath::IsNearlyEqual(
                    Feedback->ActiveMovementDuration,Feedback->EntranceRemaining));
            const float ExpectedRate=PlayRate*AssetRate;
            TestTrue(TEXT("Duration respects component and montage rates"),FMath::IsNearlyEqual(
                Feedback->EntranceRemaining,Feedback->EntranceMontage->GetPlayLength()/ExpectedRate));
            Feedback->UpdateEntrance(.02f);
            TestTrue(TEXT("First frame advances at the authored speed"),FMath::IsNearlyEqual(RateAnim->GetCurrentTime(),.02f*ExpectedRate));
            if(FreezeDuration>0) Feedback->UpdateSwapFreeze(FreezeDuration+.01f);
            Feedback->UpdateEntrance(.03f);
            TestTrue(TEXT("Freeze expiry does not change playback speed"),FMath::IsNearlyEqual(RateAnim->GetCurrentTime(),.05f*ExpectedRate));
            TestTrue(TEXT("Slow entrance remains protected after freeze"),Feedback->IsBlockingAttacks());
            Character->MoveCharacter(FVector2D(1,0));
            TestTrue(TEXT("Freeze expiry cannot release movement before entrance finishes"),Character->GetPendingMovementInputVector().IsNearlyZero());
            Feedback->FinishSwapFreeze();
        }
    }
    // A sound notify placed on a montage must survive cosmetic scrubbing.
    auto* SoundClass=LoadClass<ACharacterBase>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Ninja.BP_Ninja_C"));
    auto* SoundCharacter=World->SpawnActor<ACharacterBase>(SoundClass);
    auto* SoundFeedback=SoundCharacter->SwapPresentation.Get();
    auto* SoundMontage=DuplicateObject<UAnimMontage>(LoadObject<UAnimMontage>(nullptr,
        TEXT("/Game/HeavensDivide/Blueprints/PlayerCharacters/Montages/Ninja/AM_NinjaSwapArrival")),SoundCharacter);
    SoundMontage->Notifies.Reset();
    FAnimNotifyEvent& SoundEvent=SoundMontage->Notifies.AddDefaulted_GetRef();
    SoundEvent.Notify=NewObject<UAnimNotify_PlaySound>(SoundMontage);
    SoundEvent.Link(SoundMontage,SoundMontage->GetPlayLength()*.25f);
    SoundFeedback->EntranceMontage=SoundMontage;
    SoundFeedback->bEnableSound=true;SoundFeedback->bPlayEntranceSoundNotifies=true;
    SoundFeedback->StartEntrance();
    TestEqual(TEXT("Audio notify does not fire early"),SoundFeedback->PlayedEntranceSounds.Num(),0);
    SoundFeedback->UpdateEntrance(SoundFeedback->EntranceRemaining*.5f);
    TestEqual(TEXT("Audio notify fires when crossed"),SoundFeedback->PlayedEntranceSounds.Num(),1);
    SoundFeedback->UpdateEntrance(.001f);
    TestEqual(TEXT("Audio notify fires only once"),SoundFeedback->PlayedEntranceSounds.Num(),1);
    SoundFeedback->StopEntrance();
    World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
    return true;
}
#endif
