#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "ExperienceDropSubsystem.h"
#include "ExperiencePickupDropSettings.h"
#include "ExperiencePickup.h"
#include "ExperienceComponent.h"
#include "EnemyBase.h"
#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "SurvivorPlayerController.h"
#include "MagnetPickup.h"
#include "HealingPickupDropSettings.h"
#include "EncounterDirector.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FXPRewardGroupingTest,"HeavensDivide.Pickups.XPRewardGrouping",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FXPRewardGroupingTest::RunTest(const FString&)
{
    auto* W=UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);W->InitializeActorsForPlay(FURL());
    auto* PC=W->SpawnActor<ASurvivorPlayerController>(); auto* Party=PC->GetCharacterManager();auto* XP=PC->GetExperienceComponent();XP->RestoreRunState(100,0);
    auto* Player=W->SpawnActor<ACharacterBase>();PC->Possess(Player);Player->SetOwner(PC);
    const auto* Active=FindFProperty<FObjectProperty>(UCharacterManagerComponent::StaticClass(),TEXT("ActiveCharacter"));
    Active->SetObjectPropertyValue_InContainer(Party,Player);
    auto* Drops=W->GetSubsystem<UExperienceDropSubsystem>();
    if(!TestNotNull(TEXT("Reward index exists for this world"),Drops)){GEngine->DestroyWorldContext(W);W->DestroyWorld(false);return false;}
    auto* Class=LoadClass<AExperiencePickup>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/BP_ExperiencePickup.BP_ExperiencePickup_C"));
    if(!TestNotNull(TEXT("Saved XP orb loads"),Class)){GEngine->DestroyWorldContext(W);W->DestroyWorld(false);return false;}
    auto Orbs=[&](){TArray<AExperiencePickup*> Out;for(TActorIterator<AExperiencePickup> It(W);It;++It)if(!It->IsActorBeingDestroyed())Out.Add(*It);return Out;};
    auto Total=[&](){int32 Sum=0;for(auto* Orb:Orbs())Sum+=Orb->XPValue;return Sum;};
    auto Clear=[&](){for(auto* Orb:Orbs())Orb->Destroy();};
    auto* Floor=W->SpawnActor<AActor>();auto* Box=NewObject<UBoxComponent>(Floor);Floor->SetRootComponent(Box);
    Box->SetBoxExtent(FVector(10000,10000,10));Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Box->SetCollisionResponseToAllChannels(ECR_Block);Box->RegisterComponent();Floor->SetActorLocation(FVector(0,0,27));
    FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    int32 BaselineXP=0;
    for(int32 I=0;I<32;++I)
    {
        auto* Enemy=W->SpawnActor<AEnemyBase>(FVector(5000+(I%2)*20,0,133),FRotator::ZeroRotator,Params);
        Enemy->ExperiencePickupClass=Class;Enemy->XPReward=3;Enemy->ExperiencePickupSpawnScatterRadius=0;
        Enemy->CachedSurvivorController=PC;Enemy->ObservedCharacterManager=Party;
        Enemy->SpawnExperiencePickup();Enemy->SpawnExperiencePickup();BaselineXP+=3;
        TestEqual(TEXT("Every kill immediately preserves its exact XP"),Total(),BaselineXP);
        Enemy->Destroy(); // XP remains available after its source disappears.
    }
    TestEqual(TEXT("Thirty-two 3-XP kills leave eight pickups, 75 percent fewer"),Orbs().Num(),8);
    for(auto* Orb:Orbs()){TestEqual(TEXT("Grouped orb carries twelve XP"),Orb->XPValue,12);TestTrue(TEXT("Grouped orb rests on floor"),FMath::Abs(Orb->GetActorLocation().Z-55)<1);}
    Clear();
    // Unequal rewards, negative positions and spatial cell boundaries.
    const int32 Rewards[]={3,5,12,2,7};BaselineXP=0;
    for(int32 I=0;I<5;++I)
    {
        Drops->DropReward(Player,FVector(-5000+(I%2?-20:20),0,55),Class,Rewards[I],XP,Party);
        BaselineXP+=Rewards[I];TestEqual(TEXT("Mixed rewards and a partial final group conserve XP"),Total(),BaselineXP);
    }
    TestEqual(TEXT("Partial group is dropped immediately, with no queued remainder"),Orbs().Num(),2);
    Drops->DropReward(Player,FVector(5000,0,55),Class,3,XP,Party);
    TestEqual(TEXT("Distant kills retain their own local pickup"),Orbs().Num(),3);BaselineXP+=3;
    auto* Flight=*Orbs().FindByPredicate([](const auto* Orb){return Orb->EnemyRewardContributions==1 && Orb->XPValue==7;});
    Flight->LaunchFromChest(Flight->GetActorLocation(),1,100);
    const int32 FlightXP=Flight->XPValue;
    Drops->DropReward(Player,Flight->GetActorLocation(),Class,5,XP,Party);BaselineXP+=5;
    TestEqual(TEXT("Airborne reward is never used as an idle merge target"),Flight->XPValue,FlightXP);
    TestEqual(TEXT("Flight exclusions still preserve XP"),Total(),BaselineXP);
    auto* Magnet=W->SpawnActor<AMagnetPickup>();Magnet->Collect(PC);
    const int32 ClaimedXP=Total();
    Drops->DropReward(Player,FVector(-5000,0,55),Class,3,XP,Party);BaselineXP+=3;
    TestEqual(TEXT("New drop after magnet claim retains its full reward"),Total(),ClaimedXP+3);
    int32 Unclaimed=0;for(auto* Orb:Orbs())if(!Orb->bAttracting){++Unclaimed;TestEqual(TEXT("New reward starts its own group after the magnet"),Orb->EnemyRewardContributions,1);}
    TestEqual(TEXT("Claimed pickups cannot swallow new kills"),Unclaimed,1);
    auto* Other=W->SpawnActor<ACharacterBase>(FVector(300,300,0),FRotator::ZeroRotator,Params);PC->Possess(Other);Active->SetObjectPropertyValue_InContainer(Party,Other);
    for(auto* Orb:Orbs()){Orb->Collect();Orb->Collect();}
    TestEqual(TEXT("All combined XP is awarded exactly once after swapping characters"),XP->GetCurrentXP(),BaselineXP);
    Clear();
    for(auto Pair:{TPair<int32,bool>(0,true),TPair<int32,bool>(3,false)})
    {
        auto* Enemy=W->SpawnActor<AEnemyBase>();Enemy->ExperiencePickupClass=Class;Enemy->XPReward=Pair.Key;Enemy->bDropsXP=Pair.Value;
        Enemy->SpawnExperiencePickup();Enemy->Destroy();
    }
    TestEqual(TEXT("Disabled and zero-XP deaths remain excluded"),Orbs().Num(),0);
    auto* Original=NewObject<UExperienceComponent>(W);auto* Grouped=NewObject<UExperienceComponent>(W);
    Original->RestoreRunState(1,0);Grouped->RestoreRunState(1,0);
    for(int32 I=0;I<32;++I)Original->AddXP(3);
    for(int32 I=0;I<8;++I)Grouped->AddXP(12);
    TestEqual(TEXT("Grouping preserves level gains across thresholds"),Grouped->GetCurrentLevel(),Original->GetCurrentLevel());
    TestEqual(TEXT("Grouping preserves progress toward the next level"),Grouped->GetCurrentXP(),Original->GetCurrentXP());
    const auto* Healing=GetDefault<UHealingPickupDropSettings>();
    TestEqual(TEXT("Normal healing chance halves"),Healing->NormalDropChance,.0075f);
    TestEqual(TEXT("Elite healing chance halves"),Healing->EliteDropChance,.1f);
    TestEqual(TEXT("Boss base healing chance halves"),Healing->BossDropChance,.5f);
    TestEqual(TEXT("Healing cooldown is thirty seconds"),Healing->HealDropCooldown,30.f);
    const auto* DirectorClass=LoadClass<AEncounterDirector>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/Encounters/BP_EncounterDirector.BP_EncounterDirector_C"));
    if(TestNotNull(TEXT("Saved director loads"),DirectorClass))
    {
        const auto* D=DirectorClass->GetDefaultObject<AEncounterDirector>();
        TestEqual(TEXT("First healing urn delayed"),D->FirstUrnSeconds,36.f);
        TestEqual(TEXT("Healing interval minimum doubles"),D->UrnIntervalMin,76.f);TestEqual(TEXT("Healing interval maximum doubles"),D->UrnIntervalMax,110.f);
        TestEqual(TEXT("First magnet delayed"),D->FirstMagnetSeconds,150.f);
        TestEqual(TEXT("Magnet interval minimum doubles"),D->MagnetIntervalMin,200.f);TestEqual(TEXT("Magnet interval maximum doubles"),D->MagnetIntervalMax,280.f);
    }
    GEngine->DestroyWorldContext(W);W->DestroyWorld(false);return true;
}
#endif
