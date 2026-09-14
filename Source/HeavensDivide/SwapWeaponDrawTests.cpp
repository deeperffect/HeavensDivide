#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "SwapPresentationComponent.h"
#include "SamuraiCharacter.h"
#include "Animation/AnimMontage.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSwapWeaponDrawTest,"HeavensDivide.ImpactFeedback.SwapWeaponDraw",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSwapWeaponDrawTest::RunTest(const FString&)
{
    auto* World=UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    auto* Class=LoadClass<ASamuraiCharacter>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Samurai.BP_Samurai_C"));
    auto* Samurai=World->SpawnActor<ASamuraiCharacter>(Class);
    auto* Feedback=Samurai->SwapPresentation.Get();
    auto* Mesh=Samurai->GetMesh();
    auto* Grip=NewObject<USceneComponent>(Samurai,TEXT("TestHandGrip"));
    Grip->SetupAttachment(Mesh,Mesh->GetBoneName(0));Grip->RegisterComponent();
    auto* Weapon=NewObject<USceneComponent>(Samurai,TEXT("TestDrawWeapon"));
    Weapon->SetupAttachment(Grip);Weapon->RegisterComponent();
    const FTransform Original(FRotator(10,20,30),FVector(3,4,5),FVector(.2f));
    Weapon->SetRelativeTransform(Original);
    Feedback->ArrivalWeaponComponentName=Weapon->GetFName();
    Feedback->WeaponBackSocket=Mesh->GetBoneName(0); // Deterministic valid attachment for this fixture.
    Feedback->SwapFreezeDuration=0;Feedback->bEnableSound=false;
    auto* Montage=LoadObject<UAnimMontage>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/PlayerCharacters/Montages/Samurai/AM_AutoAttackSamurai"));
    Feedback->EntranceMontage=DuplicateObject<UAnimMontage>(Montage,Feedback);
    Feedback->EntranceMontage->RateScale=1;
    Feedback->EntranceMontage->Notifies.Reset();
    const float DrawTime=Montage->GetPlayLength()*.5f;
    auto& Marker=Feedback->EntranceMontage->Notifies.AddDefaulted_GetRef();
    Marker.NotifyName=TEXT("SwapDrawWeapon");Marker.Link(Feedback->EntranceMontage,DrawTime);
    Feedback->ArrivalWeaponDrawTime=0; // Marker must override the fallback.
    for(float Rate : {1.f,.5f})
    {
        Samurai->SetCharacterMode(ECharacterMode::Active);
        Feedback->EntrancePlayRate=Rate;
        Feedback->PlayArrival();
        TestEqual(TEXT("Entrance starts weapon on back attachment"),Weapon->GetAttachParent(),static_cast<USceneComponent*>(Mesh));
        TestEqual(TEXT("Correct back socket selected"),Weapon->GetAttachSocketName(),Feedback->WeaponBackSocket);
        TestTrue(TEXT("Back attachment preserves weapon scale"),Weapon->GetRelativeScale3D().Equals(Original.GetScale3D()));
        Feedback->UpdateEntrance(DrawTime*.9f/Rate);
        TestEqual(TEXT("Weapon stays on back before marker"),Weapon->GetAttachParent(),static_cast<USceneComponent*>(Mesh));
        Feedback->UpdateEntrance(DrawTime*.2f/Rate);
        TestEqual(TEXT("Crossing marker restores original hand parent"),Weapon->GetAttachParent(),Grip);
        TestTrue(TEXT("Original hand transform restored exactly"),Weapon->GetRelativeTransform().Equals(Original));
        Feedback->FinishSwapFreeze();
        Feedback->PlayArrival();
        Samurai->SetCharacterMode(ECharacterMode::Inactive);
        TestEqual(TEXT("Interrupted entrance restores weapon to hand"),Weapon->GetAttachParent(),Grip);
        TestTrue(TEXT("Interrupted entrance preserves original transform"),Weapon->GetRelativeTransform().Equals(Original));
    }
    Samurai->SetCharacterMode(ECharacterMode::Active);
    Feedback->EntranceMontage->Notifies.Reset();Feedback->ArrivalWeaponDrawTime=DrawTime;
    Feedback->PlayArrival();Feedback->UpdateEntrance(DrawTime/Feedback->EntrancePlayRate+.01f);
    TestEqual(TEXT("Fallback draw time works without marker"),Weapon->GetAttachParent(),Grip);
    Feedback->FinishSwapFreeze();
    Feedback->WeaponBackSocket=TEXT("MissingSwapSocket");
    Feedback->PlayArrival();
    TestEqual(TEXT("Missing back socket safely leaves weapon in hand"),Weapon->GetAttachParent(),Grip);
    Feedback->FinishSwapFreeze();
    World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
    return true;
}
#endif
