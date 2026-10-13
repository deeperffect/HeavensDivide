#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "RunChallengeSubsystem.h"
#include "SynergyMetaProgressionSubsystem.h"
#include "BalanceLabDirector.h"
#include "EnemyBase.h"
#include "EnemySpawner.h"
#include "MeleeEnemyBase.h"
#include "RangedEnemyBase.h"
#include "TankMeleeEnemyBase.h"
#include "CinderOracleBoss.h"
#include "TacticalEnemy.h"
#include "EncounterWall.h"
#include "HealingUrn.h"
#include "HealthComponent.h"
#include "SurvivorPlayerController.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunChallengeTest,"HeavensDivide.Progression.RunChallenges",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRunChallengeTest::RunTest(const FString&)
{
    const FString Slot = TEXT("Automation_RunChallenges_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString PreviousCommandLine = FCommandLine::Get();
    FCommandLine::Set(*(TEXT("-MetaSaveSlot=") + Slot + TEXT(" ") + PreviousCommandLine));
    auto* GI = NewObject<UGameInstance>(GEngine);
    GI->InitializeStandalone();
    FCommandLine::Set(*PreviousCommandLine);
    auto* World = GI->GetWorld();
    World->InitializeActorsForPlay(FURL());
    auto* Challenges = GI->GetSubsystem<URunChallengeSubsystem>();
    auto* Meta = GI->GetSubsystem<USynergyMetaProgressionSubsystem>();
    TestEqual(TEXT("Five optional challenge tracks"),RunChallenges::Modifiers().Num(),5);
    TestFalse(TEXT("Unknown challenge rejected"),Challenges->SetModifierRank(TEXT("Missing"),2));
    TestFalse(TEXT("Ranks above five rejected"),Challenges->SetModifierRank(TEXT("Health"),6));
    TestFalse(TEXT("Negative ranks rejected"),Challenges->SetModifierRank(TEXT("Health"),-1));
    TMap<FName,int32> Completed;
    for(const auto& M:RunChallenges::Modifiers())Completed.Add(M.Id,0);
    TestFalse(TEXT("A new profile cannot skip locked difficulty"),Challenges->SetModifierRank(TEXT("Health"),1));
    Meta->bSimulateSaveFailure=true;Meta->RecordDifficultyVictory(TEXT("FallenGrove"),Completed);
    TestEqual(TEXT("Failed save leaves rank one locked"),Challenges->GetUnlockedRank(TEXT("Health")),0);
    TestTrue(TEXT("Failed difficulty unlock remains pending for retry"),Meta->HasPendingSkillReward());
    Meta->bSimulateSaveFailure=false;Meta->RetrySkillReward();
    TestEqual(TEXT("Retried victory opens rank one"),Challenges->GetUnlockedRank(TEXT("Health")),1);
    Meta->LoadMetaProgression();TestEqual(TEXT("Difficulty unlock survives profile reload"),Challenges->GetUnlockedRank(TEXT("Health")),1);
    Meta->RecordDifficultyVictory(TEXT("FallenGrove"),Completed);
    TestEqual(TEXT("Repeating rank zero cannot open rank two"),Challenges->GetUnlockedRank(TEXT("Health")),1);
    Completed[TEXT("Health")]=1;Meta->RecordDifficultyVictory(TEXT("FallenGrove"),Completed);
    TestEqual(TEXT("Health progresses independently"),Challenges->GetUnlockedRank(TEXT("Health")),2);
    TestEqual(TEXT("Unused modifier stays at one"),Challenges->GetUnlockedRank(TEXT("Damage")),1);
    TestTrue(TEXT("Another map may be inspected"),Challenges->SetMap(TEXT("CinderCitadel")));
    TestEqual(TEXT("Other map starts at zero"),Challenges->GetUnlockedRank(TEXT("Health")),0);
    TestTrue(TEXT("Return to the played map"),Challenges->SetMap(TEXT("FallenGrove")));
    for(int32 Rank=1;Rank<5;++Rank)
    {for(const auto& M:RunChallenges::Modifiers())Completed[M.Id]=Rank;Meta->RecordDifficultyVictory(TEXT("FallenGrove"),Completed);}
    for (const auto& M : RunChallenges::Modifiers())
    {
        TestEqual(TEXT("Default challenge rank is zero"),Challenges->GetModifierRank(M.Id),0);
        TestTrue(TEXT("Menu accepts maximum rank"),Challenges->SetModifierRank(M.Id,5));
    }
    TestTrue(TEXT("Maximum reward bonus is +200 percent"),FMath::IsNearlyEqual(Challenges->GetRewardMultiplier(),3.f));
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto ReadFloat = [](UObject* Object,const TCHAR* Name)
    { return FindFProperty<FFloatProperty>(Object->GetClass(),Name)->GetPropertyValue_InContainer(Object); };
    for (UClass* Type : {AMeleeEnemyBase::StaticClass(),ARangedEnemyBase::StaticClass(),ATankMeleeEnemyBase::StaticClass(),ATacticalEnemy::StaticClass()})
    {
        auto* Enemy = World->SpawnActor<AEnemyBase>(Type,FVector(0,0,100),FRotator::ZeroRotator,Params);
        const float Health = Enemy->GetHealthComponent()->GetMaxHealth(), Speed = Enemy->MoveSpeed;
        const TCHAR* DamageProperty = Enemy->IsA<ATacticalEnemy>() ? TEXT("AbilityDamage") : TEXT("AttackDamage");
        const float Damage = ReadFloat(Enemy,DamageProperty);
        const float ContactDamage = Enemy->IsA<ATankMeleeEnemyBase>() ? ReadFloat(Enemy,TEXT("ContactDamage")) : 0.f;
        TestTrue(TEXT("Fresh enemy accepts challenge exactly once"),Challenges->ApplyEnemy(Enemy));
        TestTrue(TEXT("Enemy HP +75 percent"),FMath::IsNearlyEqual(Enemy->GetHealthComponent()->GetMaxHealth(),Health*1.75f));
        TestTrue(TEXT("Enemy movement +20 percent"),FMath::IsNearlyEqual(Enemy->MoveSpeed,Speed*1.2f));
        TestTrue(TEXT("Each enemy attack implementation scales damage"),FMath::IsNearlyEqual(ReadFloat(Enemy,DamageProperty),Damage*1.5f));
        if (Enemy->IsA<ATankMeleeEnemyBase>())
            TestTrue(TEXT("Contact damage scales as well as slams"),FMath::IsNearlyEqual(ReadFloat(Enemy,TEXT("ContactDamage")),ContactDamage*1.5f));
        if (auto* Tactical = Cast<ATacticalEnemy>(Enemy))
        {
            Tactical->InitializeMarch(FVector::ForwardVector,500.f,3000.f,0.f);
            TestTrue(TEXT("Grunt formations use selected movement challenge"),FMath::IsNearlyEqual(Tactical->GetVelocity().Size(),600.f));
        }
        TestFalse(TEXT("Repeated setup cannot stack challenge multipliers"),Challenges->ApplyEnemy(Enemy));
        TestTrue(TEXT("Repeat leaves HP intact"),FMath::IsNearlyEqual(Enemy->GetHealthComponent()->GetMaxHealth(),Health*1.75f));
        Enemy->Destroy();
    }
    auto* Boss = World->SpawnActor<ACinderOracleBoss>(FVector(3000,0,100),FRotator::ZeroRotator,Params);
    const float BossHealth = ReadFloat(Boss,TEXT("BossMaxHealth")), SpellDamage = Boss->SpellDamage;
    Boss->DispatchBeginPlay();
    TestTrue(TEXT("Boss challenge is applied after authored health initialization"),FMath::IsNearlyEqual(Boss->GetHealthComponent()->GetMaxHealth(),BossHealth*1.75f));
    TestTrue(TEXT("Oracle spells use the damage challenge"),FMath::IsNearlyEqual(Boss->SpellDamage,SpellDamage*1.5f));
    Boss->Destroy();

    auto* Spawner = World->SpawnActor<AEnemySpawner>();
    Spawner->AbsoluteHardAliveCap = 190;
    FEnemyPressurePhase Phase; Phase.GlobalMaxAlive = 180; Phase.NormalSpawnInterval = 1.f;
    FEnemyPopulationPhaseEntry Entry; Entry.DesiredPopulation = 100; Entry.MaxPopulation = 120;
    Phase.EnemyPopulationEntries.Add(Entry); Spawner->PressurePhases = {Phase};
    TestTrue(TEXT("Spawner accepts challenge"),Challenges->ApplySpawner(Spawner));
    TestEqual(TEXT("Population cannot exceed CPU safety ceiling"),Spawner->AbsoluteHardAliveCap,200);
    TestEqual(TEXT("Per-phase population ceiling follows safety limit"),Spawner->PressurePhases[0].GlobalMaxAlive,200);
    TestEqual(TEXT("Desired population scales once"),Spawner->PressurePhases[0].EnemyPopulationEntries[0].DesiredPopulation,130);
    TestTrue(TEXT("Spawn frequency +40 percent divides interval"),FMath::IsNearlyEqual(Spawner->PressurePhases[0].NormalSpawnInterval,1.f/1.4f));
    TestFalse(TEXT("Repeated spawner setup cannot stack"),Challenges->ApplySpawner(Spawner));
    Spawner->Destroy();
    auto* Wall = World->SpawnActor<AEncounterWall>(FVector(2000,0,100),FRotator::ZeroRotator,Params);
    auto* Urn = World->SpawnActor<AHealingUrn>(FVector(2200,0,100),FRotator::ZeroRotator,Params);
    TestFalse(TEXT("Breakable walls do not scale"),Challenges->ApplyEnemy(Wall));
    TestFalse(TEXT("Reward urns do not scale"),Challenges->ApplyEnemy(Urn));
    Wall->Destroy(); Urn->Destroy();

    for (const auto& M : RunChallenges::Modifiers()) Challenges->SetModifierRank(M.Id,0);
    Challenges->SetModifierRank(TEXT("Health"),1);
    Meta->BeginCurrencyRun();
    const int32 WalletEmbers = Meta->GetSoulEmbers(), WalletSigils = Meta->GetSoulSigils();
    for (int32 I=0;I<25;++I)
    {
        const auto Preview=Meta->PreviewCurrencyReward(1,1);
        const auto Again=Meta->PreviewCurrencyReward(1,1);
        TestEqual(TEXT("Preview does not consume fractions"),Again,Preview);
        const int32 BeforeEmbers=Meta->GetRunEmbers(),BeforeSigils=Meta->GetRunSigils();
        Meta->AwardCurrencies(1,1);
        TestEqual(TEXT("Preview matches actual Ember payout"),Meta->GetRunEmbers()-BeforeEmbers,Preview.X);
        TestEqual(TEXT("Preview matches actual Sigil payout"),Meta->GetRunSigils()-BeforeSigils,Preview.Y);
        Meta->RetrySkillReward();
        Meta->LoadMetaProgression();
        // Emulate a new run for each tiny reward; fractional bonus must survive both.
        if (I<24) Meta->BeginCurrencyRun();
    }
    TestEqual(TEXT("Small ember bonuses survive short runs and reload"),Meta->GetSoulEmbers(),WalletEmbers+27);
    TestEqual(TEXT("Small sigil bonuses survive short runs and reload"),Meta->GetSoulSigils(),WalletSigils+27);
    Meta->RetrySkillReward();
    TestFalse(TEXT("Both currency queues save together"),Meta->HasPendingSkillReward());
    Meta->LoadMetaProgression();
    TestEqual(TEXT("Embers persist after reload"),Meta->GetSoulEmbers(),WalletEmbers+27);
    TestEqual(TEXT("Sigils persist after reload"),Meta->GetSoulSigils(),WalletSigils+27);
    Challenges->SetModifierRank(TEXT("Health"),0);
    auto* PC = World->SpawnActor<ASurvivorPlayerController>();
    TestFalse(TEXT("Difficulty is locked in gameplay"),Challenges->SetModifierRank(TEXT("Health"),5));
    TestEqual(TEXT("Rejected edit leaves run difficulty unchanged"),Challenges->GetModifierRank(TEXT("Health")),0);
    Challenges->BeginRun(); Meta->BeginCurrencyRun();
    auto Kill = [&](UClass* Type,EEnemyDropCategory Category = EEnemyDropCategory::Normal,bool Suppress = false)
    {
        auto* Enemy = World->SpawnActor<AEnemyBase>(Type,FVector(0,0,100),FRotator::ZeroRotator,Params);
        Enemy->XPReward = 0; Enemy->DropCategory = Category;
        if (Suppress) Enemy->DisablePermanentCurrencyReward();
        Enemy->DispatchBeginPlay(); Enemy->GetHealthComponent()->ApplyDamage(1000000);
        return Enemy;
    };
    for (int32 I=0;I<4;++I) Kill(AEnemyBase::StaticClass());
    TestEqual(TEXT("Four normal kills do not round up to an ember"),Meta->GetRunEmbers(),0);
    auto* Fifth = Kill(AEnemyBase::StaticClass());
    TestEqual(TEXT("Fifth eligible kill credits one ember"),Meta->GetRunEmbers(),1);
    Challenges->NotifyEnemyKilled(Fifth);
    TestEqual(TEXT("Repeated death notification cannot pay twice"),Meta->GetRunEmbers(),1);
    Kill(ATacticalEnemy::StaticClass());
    TestEqual(TEXT("Tactical enemies grant two embers"),Meta->GetRunEmbers(),3);
    Kill(AEnemyBase::StaticClass(),EEnemyDropCategory::Elite);
    TestEqual(TEXT("Elite gives three embers"),Meta->GetRunEmbers(),6);
    TestEqual(TEXT("Elite gives one sigil"),Meta->GetRunSigils(),1);
    Kill(ATacticalEnemy::StaticClass(),EEnemyDropCategory::Normal,true);
    TestEqual(TEXT("Repeatable boss guards cannot farm currency"),Meta->GetRunEmbers(),6);
    auto* Despawned = World->SpawnActor<AEnemyBase>(FVector(1000,0,100),FRotator::ZeroRotator,Params);
    Despawned->DispatchBeginPlay(); Despawned->Destroy();
    TestEqual(TEXT("Despawn cleanup gives no currency"),Meta->GetRunEmbers(),6);
    auto* Lab = World->SpawnActor<ABalanceLabDirector>();
    auto* TrainingEnemy = World->SpawnActor<AEnemyBase>(FVector(1000,0,100),FRotator::ZeroRotator,Params);
    TestFalse(TEXT("Balance Lab ignores selected challenge"),Challenges->ApplyEnemy(TrainingEnemy));
    Kill(AEnemyBase::StaticClass(),EEnemyDropCategory::Elite);
    RunChallenges::AwardCurrencies(Lab,20,2);
    TestEqual(TEXT("Balance Lab grants no permanent rewards"),Meta->GetRunEmbers(),6);
    GI->Shutdown();
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    UGameplayStatics::DeleteGameInSlot(Slot,0);
    return true;
}
#endif
