#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "SwapAfterimage.h"
#include "SwapPresentationComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/SkeletalMesh.h"
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
        // Read the saved usage before a render proxy can auto-enable it in the editor.
        // A slot can hold the correct MID while the renderer substitutes opaque grey
        // for clothing, so pointer equality alone cannot catch this regression.
        auto* GhostMaterial=Source->SwapPresentation->GhostMaterial.Get();
        if(!TestNotNull(Name+TEXT(" ghost material"),GhostMaterial)) continue;
        TestTrue(Name+TEXT(" saved ghost supports skeletal meshes"),GhostMaterial->GetUsageByFlag(MATUSAGE_SkeletalMesh));
        TestTrue(Name+TEXT(" saved ghost supports cloth sections"),GhostMaterial->GetUsageByFlag(MATUSAGE_Clothing));
        TestTrue(Name+TEXT(" fixture includes simulated clothing"),!Source->GetMesh()->GetSkeletalMeshAsset()->GetMeshClothingAssets().IsEmpty());
        const auto CheckFadeMaterials=[&](ASwapAfterimage* Afterimage,const FString& Phase)
        {
            TestEqual(Name+Phase+TEXT(" uses the saved material as a valid parent"),Afterimage->FadeMaterial->Parent.Get(),GhostMaterial);
            TestTrue(Name+Phase+TEXT(" fade supports clothing"),Afterimage->FadeMaterial->GetUsageByFlag(MATUSAGE_Clothing));
            TestEqual(Name+Phase+TEXT(" preserves character tint"),Afterimage->FadeMaterial->K2_GetVectorParameterValue(TEXT("Tint")),Source->SwapPresentation->GetPresentationColor());
            TInlineComponentArray<USkinnedMeshComponent*> Meshes(Afterimage);
            TestFalse(Name+Phase+TEXT(" has a character mesh"),Meshes.IsEmpty());
            for(auto* Mesh:Meshes)
            {
                TestEqual(Name+Phase+TEXT(" retains body and cloth slots"),Mesh->GetNumMaterials(),Source->GetMesh()->GetNumMaterials());
                for(int32 Slot=0;Slot<Mesh->GetNumMaterials();++Slot)
                    TestEqual(Name+Phase+FString::Printf(TEXT(" slot %d shares fade opacity and tint"),Slot),
                        Mesh->GetMaterial(Slot),static_cast<UMaterialInterface*>(Afterimage->FadeMaterial));
            }
            TInlineComponentArray<UStaticMeshComponent*> Weapons(Afterimage);
            for(auto* Weapon:Weapons)
            {
                TestTrue(Name+Phase+TEXT(" weapon copy supports translucent rendering"),Weapon->IsDisallowNanite());
                for(int32 Slot=0;Slot<Weapon->GetNumMaterials();++Slot)
                    TestEqual(Name+Phase+TEXT(" weapon shares character fade"),Weapon->GetMaterial(Slot),static_cast<UMaterialInterface*>(Afterimage->FadeMaterial));
            }
        };
        auto* Snapshot=World->SpawnActor<ASwapAfterimage>();
        Snapshot->Initialize(Source,GhostMaterial,Source->SwapPresentation->GetPresentationColor(),.2f);
        Snapshot->AdvanceVisual(.1f);
        CheckFadeMaterials(Snapshot,TEXT(" static snapshot"));
        TestTrue(Name+TEXT(" snapshot fades halfway"),FMath::IsNearlyEqual(Snapshot->FadeMaterial->K2_GetScalarParameterValue(TEXT("Opacity")),.65f*.25f,.0001f));
        Snapshot->AdvanceVisual(.11f);
        TestTrue(Name+TEXT(" snapshot body and cloth expire together"),Snapshot->IsActorBeingDestroyed());
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
        for(int32 Slot=0;Slot<Source->GetMesh()->GetNumMaterials();++Slot)
            TestEqual(TEXT("Departure starts with normal body and cloth materials"),Copy->AnimatedMesh->GetMaterial(Slot),Source->GetMesh()->GetMaterial(Slot));
        Copy->AdvanceVisual(.1f);
        for(int32 Slot=0;Slot<Source->GetMesh()->GetNumMaterials();++Slot)
            TestEqual(TEXT("Normal body and cloth materials remain during montage"),Copy->AnimatedMesh->GetMaterial(Slot),Source->GetMesh()->GetMaterial(Slot));
        TestTrue(TEXT("Departure advances in real time"),Anim->GetCurrentTime()>.05f);
        TestTrue(TEXT("Outgoing gameplay character stays hidden"),Source->IsHidden());
        TestFalse(TEXT("Outgoing gameplay collision stays disabled"),Source->GetActorEnableCollision());
        TestFalse(TEXT("Copy never has gameplay collision"),Copy->GetActorEnableCollision());
        TestTrue(TEXT("Montage cannot move the proxy actor"),Copy->GetActorLocation().IsNearlyZero());
        Copy->AdvanceVisual(Copy->DepartureDuration-.1f+.001f);
        TestTrue(TEXT("Departure reaches its final frame"),FMath::IsNearlyEqual(Anim->GetCurrentTime(),Montage->GetPlayLength()));
        TestEqual(TEXT("Color starts after montage completion"),Copy->AnimatedMesh->GetMaterial(0),static_cast<UMaterialInterface*>(Copy->FadeMaterial));
        TestFalse(TEXT("Completed montage retains a separate fade tail"),Copy->IsActorBeingDestroyed());
        Copy->AdvanceVisual((Copy->Lifetime-Copy->DepartureDuration)*.5f-.001f);
        CheckFadeMaterials(Copy,TEXT(" animated departure"));
        TestTrue(Name+TEXT(" departure fades halfway"),FMath::IsNearlyEqual(Copy->FadeMaterial->K2_GetScalarParameterValue(TEXT("Opacity")),.65f*.25f,.0001f));
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
            PortalCopy->EnablePortalTrail(.16f);
            PortalCopy->AdvanceVisual(PortalCopy->DepartureDuration*.5f);
            TestEqual(TEXT("A long frame emits one trail sample without a burst of copies"),PortalCopy->TrailCount,1);
            int32 TrailSamples=0;
            for(TActorIterator<ASwapAfterimage> It(World);It;++It)
                if(It->bRealTimeFade && !It->IsActorBeingDestroyed())
                {
                    ++TrailSamples;
                    TestFalse(TEXT("Trail samples have no collision"),It->GetActorEnableCollision());
                    It->AdvanceVisual(.08f);
                    CheckFadeMaterials(*It,TEXT(" portal trail"));
                    TestTrue(Name+TEXT(" trail fades halfway"),FMath::IsNearlyEqual(It->FadeMaterial->K2_GetScalarParameterValue(TEXT("Opacity")),.35f*.25f,.0001f));
                    It->AdvanceVisual(.2f);
                    TestTrue(TEXT("Trail samples expire independently"),It->IsActorBeingDestroyed());
                }
            TestEqual(TEXT("Portal dash produces a visible trail actor"),TrailSamples,1);
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
