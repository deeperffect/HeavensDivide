#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PlayerUpgradeComponent.h"
#include "AutoAttackComponent.h"
#include "CharacterManagerComponent.h"
#include "CharacterStatsComponent.h"
#include "SurvivorPlayerController.h"
#include "SamuraiCharacter.h"
#include "HealthComponent.h"
#include "IaijutsuBuild.h"
#include "CrescentBuild.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSamuraiScalingConversionTest,"HeavensDivide.Combat.SamuraiScalingConversion",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSamuraiScalingConversionTest::RunTest(const FString&)
{
    auto* World=UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const auto Cleanup=[&]{World->DestroyWorld(false);GEngine->DestroyWorldContext(World);};
    auto* Class=LoadClass<ASurvivorPlayerController>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController.BP_SurvivorPlayerController_C"));
    auto* PC=World->SpawnActor<ASurvivorPlayerController>(Class);
    if(!TestNotNull(TEXT("Saved controller"),PC)){Cleanup();return false;}
    PC->GetPlayerHealthComponent()->RestoreCurrentHealth(100);
    auto* U=PC->GetPlayerUpgrades();
    auto* Samurai=World->SpawnActor<ASamuraiCharacter>();
    if(!TestNotNull(TEXT("Samurai"),Samurai)){Cleanup();return false;}
    Samurai->SetOwner(PC);
    FindFProperty<FObjectProperty>(UCharacterManagerComponent::StaticClass(),TEXT("SamuraiCharacter"))->SetObjectPropertyValue_InContainer(PC->GetCharacterManager(),Samurai);
    auto* Stats=Samurai->GetCharacterStats();
    const FName Groups[3][3]={
        {TEXT("SamuraiHeavyBlade"),TEXT("SamuraiTempo"),TEXT("SamuraiArea")},
        {TEXT("IaijutsuDamage"),TEXT("IaijutsuChargeSpeed"),TEXT("IaijutsuWidth")},
        {TEXT("CrescentDamage"),TEXT("CrescentSpeed"),TEXT("CrescentRange")}};
    const FName Stances[]={TEXT("BattleStance"),TEXT("Iaijutsu"),TEXT("BladeWave")};
    auto Card=[&](FName Id){return U->FindUpgradeDefinition(Id);};
    for(const auto& Group:Groups)for(FName Id:Group)
        if(!TestNotNull(*Id.ToString(),Card(Id))){Cleanup();return false;}
    const float Common[3][3]={{.2f,.1f,.15f},{.2f,.15f,.25f},{.2f,.2f,.2f}};
    const float Rare[]={.3f,.15f,.25f};
    FPlayerUpgradeRunState Empty;U->CaptureRunState(Empty);
    for(int32 Route=0;Route<3;++Route)
    {
        U->RestoreRunState(Empty);
        for(int32 Stat=0;Stat<3;++Stat)
        {
            auto* Generic=Card(Groups[0][Stat]);
            TestTrue(TEXT("All three ordinary upgrades offered before a stance"),U->GetEligibleUpgradesForCategory(EUpgradeCategory::Samurai).Contains(Generic));
            TestEqual(TEXT("Shared investment has five ranks"),Generic->MaxLevel,5);
            TestTrue(TEXT("Acquire ordinary rank"),U->AcquireUpgrade(Generic));
            TestTrue(TEXT("Acquire Rare rank"),U->AcquireUpgradeResolved(Generic,Rare[Stat],EUpgradeRarity::Rare));
        }
        TestTrue(TEXT("Damage is ordinary damage without a speed penalty"),FMath::IsNearlyEqual(Stats->GetFinalDamageMultiplier(),1.5f));
        TestTrue(TEXT("Pre-stance speed applies"),FMath::IsNearlyEqual(Stats->GetFinalAttackSpeedMultiplier(),1.25f));
        TestTrue(TEXT("Pre-stance area applies"),FMath::IsNearlyEqual(Stats->GetFinalAttackAreaMultiplier(),1.4f));
        FPlayerUpgradeRunState Before;U->CaptureRunState(Before);U->RestoreRunState(Before);
        TestEqual(TEXT("All six ordinary purchases award Samurai mastery"),Before.SamuraiMastery,6);
        for(FName Id:Groups[0])TestEqual(TEXT("Pre-stance save retains invested ranks"),U->GetUpgradeLevelById(Id),2);
        TestTrue(TEXT("Trial opens"),U->BeginDirectCategoryUpgradeSelection(EUpgradeCategory::SamuraiTrial,3));
        TestTrue(TEXT("Selecting stance converts existing investment"),U->SelectUpgrade(Card(Stances[Route])));
        TestEqual(TEXT("Only stance acquisition adds mastery"),U->GetSamuraiMasteryPoints(),Before.SamuraiMastery+1);
        for(int32 Stat=0;Stat<3;++Stat)
        {
            const FName Id=Groups[Route][Stat];
            const float Expected=(Common[0][Stat]+Rare[Stat])*Common[Route][Stat]/Common[0][Stat];
            TestEqual(TEXT("Two ranks transfer to selected form"),U->GetUpgradeLevelById(Id),2);
            TestTrue(TEXT("Rarity strength transfers proportionally to stance tuning"),FMath::IsNearlyEqual(U->GetAccumulatedUpgradeMagnitude(Id),Expected));
            TestTrue(TEXT("Converted card can continue levelling"),U->AcquireUpgrade(Card(Id)));
            TestEqual(TEXT("Next purchase advances same investment"),U->GetUpgradeLevelById(Id),3);
            TestTrue(TEXT("Next purchase keeps carried bonus"),FMath::IsNearlyEqual(U->GetAccumulatedUpgradeMagnitude(Id),Expected+Common[Route][Stat]));
            if(Route==1)TestTrue(TEXT("Iaijutsu consumes converted magnitude"),FMath::IsNearlyEqual(IaijutsuBuild::Scaling(U,Id,Common[Route][Stat]),Expected+Common[Route][Stat]));
            if(Route==2)TestTrue(TEXT("Crescent consumes converted magnitude"),FMath::IsNearlyEqual(CrescentBuild::Scaling(U,Id,Common[Route][Stat]),Expected+Common[Route][Stat]));
            for(int32 Other=0;Other<3;++Other)if(Other!=Route)
            {
                TestEqual(TEXT("Other forms do not retain duplicate ranks"),U->GetUpgradeLevelById(Groups[Other][Stat]),0);
                TestFalse(TEXT("Other form cannot double-dip"),U->CanAcquireUpgrade(Card(Groups[Other][Stat])));
            }
        }
        if(Route!=0)
        {
            TestEqual(TEXT("Converted damage does not also apply old melee modifier"),Stats->GetFinalDamageMultiplier(),1.f);
            TestEqual(TEXT("Converted speed does not also accelerate attack cooldown"),Stats->GetFinalAttackSpeedMultiplier(),1.f);
            TestEqual(TEXT("Converted size does not also apply melee area"),Stats->GetFinalAttackAreaMultiplier(),1.f);
        }
        FPlayerUpgradeRunState Saved;U->CaptureRunState(Saved);
        U->RestoreRunState(Saved);U->RestoreRunState(Saved);
        for(FName Id:Groups[Route])
        {
            TestEqual(TEXT("Restore retains converted level"),U->GetUpgradeLevelById(Id),Saved.Levels[Id]);
            TestTrue(TEXT("Restore cannot duplicate conversion strength"),FMath::IsNearlyEqual(U->GetAccumulatedUpgradeMagnitude(Id),Saved.AccumulatedMagnitudes[Id]));
        }
        TestEqual(TEXT("Restore retains mastery exactly"),U->GetSamuraiMasteryPoints(),Saved.SamuraiMastery);
        // A banished pre-stance upgrade must remain banished in its chosen form.
        FPlayerUpgradeRunState Banished=Before;Banished.BanishedUpgrades.Add(TEXT("SamuraiArea"));
        U->RestoreRunState(Banished);U->AcquireUpgrade(Card(Stances[Route]));
        TestFalse(TEXT("Banishment follows the investment"),U->CanAcquireUpgrade(Card(Groups[Route][2])));
        TestEqual(TEXT("Banishing does not erase owned ranks"),U->GetUpgradeLevelById(Groups[Route][2]),2);
        U->RestoreRunState(Empty);
        for(FName Id:Groups[0])for(int32 Rank=0;Rank<5;++Rank)TestTrue(TEXT("Acquire shared rank up to cap"),U->AcquireUpgrade(Card(Id)));
        U->AcquireUpgrade(Card(Stances[Route]));
        for(FName Id:Groups[Route])
        {
            TestEqual(TEXT("Five ranks survive stance selection"),U->GetUpgradeLevelById(Id),5);
            TestFalse(TEXT("Choosing stance does not reset rank cap"),U->CanAcquireUpgrade(Card(Id)));
        }
    }
    FPlayerUpgradeRunState Legacy=Empty;
    Legacy.Levels.Add(TEXT("SamuraiHeavyBlade"),1);Legacy.Definitions.Add(TEXT("SamuraiHeavyBlade"),Card(TEXT("SamuraiHeavyBlade")));
    U->RestoreRunState(Legacy);
    TestTrue(TEXT("Old Heavy Blade keeps 40 percent damage"),FMath::IsNearlyEqual(Stats->GetFinalDamageMultiplier(),1.4f));
    TestEqual(TEXT("Old Heavy Blade speed penalty is retired"),Stats->GetFinalAttackSpeedMultiplier(),1.f);
    U->AcquireUpgrade(Card(TEXT("Iaijutsu")));
    TestTrue(TEXT("Legacy damage investment converts too"),FMath::IsNearlyEqual(IaijutsuBuild::Scaling(U,TEXT("IaijutsuDamage"),.2f),.4f,.0001f));
    FPlayerUpgradeRunState Mixed=Empty;Mixed.SamuraiMastery=8;
    for(FName Id:{FName(TEXT("SamuraiArea")),FName(TEXT("IaijutsuWidth")),FName(TEXT("Iaijutsu"))})Mixed.Definitions.Add(Id,Card(Id));
    Mixed.Levels.Add(TEXT("SamuraiArea"),4);Mixed.AccumulatedMagnitudes.Add(TEXT("SamuraiArea"),.6f);
    Mixed.Levels.Add(TEXT("IaijutsuWidth"),3);Mixed.Levels.Add(TEXT("Iaijutsu"),1);
    U->RestoreRunState(Mixed);
    TestEqual(TEXT("Mixed saves retain the shared rank ceiling"),U->GetUpgradeLevelById(TEXT("IaijutsuWidth")),5);
    TestTrue(TEXT("Mixed saves preserve all invested bonus strength"),FMath::IsNearlyEqual(IaijutsuBuild::Scaling(U,TEXT("IaijutsuWidth"),.25f),1.75f,.0001f));
    TestEqual(TEXT("Mixed save conversion does not award or delete mastery"),U->GetSamuraiMasteryPoints(),8);
    Cleanup();return true;
}
#endif
