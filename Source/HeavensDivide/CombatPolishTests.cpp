#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "MagnetPickup.h"
#include "ExperiencePickup.h"
#include "ExperienceComponent.h"
#include "CharacterManagerComponent.h"
#include "SamuraiCharacter.h"
#include "NinjaCharacter.h"
#include "SwapPresentationComponent.h"
#include "SurvivorPlayerController.h"
#include "HealingUrn.h"
#include "HealingPickup.h"
#include "HealthComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "UObject/UnrealType.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace CombatPolishTests
{
UWorld* World()
{
    auto* W=UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);W->InitializeActorsForPlay(FURL());return W;
}
void SetParty(UCharacterManagerComponent* P,FName Field,ACharacterBase* C)
{ FindFProperty<FObjectProperty>(UCharacterManagerComponent::StaticClass(),Field)->SetObjectPropertyValue_InContainer(P,C); }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSwapPlacementRegressionTest,"HeavensDivide.ImpactFeedback.SwapPlacement",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSwapPlacementRegressionTest::RunTest(const FString&)
{
    using namespace CombatPolishTests;
    auto* W=World();auto* PC=W->SpawnActor<ASurvivorPlayerController>();auto* Party=PC->GetCharacterManager();
    FActorSpawnParameters P;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;P.Owner=PC;
    auto* S=W->SpawnActor<ASamuraiCharacter>(FVector(300,400,500),FRotator::ZeroRotator,P);
    auto* N=W->SpawnActor<ANinjaCharacter>(FVector(-1000,0,-6000),FRotator::ZeroRotator,P);
    S->SwapPresentation->bEnabled=false;N->SwapPresentation->bEnabled=false;
    SetParty(Party,TEXT("SamuraiCharacter"),S);SetParty(Party,TEXT("NinjaCharacter"),N);SetParty(Party,TEXT("ActiveCharacter"),S);
    N->SetCharacterMode(ECharacterMode::Inactive);
    TestFalse(TEXT("Inactive movement component cannot tick gravity"),N->GetCharacterMovement()->IsComponentTickEnabled());
    TestEqual(TEXT("Inactive movement is disabled"),N->GetCharacterMovement()->MovementMode.GetValue(),MOVE_None);
    N->SetCharacterMode(ECharacterMode::Assisting);
    TestFalse(TEXT("Assist cannot fall through disabled collision"),N->GetCharacterMovement()->IsComponentTickEnabled());
    N->SetCharacterMode(ECharacterMode::Inactive);
    N->GetCapsuleComponent()->SetCapsuleHalfHeight(110);
    S->SetCharacterMode(ECharacterMode::Active);PC->Possess(S);
    S->GetCharacterMovement()->SetMovementMode(MOVE_Falling);S->GetCharacterMovement()->Velocity=FVector(75,0,-55);
    N->GetCharacterMovement()->Velocity=FVector(0,0,-4000); // Simulate a stale pre-fix hidden pawn.
    Party->SwapCharacter();
    TestTrue(TEXT("Swap possesses Ninja"),PC->GetPawn()==N);
    TestTrue(TEXT("Swap transfers feet without a side offset"),N->GetActorLocation().Equals(FVector(300,400,522),.01));
    TestTrue(TEXT("Swap discards stale hidden vertical velocity"),N->GetCharacterMovement()->Velocity.Equals(FVector(75,0,-55)));
    TestFalse(TEXT("Departed Samurai collision is disabled"),S->GetActorEnableCollision());
    TestFalse(TEXT("Departed Samurai cannot accumulate falling velocity"),S->GetCharacterMovement()->IsComponentTickEnabled());
    for(int32 I=0;I<12;++I)Party->SwapCharacter();
    TestTrue(TEXT("Repeated swaps do not accumulate an offset"),N->GetActorLocation().Equals(FVector(300,400,522),.01));
    W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMagnetRewardTest,"HeavensDivide.Pickups.Magnet",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMagnetRewardTest::RunTest(const FString&)
{
    using namespace CombatPolishTests;
    auto* W=World();auto* PC=W->SpawnActor<ASurvivorPlayerController>();auto* Party=PC->GetCharacterManager();auto* XP=PC->GetExperienceComponent();
    XP->RestoreRunState(100,0);
    FActorSpawnParameters P;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* S=W->SpawnActor<ASamuraiCharacter>(FVector::ZeroVector,FRotator::ZeroRotator,P);
    auto* N=W->SpawnActor<ANinjaCharacter>(FVector(300,300,0),FRotator::ZeroRotator,P);
    PC->Possess(S);SetParty(Party,TEXT("ActiveCharacter"),S);
    TArray<AExperiencePickup*> Orbs;
    for(int32 I=0;I<3;++I)
    {
        auto* Orb=W->SpawnActor<AExperiencePickup>(FVector(10000*(I+1),0,0),FRotator::ZeroRotator,P);
        Orb->InitializePickup(7+I,XP,Party);Orbs.Add(Orb);
    }
    Orbs[1]->LaunchFromChest(FVector(5000,0,0),2,100);
    auto* Heal=W->SpawnActor<AHealingPickup>(FVector(2000,0,0),FRotator::ZeroRotator,P);
    auto* Magnet=W->SpawnActor<AMagnetPickup>();
    TestEqual(TEXT("Magnet claims every distant XP reward"),Magnet->Collect(PC),3);
    TestEqual(TEXT("Repeated collection cannot trigger a second magnet"),Magnet->Collect(PC),0);
    TestFalse(TEXT("Magnet interrupts chest reward flight"),Orbs[1]->bRewardFlight);
    for(auto* Orb:Orbs)TestEqual(TEXT("Magnet orbs disable overlaps while flying"),Orb->PickupCollision->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
    SetParty(Party,TEXT("ActiveCharacter"),N);PC->Possess(N);
    for(int32 Frame=0;Frame<150;++Frame)for(auto* Orb:Orbs)if(!Orb->bCollected)Orb->Tick(1.f/60);
    for(auto* Orb:Orbs){TestTrue(TEXT("Far XP arrives promptly after character swap"),Orb->bCollected);Orb->Collect();}
    TestEqual(TEXT("Magnet conserves all XP exactly once"),XP->GetCurrentXP(),24);
    TestFalse(TEXT("Magnet leaves healing pickups available"),Heal->IsActorBeingDestroyed());
    auto* UrnClass=LoadClass<AHealingUrn>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/Encounters/BP_MagnetUrn.BP_MagnetUrn_C"));
    if(TestNotNull(TEXT("Magnet urn is installed"),UrnClass))
    {
        auto* Urn=W->SpawnActor<AHealingUrn>(UrnClass,FVector(1000,0,0),FRotator::ZeroRotator,P);
        TestTrue(TEXT("New urn is configured for magnet reward"),Urn->bDropsMagnet);
        Urn->DispatchBeginPlay();Urn->GetHealthComponent()->SetMaxHealthPreservePercent(1000000);
        TestTrue(TEXT("Tiny Samurai hit breaks magnet urn regardless of health scaling"),Urn->ApplyPlayerDamage(.001f,EPlayerAttackSource::Samurai));
        Urn->ApplyPlayerDamage(10000,EPlayerAttackSource::Ninja);
        int32 Count=0;for(TActorIterator<AMagnetPickup> It(W);It;++It)if(!It->IsActorBeingDestroyed())++Count;
        TestTrue(TEXT("Both character weapons can break the urn"),Urn->IsDead());TestEqual(TEXT("Breaking creates one magnet"),Count,1);
    }
    W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
}
#endif
