#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "SurvivorPlayerController.h"
#include "PlayerUpgradeComponent.h"
#include "EnemyBase.h"
#include "EnemyStatusEffectComponent.h"
#include "HealthComponent.h"
#include "BloodshiftBalanceData.h"
#include "HeavensDivideMetaSaveGame.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBalanceRuntimeTest,"HeavensDivide.Balance.Runtime",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBalanceRuntimeTest::RunTest(const FString&)
{
    auto* World=UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);World->InitializeActorsForPlay(FURL());
    auto* PCClass=LoadClass<ASurvivorPlayerController>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController.BP_SurvivorPlayerController_C"));
    auto* PC=World->SpawnActor<ASurvivorPlayerController>(PCClass);
    auto* U=PC?PC->GetPlayerUpgrades():nullptr;
    if(!TestNotNull(TEXT("Saved upgrade pool"),U)){World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return false;}
    auto* Blood=U->FindUpgradeDefinition(TEXT("BattleStance"));auto* Barrage=U->FindUpgradeDefinition(TEXT("BarrageStance"));
    if(!Blood||!Barrage){AddError(TEXT("Stance definitions missing"));World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return false;}
    const auto BloodBefore=Blood->BalanceParameters,BarrageBefore=Barrage->BalanceParameters;
    Blood->BalanceParameters.Add(TEXT("BleedHitFraction"),.24f);
    Blood->BalanceParameters.Add(TEXT("BleedDuration"),4.f);
    Blood->BalanceParameters.Add(TEXT("BleedTickInterval"),.5f);
    Blood->BalanceParameters.Add(TEXT("BleedStackCap"),2.f);
    U->AcquireUpgrade(Blood);U->AcquireUpgrade(Barrage);
    auto* Enemy=World->SpawnActor<AEnemyBase>();
    auto* Status=Enemy->GetStatusEffectComponent();
    Status->ApplyStatus(EEnemyStatusEffect::Bleed,U,EPlayerAttackSource::Samurai,false,100.f);
    TestTrue(TEXT("Edited bleed fraction and tick budget reach actual status damage"),FMath::IsNearlyEqual(Status->CalculateRemainingStatusDamage(EEnemyStatusEffect::Bleed),24.f));
    Status->ApplyStatus(EEnemyStatusEffect::Bleed,U,EPlayerAttackSource::Samurai,false,100.f);
    Status->ApplyStatus(EEnemyStatusEffect::Bleed,U,EPlayerAttackSource::Samurai,false,100.f);
    TestEqual(TEXT("Edited bleed cap limits real stacks"),Status->GetStatusStacks(EEnemyStatusEffect::Bleed),2);
    TestTrue(TEXT("Capped bleed damage uses only accepted stacks"),FMath::IsNearlyEqual(Status->CalculateRemainingStatusDamage(EEnemyStatusEffect::Bleed),48.f));
    Barrage->BalanceParameters.Add(TEXT("PoisonHitFraction"),.16f);
    Barrage->BalanceParameters.Add(TEXT("PoisonDuration"),4.f);
    Barrage->BalanceParameters.Add(TEXT("PoisonTickInterval"),1.f);
    Status->ApplyBarragePoison(U,100.f,false);
    TestTrue(TEXT("Edited poison fraction and cadence reach real poison damage"),FMath::IsNearlyEqual(Status->CalculateRemainingStatusDamage(EEnemyStatusEffect::Poison),16.f));
    auto* Rules=const_cast<UBloodshiftBalanceData*>(UBloodshiftBalanceData::Get());
    const float Before=Rules->Skills[0].PerRank;Rules->Skills[0].PerRank=.123f;
    auto* Save=NewObject<UHeavensDivideMetaSaveGame>();Save->SkillRanks.Add(Rules->Skills[0].Id,1);
    TestTrue(TEXT("Permanent bonus reads saved balance asset"),FMath::IsNearlyEqual(MetaSkillTree::Bonus(*Save,Rules->Skills[0].Effect),.123f));
    TestTrue(TEXT("Permanent skill copy follows tuned value"),MetaSkillTree::Description(Rules->Skills[0]).Contains(TEXT("12.3%")));
    Rules->Skills[0].PerRank=Before;Blood->BalanceParameters=BloodBefore;Barrage->BalanceParameters=BarrageBefore;
    World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return true;
}
#endif
