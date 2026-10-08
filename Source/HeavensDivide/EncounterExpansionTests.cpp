#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "TacticalEnemy.h"
#include "CinderOracleBoss.h"
#include "EncounterHazard.h"
#include "HealingUrn.h"
#include "HealingPickup.h"
#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "HealthComponent.h"
#include "SurvivorPlayerController.h"
#include "EnemyDeathComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEncounterExpansionTest,"HeavensDivide.Encounters.Mechanics",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEncounterExpansionTest::RunTest(const FString&)
{
    auto* W=UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);W->InitializeActorsForPlay(FURL());
    auto* PC=W->SpawnActor<ASurvivorPlayerController>();auto* Player=W->SpawnActor<ACharacterBase>();
    PC->Possess(Player);Player->SetOwner(PC);
    FindFProperty<FObjectProperty>(UCharacterManagerComponent::StaticClass(),TEXT("ActiveCharacter"))->SetObjectPropertyValue_InContainer(PC->GetCharacterManager(),Player);
    auto* HP=PC->GetPlayerHealthComponent();HP->RestoreCurrentHealth(100);
    FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto Setup=[&](AEnemyBase* E)
    {
        E->SetTarget(Player);E->CachedSurvivorController=PC;E->ObservedCharacterManager=PC->GetCharacterManager();
        E->bDropsXP=false;E->EnemyDeathComponent->DeathNiagaraSystem=nullptr;E->EnemyDeathComponent->DeathSound=nullptr;
        E->GetHealthComponent()->OnDeath.AddUniqueDynamic(E,&AEnemyBase::HandleDeath);
    };
    auto CountHazards=[&]() { int32 N=0;for(TActorIterator<AEncounterHazard> It(W);It;++It) if(!It->IsActorBeingDestroyed()) ++N;return N; };
    auto ClearHazards=[&]() { for(TActorIterator<AEncounterHazard> It(W);It;++It) It->Destroy(); };
    auto* Caster=W->SpawnActor<ATacticalEnemy>(FVector::ZeroVector,FRotator::ZeroRotator,Params);Setup(Caster);
    Player->SetActorLocation(FVector(500,0,0));Caster->BeginAbility();
    TestEqual(TEXT("Caster creates one locked warning"),CountHazards(),1);
    AEncounterHazard* H=*TActorIterator<AEncounterHazard>(W);
    H->Tick(.5f);TestEqual(TEXT("Warning does not damage early"),HP->GetCurrentHealth(),100.f);
    Player->SetActorLocation(FVector(900,0,0));H->Tick(.6f);
    TestEqual(TEXT("Moving out during warning avoids impact"),HP->GetCurrentHealth(),100.f);
    TestTrue(TEXT("Warning stays at its original position"),H->GetActorLocation().Equals(FVector(500,0,0)));
    H->Destroy();Caster->StopEnemyBehavior();
    Player->SetActorLocation(FVector(500,0,0));Caster->BeginAbility();H=*TActorIterator<AEncounterHazard>(W);
    H->Tick(1.1f);TestEqual(TEXT("Circle damages on impact"),HP->GetCurrentHealth(),86.f);
    H->Tick(.03f);TestEqual(TEXT("Burst does not hit again next frame"),HP->GetCurrentHealth(),86.f);
    Caster->GetHealthComponent()->ApplyDamage(10000);
    TestEqual(TEXT("Killing caster removes pending and active hazards"),CountHazards(),0);

    auto* Owner=W->SpawnActor<AActor>();H=AEncounterHazard::Spawn(Owner,FVector::ZeroVector,EEncounterShape::Ring,400,180,1,10);
    TestFalse(TEXT("Ring center is safe"),H->ContainsPoint(FVector(100,0,0)));
    TestTrue(TEXT("Ring band is dangerous"),H->ContainsPoint(FVector(300,0,0)));
    TestFalse(TEXT("Ring exterior is safe"),H->ContainsPoint(FVector(450,0,0)));H->Destroy();
    H=AEncounterHazard::Spawn(Owner,FVector::ZeroVector,EEncounterShape::Lane,1000,100,1,10,.2f,90);
    TestTrue(TEXT("Rotated lane uses the visible axis"),H->ContainsPoint(FVector(0,450,0)));
    TestFalse(TEXT("Standing beside lane is safe"),H->ContainsPoint(FVector(100,0,0)));H->Destroy();

    auto* Charger=W->SpawnActor<ATacticalEnemy>(FVector::ZeroVector,FRotator::ZeroRotator,Params);Setup(Charger);
    Charger->TacticalRole=ETacticalEnemyRole::FangStalker;Player->SetActorLocation(FVector(600,0,0));Charger->BeginAbility();
    Player->SetActorLocation(FVector(600,600,0));Charger->FinishAbility();
    TestTrue(TEXT("Charge direction is committed, not homing"),Charger->LockedDirection.Equals(FVector::ForwardVector));
    TestEqual(TEXT("Double charger schedules one follow-up"),Charger->FollowupCharges,1);
    Charger->FinishAbility();TestFalse(TEXT("Second dash does not schedule an infinite combo"),Charger->bSecondDash);
    Charger->SetGameplaySuspended(true);TestEqual(TEXT("Trial interruption cancels charge"),Charger->ChargeRemaining,0.f);
    TestEqual(TEXT("Trial interruption removes charge warning"),CountHazards(),0);

    auto* Marcher=W->SpawnActor<ATacticalEnemy>(FVector::ZeroVector,FRotator::ZeroRotator,Params);Setup(Marcher);
    Marcher->InitializeMarch(FVector::ForwardVector,300,1200,0);Player->SetActorLocation(FVector(500,0,0));
    HP->RestoreCurrentHealth(100);Marcher->HitAlongMovement(FVector(300,0,0),FVector(700,0,0));
    TestEqual(TEXT("March swept contact cannot tunnel through player"),HP->GetCurrentHealth(),86.f);
    Marcher->HitAlongMovement(FVector(300,0,0),FVector(700,0,0));TestEqual(TEXT("Each marcher only hits once"),HP->GetCurrentHealth(),86.f);
    auto* Survivor=W->SpawnActor<ATacticalEnemy>(FVector(0,115,0),FRotator::ZeroRotator,Params);Setup(Survivor);
    Survivor->InitializeMarch(FVector::ForwardVector,300,1200,0);
    Marcher->GetHealthComponent()->ApplyDamage(10000);
    TestTrue(TEXT("Marcher is killable"),Marcher->IsDead());TestFalse(TEXT("Killing one leaves its neighbor alive"),Survivor->IsDead());
    TestEqual(TEXT("Neighbor stays in its lane; gap is not refilled"),Survivor->GetActorLocation().Y,115.);

    const FString Root=TEXT("/Game/HeavensDivide/Blueprints/EnemyCharacters/Tactical/BP_");
    for(const TCHAR* Name:{TEXT("AshSeer"),TEXT("HexSniper"),TEXT("MireWeaver"),TEXT("HornLancer"),TEXT("FangStalker"),TEXT("GraveCantor"),TEXT("WarDrummer"),TEXT("OgreWarden"),TEXT("StormGorilla"),TEXT("FrostOracle")})
    {
        auto* Class=LoadClass<ATacticalEnemy>(nullptr,*(Root+Name+TEXT(".BP_")+Name+TEXT("_C")));
        if(!TestNotNull(FString(Name)+TEXT(" Blueprint loads"),Class))continue;
        auto* E=W->SpawnActor<ATacticalEnemy>(Class,FVector::ZeroVector,FRotator::ZeroRotator,Params);Setup(E);
        TestNotNull(FString(Name)+TEXT(" visible mesh"),E->GetMesh()->GetSkeletalMeshAsset());
        TestNotNull(FString(Name)+TEXT(" animation instance"),E->GetMesh()->GetAnimInstance());
        E->BeginAbility();E->FinishAbility();E->StopEnemyBehavior();E->Destroy();ClearHazards();
    }
    auto* BossClass=LoadClass<ACinderOracleBoss>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/EnemyCharacters/Bosses/CinderOracle/BP_CinderOracle.BP_CinderOracle_C"));
    auto* Boss=W->SpawnActor<ACinderOracleBoss>(BossClass,FVector::ZeroVector,FRotator::ZeroRotator,Params);Setup(Boss);Boss->bOracleActive=true;
    for(int32 Spell=0;Spell<7;++Spell)
    {
        Boss->CastAbility(Spell);TestTrue(FString::Printf(TEXT("Boss spell %d creates telegraphs"),Spell+1),CountHazards()>0);ClearHazards();
    }
    Boss->CastAbility(4);Boss->CastAbility(4);TestTrue(TEXT("Summon limit is four"),Boss->Guardians.Num()<=4);
    Boss->StopBossCombat();TestEqual(TEXT("Boss cancellation clears hazards"),CountHazards(),0);
    TestEqual(TEXT("Boss cancellation clears guardians"),Boss->Guardians.Num(),0);

    auto* UrnClass=LoadClass<AHealingUrn>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/Encounters/BP_HealingUrn.BP_HealingUrn_C"));
    auto* Urn=W->SpawnActor<AHealingUrn>(UrnClass,FVector::ZeroVector,FRotator::ZeroRotator,Params);Setup(Urn);
    Urn->ApplyPlayerDamage(1000,EPlayerAttackSource::Ninja);
    int32 Heals=0;for(TActorIterator<AHealingPickup> It(W);It;++It)++Heals;
    TestTrue(TEXT("Weapons can break urn"),Urn->IsDead());TestEqual(TEXT("Breaking urn creates one heal"),Heals,1);
    Urn->ApplyPlayerDamage(1000,EPlayerAttackSource::Samurai);
    Heals=0;for(TActorIterator<AHealingPickup> It(W);It;++It)++Heals;
    TestEqual(TEXT("Repeated damage cannot duplicate healing drop"),Heals,1);
    // This fixture has no local viewport. Exercise victory state without creating its screen widget.
    FindFProperty<FObjectPropertyBase>(ASurvivorPlayerController::StaticClass(),TEXT("VictoryWidgetClass"))->SetObjectPropertyValue_InContainer(PC,nullptr);
    Boss->bOracleActive=true;Boss->CastAbility(0);Boss->GetHealthComponent()->ApplyDamage(100000);
    TestTrue(TEXT("Caster boss death reaches final boss death state"),Boss->GetBossState()==EFinalBossState::Dead);
    TestFalse(TEXT("Caster boss death completes the run"),PC->IsRunInProgress());
    TestEqual(TEXT("Victory leaves no boss hazards"),CountHazards(),0);
    W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
}
#endif
