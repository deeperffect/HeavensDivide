#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "SwapAfterimage.h"
#include "SwapPresentationComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSwapDepartureTest,"HeavensDivide.ImpactFeedback.SwapDeparture",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSwapDepartureTest::RunTest(const FString&)
{
    auto* World=UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    for(const FString Name : {FString(TEXT("Ninja")),FString(TEXT("Samurai"))})
    {
        const FString Path=TEXT("/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_")+Name+TEXT(".BP_")+Name+TEXT("_C");
        auto* Class=LoadClass<ACharacterBase>(nullptr,*Path);
        FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Source=World->SpawnActor<ACharacterBase>(Class,FVector::ZeroVector,FRotator::ZeroRotator,Params);
        if(!TestNotNull(Name+TEXT(" source"),Source)) continue;
        // Existing compatible montages are fixtures only, not character default changes.
        auto* Montage=LoadObject<UAnimMontage>(nullptr,Name==TEXT("Ninja") ?
            TEXT("/Game/HeavensDivide/Blueprints/PlayerCharacters/Montages/Ninja/AM_NinjaSwapArrival") :
            TEXT("/Game/HeavensDivide/Blueprints/PlayerCharacters/Montages/Samurai/AM_AutoAttackSamurai"));
        if(!TestNotNull(Name+TEXT(" montage fixture"),Montage)) continue;
        Montage=DuplicateObject<UAnimMontage>(Montage,Source);
        Montage->RateScale=1;
        Source->SwapPresentation->DepartureMontage=Montage;
        Source->SwapPresentation->DeparturePlayRate=1;
        // This section verifies the legacy fade; portal movement is tested below.
        // Do not inherit portal assignments from the user's saved Blueprint tuning.
        Source->SwapPresentation->DeparturePortal=nullptr;
        Source->SetCharacterMode(ECharacterMode::Inactive);
        Source->SwapPresentation->PlayDeparture();
        ASwapAfterimage* Copy=nullptr;
        for(TActorIterator<ASwapAfterimage> It(World);It;++It)
            if(It->GetOwner()==Source && !It->IsActorBeingDestroyed()) Copy=*It;
        if(!TestNotNull(Name+TEXT(" departure copy"),Copy)) continue;
        if(!TestNotNull(Name+TEXT(" animated mesh"),Copy->AnimatedMesh.Get())) continue;
        auto* Anim=Copy->AnimatedMesh->GetSingleNodeInstance();
        TestEqual(TEXT("Copy plays assigned montage"),Anim->GetAnimationAsset(),static_cast<UAnimationAsset*>(Montage));
        TestEqual(TEXT("Departure starts with normal materials"),Copy->AnimatedMesh->GetMaterial(0),Source->GetMesh()->GetMaterial(0));
        Copy->AdvanceVisual(.1f);
        TestEqual(TEXT("Normal materials remain during montage"),Copy->AnimatedMesh->GetMaterial(0),Source->GetMesh()->GetMaterial(0));
        TestTrue(TEXT("Departure advances in real time"),Anim->GetCurrentTime()>.05f);
        TestTrue(TEXT("Outgoing gameplay character stays hidden"),Source->IsHidden());
        TestFalse(TEXT("Outgoing gameplay collision stays disabled"),Source->GetActorEnableCollision());
        TestFalse(TEXT("Copy never has gameplay collision"),Copy->GetActorEnableCollision());
        TestTrue(TEXT("Montage cannot move the proxy actor"),Copy->GetActorLocation().IsNearlyZero());
        Copy->AdvanceVisual(Copy->DepartureDuration-.1f+.001f);
        TestTrue(TEXT("Departure reaches its final frame"),FMath::IsNearlyEqual(Anim->GetCurrentTime(),Montage->GetPlayLength()));
        TestEqual(TEXT("Color starts after montage completion"),Copy->AnimatedMesh->GetMaterial(0),static_cast<UMaterialInterface*>(Copy->FadeMaterial));
        TestFalse(TEXT("Completed montage retains a separate fade tail"),Copy->IsActorBeingDestroyed());
        Copy->AdvanceVisual(Copy->Lifetime-Copy->Age+.01f);
        TestTrue(TEXT("Departure copy expires"),Copy->IsActorBeingDestroyed());

        for(float AssetRate : {1.f,.5f})
        for(float PlayRate : {1.f,.5f})
        for(float WorldSpeed : {1.f,.0001f})
        {
            Montage->RateScale=AssetRate;
            World->GetWorldSettings()->SetTimeDilation(WorldSpeed);
            auto* RateCopy=World->SpawnActor<ASwapAfterimage>();
            if(!TestTrue(TEXT("Rate fixture initializes"),RateCopy->InitializeDeparture(Source,
                Source->SwapPresentation->GhostMaterial,Source->SwapPresentation->GetPresentationColor(),Montage,PlayRate,.2f))) continue;
            const float ExpectedRate=AssetRate*PlayRate;
            TestTrue(TEXT("Full duration follows authored speed"),FMath::IsNearlyEqual(
                RateCopy->DepartureDuration,Montage->GetPlayLength()/ExpectedRate));
            RateCopy->AdvanceVisual(.1f);
            TestTrue(TEXT("Normal and slow departure rates are honored during and outside freeze"),FMath::IsNearlyEqual(
                RateCopy->AnimatedMesh->GetSingleNodeInstance()->GetCurrentTime(),.1f*ExpectedRate));
            TestEqual(TEXT("Slower departure keeps original materials"),RateCopy->AnimatedMesh->GetMaterial(0),Source->GetMesh()->GetMaterial(0));
            RateCopy->AdvanceVisual(RateCopy->DepartureDuration-RateCopy->Age+.001f);
            TestEqual(TEXT("Color waits for full duration at every rate"),RateCopy->AnimatedMesh->GetMaterial(0),static_cast<UMaterialInterface*>(RateCopy->FadeMaterial));
            TestFalse(TEXT("Fade tail survives slow montage completion"),RateCopy->IsActorBeingDestroyed());
            RateCopy->AdvanceVisual(.21f);
            TestTrue(TEXT("Slow departure cleans up after fade"),RateCopy->IsActorBeingDestroyed());
        }
        World->GetWorldSettings()->SetTimeDilation(1.f);
        auto* PortalCopy=World->SpawnActor<ASwapAfterimage>();
        if(TestTrue(TEXT("Portal departure initializes"),PortalCopy->InitializeDeparture(Source,
            Source->SwapPresentation->GhostMaterial,Source->SwapPresentation->GetPresentationColor(),Montage,1.f,.2f)))
        {
            const FVector Start=PortalCopy->GetActorLocation();
            const FVector Destination=Start+FVector(Name==TEXT("Ninja") ? 180.f : -180.f,0,0);
            const FVector GameplayLocation=Source->GetActorLocation();
            PortalCopy->SetPortalDestination(Destination);
            PortalCopy->AdvanceVisual(PortalCopy->DepartureDuration*.5f);
            TestTrue(TEXT("Portal dash advances toward its destination"),PortalCopy->GetActorLocation().Equals(FMath::Lerp(Start,Destination,.25f),.01f));
            TestTrue(TEXT("Portal dash leaves gameplay position unchanged"),Source->GetActorLocation().Equals(GameplayLocation));
            PortalCopy->AdvanceVisual(PortalCopy->DepartureDuration);
            TestTrue(TEXT("Portal dash reaches exact destination"),PortalCopy->GetActorLocation().Equals(Destination,.01f));
            TestTrue(TEXT("Departure disappears at portal instead of fading outside it"),PortalCopy->IsActorBeingDestroyed());
        }
    }
    World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
    return true;
}
#endif
