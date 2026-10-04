#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PlayerUpgradeComponent.h"
#include "SurvivorPlayerController.h"
#include "HealthComponent.h"
#include "UpgradeDefinition.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSamuraiStanceBalanceTest,"HeavensDivide.Combat.SamuraiStanceBalance",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSamuraiStanceBalanceTest::RunTest(const FString&)
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
    auto Card=[&](FName Id){return U->FindUpgradeDefinition(Id);};
    const FName Frequency[][3]={
        {TEXT("BattleStance"),TEXT("DoubleCut"),TEXT("DoubleCutFrequency")},
        {TEXT("Iaijutsu"),TEXT("IaijutsuDoubleCut"),TEXT("IaijutsuDoubleCutFrequency")},
        {TEXT("BladeWave"),TEXT("CrescentDoubleCut"),TEXT("CrescentDoubleCutFrequency")}};
    const FName Tradeoffs[][4]={
        {TEXT("Iaijutsu"),TEXT("IaijutsuMarkDamage"),TEXT("IaijutsuDashPact"),TEXT("IaijutsuDamage")},
        {TEXT("BladeWave"),TEXT("CrescentSlow"),TEXT("CrescentEruptionPact"),TEXT("CrescentDamage")},
        {TEXT("BladeWave"),TEXT("CrescentSlowDuration"),TEXT("CrescentEruptionPact"),TEXT("CrescentDamage")}};
    for(const auto& Row:Frequency)for(FName Id:Row)
        if(!TestNotNull(*Id.ToString(),Card(Id))){Cleanup();return false;}
    for(const auto& Row:Tradeoffs)for(FName Id:Row)
        if(!TestNotNull(*Id.ToString(),Card(Id))){Cleanup();return false;}
    if(!Card(TEXT("IaijutsuMarkPact"))||!Card(TEXT("CrescentField"))){Cleanup();return false;}
    auto Acquire=[&](FName Id){return U->AcquireUpgrade(Card(Id));};
    auto Offered=[&](FName Id)
    {
        // Reward presentation uses copied definitions; eligibility is identified
        // by stable upgrade ID, not the saved asset's UObject pointer.
        return U->GetCurrentUpgradeChoices().ContainsByPredicate(
            [Id](const UUpgradeDefinition* Choice){return Choice && Choice->UpgradeId==Id;});
    };
    FPlayerUpgradeRunState Empty;U->CaptureRunState(Empty);
    for(const auto& Row:Frequency)
    {
        U->RestoreRunState(Empty);Acquire(Row[0]);Acquire(Row[1]);
        TestEqual(TEXT("Saved frequency cap is three"),Card(Row[2])->MaxLevel,3);
        for(int32 Rank=0;Rank<3;++Rank)TestTrue(TEXT("Frequency rank is useful and obtainable"),Acquire(Row[2]));
        TestFalse(TEXT("Fourth frequency rank rejected"),Acquire(Row[2]));
        auto* Stale=DuplicateObject<UUpgradeDefinition>(Card(Row[2]),U);Stale->MaxLevel=4;
        TestFalse(TEXT("Stale four-rank asset cannot bypass cap"),U->AcquireUpgrade(Stale));
        TestFalse(TEXT("Max frequency no longer offered"),U->GetEligibleUpgradesForCategory(EUpgradeCategory::Samurai).Contains(Card(Row[2])));
        FPlayerUpgradeRunState Old;U->CaptureRunState(Old);Old.Levels[Row[2]]=4;Old.SamuraiMastery=20;
        U->RestoreRunState(Old);
        TestEqual(TEXT("Legacy fourth rank clamps to three"),U->GetUpgradeLevelById(Row[2]),3);
        TestEqual(TEXT("Clamping preserves mastery"),U->GetSamuraiMasteryPoints(),20);
        U->DebugForceAcquireUpgrade(Stale,4);
        TestEqual(TEXT("Debug grants also respect useful rank ceiling"),U->GetUpgradeLevelById(Row[2]),3);
    }
    for(const auto& Row:Tradeoffs)
    {
        U->RestoreRunState(Empty);Acquire(Row[0]);
        if(Row[0]==TEXT("BladeWave"))Acquire(TEXT("CrescentField"));
        Acquire(Row[1]);Acquire(Row[1]);Acquire(Row[3]);
        const int32 MasteryBefore=U->GetSamuraiMasteryPoints();
        TestTrue(TEXT("Shrine remains available after status investment"),U->BeginBloodShrineSelection(3));
        TestTrue(TEXT("Tradeoff remains a Shrine choice"),Offered(Row[2]));
        TestTrue(TEXT("Selecting tradeoff converts existing ranks"),U->SelectUpgrade(Card(Row[2])));
        TestEqual(TEXT("Converted status ranks are removed"),U->GetUpgradeLevelById(Row[1]),0);
        TestEqual(TEXT("Status ranks merge with damage ranks"),U->GetUpgradeLevelById(Row[3]),3);
        TestTrue(TEXT("Three damage ranks retain their full benefit"),FMath::IsNearlyEqual(U->GetAccumulatedUpgradeMagnitude(Row[3]),.6f,.0001f));
        TestEqual(TEXT("Only the pact purchase adds mastery"),U->GetSamuraiMasteryPoints(),MasteryBefore+1);
        TestFalse(TEXT("Disabled status upgrade is ineligible"),U->CanAcquireUpgrade(Card(Row[1])));
        TestFalse(TEXT("Disabled status cannot be acquired directly"),Acquire(Row[1]));
        TestFalse(TEXT("Debug grant cannot reintroduce dead status ranks"),U->DebugForceAcquireUpgrade(Card(Row[1]),5));
        auto* Stale=DuplicateObject<UUpgradeDefinition>(Card(Row[1]),U);Stale->PrerequisiteUpgradeIds.Empty();
        TestFalse(TEXT("Stale asset cannot bypass tradeoff exclusion"),U->AcquireUpgrade(Stale));
        TestFalse(TEXT("Normal offers exclude dead status card"),U->GetEligibleUpgradesForCategory(EUpgradeCategory::Samurai).Contains(Card(Row[1])));
        U->BeginDirectUpgradeSelection(1000);
        TestFalse(TEXT("Unrestricted offers exclude dead status card"),Offered(Row[1]));
        U->BeginDirectCategoryUpgradeSelection(EUpgradeCategory::SamuraiTrial,1000);
        TestFalse(TEXT("Repeat trials exclude dead status card"),Offered(Row[1]));
        TestTrue(TEXT("Converted damage can keep levelling"),Acquire(Row[3]));
        TestTrue(TEXT("Further purchases keep converted magnitude"),FMath::IsNearlyEqual(U->GetAccumulatedUpgradeMagnitude(Row[3]),.8f,.0001f));
        FPlayerUpgradeRunState Saved;U->CaptureRunState(Saved);U->RestoreRunState(Saved);
        TestTrue(TEXT("Converted damage survives restore"),FMath::IsNearlyEqual(U->GetAccumulatedUpgradeMagnitude(Row[3]),.8f,.0001f));

        // Old mixed saves can have capped, rarity-enhanced damage plus disabled
        // status ranks. Keep every spent rank's strength without reopening cap.
        auto Legacy=Saved;Legacy.Levels[Row[3]]=5;Legacy.AccumulatedMagnitudes[Row[3]]=1.3f;
        Legacy.Levels.Add(Row[1],2);Legacy.Definitions.Add(Row[1],Card(Row[1]));Legacy.SamuraiMastery=42;
        if(Row[0]==TEXT("Iaijutsu"))
        {Legacy.Levels.Add(TEXT("IaijutsuMarkPact"),1);Legacy.Definitions.Add(TEXT("IaijutsuMarkPact"),Card(TEXT("IaijutsuMarkPact")));}
        U->RestoreRunState(Legacy);
        const float Expected=Row[0]==TEXT("Iaijutsu")?1.9f:1.7f;
        TestEqual(TEXT("Legacy merge preserves damage rank cap"),U->GetUpgradeLevelById(Row[3]),5);
        TestFalse(TEXT("Legacy merge does not reopen capped damage offers"),U->CanAcquireUpgrade(Card(Row[3])));
        TestTrue(TEXT("Legacy merge preserves existing rarity strength and refunds dead ranks"),FMath::IsNearlyEqual(U->GetAccumulatedUpgradeMagnitude(Row[3]),Expected,.0001f));
        TestEqual(TEXT("Legacy merge preserves mastery"),U->GetSamuraiMasteryPoints(),42);
        TestFalse(TEXT("Legacy dead status ranks removed"),U->HasUpgradeId(Row[1]));
        TestFalse(TEXT("Legacy ineffective mark pact and width penalty removed"),U->HasUpgradeId(TEXT("IaijutsuMarkPact")));
        FPlayerUpgradeRunState Normalized;U->CaptureRunState(Normalized);
        U->RestoreRunState(Normalized);U->CaptureRunState(Normalized);U->RestoreRunState(Normalized);
        TestTrue(TEXT("Repeated restoration cannot duplicate conversion"),FMath::IsNearlyEqual(U->GetAccumulatedUpgradeMagnitude(Row[3]),Expected,.0001f));
    }
    // Both ordering directions enforce the two incompatible Iaijutsu pacts.
    for(bool bDashFirst:{false,true})
    {
        U->RestoreRunState(Empty);Acquire(TEXT("Iaijutsu"));
        const FName First=bDashFirst?TEXT("IaijutsuDashPact"):TEXT("IaijutsuMarkPact");
        const FName Second=bDashFirst?TEXT("IaijutsuMarkPact"):TEXT("IaijutsuDashPact");
        TestTrue(TEXT("First pact acquired"),Acquire(First));
        TestFalse(TEXT("Incompatible pact cannot be acquired"),Acquire(Second));
        TestFalse(TEXT("Debug cannot combine incompatible pacts"),U->DebugForceAcquireUpgrade(Card(Second)));
        U->BeginBloodShrineSelection(3);
        TestFalse(TEXT("Shrine excludes incompatible pact"),Offered(Second));
    }
    Cleanup();return true;
}
#endif
