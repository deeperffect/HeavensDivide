#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "SwapPresentationComponent.h"
#include "SwapAfterimage.h"
#include "NinjaCharacter.h"
#include "SamuraiCharacter.h"
#include "PlayerHUDWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSwapPresentationTest,"HeavensDivide.ImpactFeedback.SwapPresentation",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSwapPresentationTest::RunTest(const FString&)
{
    auto* World=UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    auto Class=LoadClass<ANinjaCharacter>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Ninja.BP_Ninja_C"));
    FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Ninja=World->SpawnActor<ANinjaCharacter>(Class,FVector::ZeroVector,FRotator::ZeroRotator,Params);
    auto* Samurai=World->SpawnActor<ASamuraiCharacter>(FVector(1000,0,0),FRotator::ZeroRotator,Params);
    if(!TestNotNull(TEXT("Ninja spawned"),Ninja) || !TestNotNull(TEXT("Samurai spawned"),Samurai))
    { World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return false; }
    auto* Feedback=Ninja->SwapPresentation.Get();
    TestNotNull(TEXT("Saved Ninja inherits presentation component"),Feedback);
    TestNotNull(TEXT("Ghost material is assigned"),Feedback->GhostMaterial.Get());
    TestNotEqual(TEXT("Character colors are distinct"),Feedback->GetPresentationColor(),Samurai->SwapPresentation->GetPresentationColor());
    auto* Material=Ninja->GetMesh()->GetMaterial(0);
    Ninja->SetCharacterMode(ECharacterMode::Assisting);
    TestEqual(TEXT("Tag Team preserves normal material"),Ninja->GetMesh()->GetMaterial(0),Material);
    Ninja->SetCharacterMode(ECharacterMode::Active);
    TestEqual(TEXT("Normal character material restored"),Ninja->GetMesh()->GetMaterial(0),Material);
    const FVector Position=Ninja->GetActorLocation();
    Feedback->bEnableSound=false;
    Feedback->SwapFreezeDuration=0; // This test isolates the cosmetic presentation.
    Feedback->bEnableNinjaArrivalDrop=false;
    Feedback->EntranceMontage=nullptr;
    Feedback->DepartureMontage=nullptr;
    Feedback->PlayArrival();
    Feedback->TickComponent(.3f,LEVELTICK_All,nullptr);
    TestTrue(TEXT("Presentation does not move the character"),Ninja->GetActorLocation().Equals(Position));
    TestTrue(TEXT("Active collision remains enabled"),Ninja->GetActorEnableCollision());
    TestFalse(TEXT("No readiness or animation means no idle tick"),Feedback->IsComponentTickEnabled());
    Feedback->PlayDeparture();
    bool FoundGhost=false;
    for(TActorIterator<ASwapAfterimage> It(World);It;++It)
    {
        FoundGhost=true;
        TestFalse(TEXT("Afterimage has no gameplay collision"),It->GetActorEnableCollision());
        It->Tick(1);
        TestTrue(TEXT("Afterimage expires"),It->IsActorBeingDestroyed());
    }
    TestTrue(TEXT("Departure created an afterimage"),FoundGhost);

    auto* HUD=NewObject<UPlayerHUDWidget>();
    HUD->WidgetTree=NewObject<UWidgetTree>(HUD);
    auto* Portrait=HUD->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),TEXT("IMG_IconNinja"));
    HUD->WidgetTree->RootWidget=Portrait;
    HUD->StartSwapPortraitPulse(Ninja);
    TestTrue(TEXT("Incoming portrait enlarges"),Portrait->GetRenderTransform().Scale.X>1);
    HUD->UpdateSwapPortraitPulse(1);
    TestEqual(TEXT("Portrait scale restores"),Portrait->GetRenderTransform().Scale,FVector2D(1,1));
    TestEqual(TEXT("Portrait color restores"),Portrait->GetColorAndOpacity(),FLinearColor::White);
    World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
    return true;
}
#endif
