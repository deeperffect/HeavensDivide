#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "TacticalEnemy.h"
#include "CinderOracleBoss.h"
#include "EncounterHazard.h"
#include "IndicatorAttackVFX.h"
#include "HealingUrn.h"
#include "HealingPickup.h"
#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "HealthComponent.h"
#include "SurvivorPlayerController.h"
#include "EnemyDeathComponent.h"
#include "EnemyLightweightMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "UObject/UnrealType.h"
#include "CombatGround.h"
#include "ExperiencePickup.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"

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
    // Isolate enemy hit geometry while manually ticking actors; hit grace has its own test.
    HP->DamageGracePeriod=0.f;
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
    TestEqual(TEXT("Circle shares the elite material"),H->Material->Parent->GetName(),FString(TEXT("M_AttackIndicatorCircle")));
    TestTrue(TEXT("Elite fill progresses with the damage warning"),H->Material->K2_GetScalarParameterValue(TEXT("FillAmount"))>0.f);
    TestTrue(TEXT("Enemy role color does not override elite warning colors"),H->Material->K2_GetVectorParameterValue(TEXT("FillColor")).Equals(UMaterialInstanceDynamic::Create(H->Material->Parent,W)->K2_GetVectorParameterValue(TEXT("FillColor"))));
    Player->SetActorLocation(FVector(900,0,0));H->Tick(.6f);
    TestEqual(TEXT("Moving out during warning avoids impact"),HP->GetCurrentHealth(),100.f);
    TestTrue(TEXT("Warning stays at its original position"),H->GetActorLocation().Equals(FVector(500,0,0)));
    H->Destroy();Caster->StopEnemyBehavior();
    Player->SetActorLocation(FVector(500,0,0));Caster->BeginAbility();H=*TActorIterator<AEncounterHazard>(W);
    H->Tick(1.1f);TestEqual(TEXT("Circle damages on impact"),HP->GetCurrentHealth(),86.f);
    TestTrue(TEXT("Warning uses a visible surface independent of decals"),H->HasVisibleWarning() && H->GroundSurface->IsVisible());
    bool bHasExecutionVFX = false;
    for (TActorIterator<AIndicatorAttackVFX> It(W); It; ++It)
        if (It->GetOwner() == Caster && !It->IsActorBeingDestroyed()) bHasExecutionVFX = true;
    TestTrue(TEXT("Damage frame has execution VFX or its fallback volume"),bHasExecutionVFX || H->ImpactColumn->IsVisible());
    H->Tick(.03f);TestEqual(TEXT("Burst does not hit again next frame"),HP->GetCurrentHealth(),86.f);
    Caster->GetHealthComponent()->ApplyDamage(10000);
    TestEqual(TEXT("Killing caster removes pending and active hazards"),CountHazards(),0);

    auto* Owner=W->SpawnActor<AActor>();H=AEncounterHazard::Spawn(Owner,FVector::ZeroVector,EEncounterShape::Ring,400,180,1,10);
    TestFalse(TEXT("Ring center is safe"),H->ContainsPoint(FVector(100,0,0)));
    TestEqual(TEXT("Visible ring hole matches its safe radius"),H->Material->K2_GetScalarParameterValue(TEXT("InnerRadius")),.45f);
    H->Tick(1.01f);
    TestFalse(TEXT("Ring impact does not cover its safe center with a solid cylinder"),H->ImpactColumn->IsVisible());
    TestTrue(TEXT("Ring band is dangerous"),H->ContainsPoint(FVector(300,0,0)));
    TestFalse(TEXT("Ring exterior is safe"),H->ContainsPoint(FVector(450,0,0)));H->Destroy();
    H=AEncounterHazard::Spawn(Owner,FVector::ZeroVector,EEncounterShape::Lane,1000,100,1,10,.2f,90);
    TestEqual(TEXT("Lane shares the Ogre material"),H->Material->Parent->GetName(),FString(TEXT("M_AttackIndicatorRectangle")));
    TestEqual(TEXT("Lane outline respects its aspect ratio"),H->Material->K2_GetScalarParameterValue(TEXT("LaneAspect")),.1f);
    TestTrue(TEXT("Rotated lane uses the visible axis"),H->ContainsPoint(FVector(0,450,0)));
    TestFalse(TEXT("Standing beside lane is safe"),H->ContainsPoint(FVector(100,0,0)));H->Destroy();
    HP->RestoreCurrentHealth(100);Player->SetActorLocation(FVector::ZeroVector);
    H=AEncounterHazard::Spawn(Owner,FVector::ZeroVector,EEncounterShape::Circle,400,0,1,10,.2f);
    H->Tick(3.f);TestEqual(TEXT("Expired attack cannot damage after a long hitch"),HP->GetCurrentHealth(),100.f);

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
        E->bUseEnemySeparation=false;E->bUseCrowdSpread=false;E->Cooldown=2;E->Recovery=0;
        Player->SetActorLocation(E->GetActorLocation()+FVector(E->CastRange+500,0,0));E->UpdateEnemyBehavior(.1f);
        TestTrue(FString(Name)+TEXT(" chases toward a player outside attack range"),E->bHasDesiredMovementDirection && E->DesiredMovementDirection.X>0);
        E->LightweightMovementComponent->SetMovementEnabled(true);E->LightweightMovementComponent->RefreshSpawnZ();
        const FVector Before=E->GetActorLocation();
        for(int32 Frame=0;Frame<12;++Frame)
        {
            E->ApplyDesiredMovementInput(1.f/60);
            E->LightweightMovementComponent->TickComponent(1.f/60,LEVELTICK_All,&E->LightweightMovementComponent->PrimaryComponentTick);
        }
        TestTrue(FString(Name)+TEXT(" actually approaches the player"),E->GetActorLocation().X>Before.X+10);
        TestTrue(FString(Name)+TEXT(" reports locomotion velocity to its Animation Blueprint"),E->GetVelocity().Size2D()>10);
        for(float Distance:{E->GetEngagementRange(),100.f})
        {
            Player->SetActorLocation(E->GetActorLocation()+FVector(Distance,0,0));E->UpdateEnemyBehavior(.1f);
            TestFalse(FString(Name)+TEXT(" holds position at attack range and when approached"),E->bHasDesiredMovementDirection);
            TestTrue(FString(Name)+TEXT(" stops locomotion rather than retreating"),E->GetVelocity().IsNearlyZero());
        }
        Player->SetActorLocation(E->GetActorLocation()+FVector(E->GetEngagementRange(),0,0));
        E->Cooldown=0;E->UpdateEnemyBehavior(.1f);
        TestTrue(FString(Name)+TEXT(" attacks when ready inside range"),E->CastRemaining>0);
        if(E->TacticalRole==ETacticalEnemyRole::OgreWarden || E->TacticalRole==ETacticalEnemyRole::StormGorilla)
        {
            bool bCovered=false;
            for(TActorIterator<AEncounterHazard> It(W);It;++It)
                if(It->GetOwner()==E && !It->IsActorBeingDestroyed()) bCovered|=It->ContainsPoint(Player->GetActorLocation());
            TestTrue(FString(Name)+TEXT(" self-centered attack reaches its stopping distance"),bCovered);
        }
        E->SetGameplaySuspended(true);E->SetGameplaySuspended(false);
        Player->SetActorLocation(E->GetActorLocation()+FVector(E->CastRange+500,0,0));E->UpdateEnemyBehavior(.1f);
        TestTrue(FString(Name)+TEXT(" resumes chasing when player leaves range"),E->bHasDesiredMovementDirection);
        E->BeginAbility();E->FinishAbility();E->StopEnemyBehavior();E->Destroy();ClearHazards();
    }
    // Reproduce a raised floor and characters blocking the ground channel.
    auto* Floor=W->SpawnActor<AActor>();auto* FloorBox=NewObject<UBoxComponent>(Floor);
    Floor->SetRootComponent(FloorBox);FloorBox->SetBoxExtent(FVector(10000,10000,10));
    FloorBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);FloorBox->SetCollisionResponseToAllChannels(ECR_Block);
    FloorBox->RegisterComponent();Floor->SetActorLocation(FVector(0,0,27)); // Surface Z=37.
    Player->SetActorLocation(FVector(500,0,400));
    Player->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Player->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel2,ECR_Block);
    FVector Surface;
    TestTrue(TEXT("Ground projection passes through the player"),CombatGround::FindSurface(W,Player->GetActorLocation(),nullptr,Surface));
    TestTrue(TEXT("Character top never becomes the ground"),FMath::IsNearlyEqual(Surface.Z,37.,.1));
    H=AEncounterHazard::Spawn(Owner,Player->GetActorLocation(),EEncounterShape::Circle,200,0,1,22);
    TestTrue(TEXT("Boss warning lies on the actual floor"),FMath::IsNearlyEqual(H->GetActorLocation().Z,41.,.1));H->Destroy();
    auto* Dropper=W->SpawnActor<AEnemyBase>(FVector(1500,0,700),FRotator::ZeroRotator,Params);
    Dropper->DispatchBeginPlay();Setup(Dropper);
    TestTrue(TEXT("Spawned enemy feet snap to the floor"),FMath::IsNearlyEqual(Dropper->GetActorLocation().Z-Dropper->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),37.,1.));
    Dropper->StopEnemyBehavior();Dropper->bDropsXP=true;Dropper->XPReward=1;
    Dropper->ExperiencePickupClass=AExperiencePickup::StaticClass();Dropper->SpawnExperiencePickup();
    int32 GroundedDrops=0;
    for(TActorIterator<AExperiencePickup> It(W);It;++It) if(It->GetOwner()==Dropper)
    {
        ++GroundedDrops;
        TestTrue(TEXT("XP rests at floor-relative height rather than body height"),FMath::IsNearlyEqual(It->GetActorLocation().Z,55.,.1));
    }
    TestEqual(TEXT("Ground placement preserves the XP drop"),GroundedDrops,1);
    auto* BossClass=LoadClass<ACinderOracleBoss>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/EnemyCharacters/Bosses/CinderOracle/BP_CinderOracle.BP_CinderOracle_C"));
    auto* Boss=W->SpawnActor<ACinderOracleBoss>(BossClass,FVector::ZeroVector,FRotator::ZeroRotator,Params);Setup(Boss);Boss->bOracleActive=true;
    for(int32 Spell=0;Spell<7;++Spell)
    {
        Boss->CastAbility(Spell);TestTrue(FString::Printf(TEXT("Boss spell %d creates telegraphs"),Spell+1),CountHazards()>0);ClearHazards();
    }
    Boss->CastAbility(4);Boss->CastAbility(4);TestTrue(TEXT("Summon limit is four"),Boss->Guardians.Num()<=4);
    TestTrue(TEXT("Boss successfully summons grounded guards"),Boss->Guardians.Num()>0);
    for(auto Guard:Boss->Guardians) if(Guard.IsValid())
    {
        Guard->DispatchBeginPlay();
        TestTrue(TEXT("Guard feet do not inherit the boss center height"),FMath::IsNearlyEqual(Guard->GetActorLocation().Z-Guard->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),37.,1.));
    }
    Boss->StopBossCombat();TestEqual(TEXT("Boss cancellation clears hazards"),CountHazards(),0);
    TestEqual(TEXT("Boss cancellation clears guardians"),Boss->Guardians.Num(),0);

    auto* UrnClass=LoadClass<AHealingUrn>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/Encounters/BP_HealingUrn.BP_HealingUrn_C"));
    auto* Urn=W->SpawnActor<AHealingUrn>(UrnClass,FVector::ZeroVector,FRotator::ZeroRotator,Params);Setup(Urn);
    Urn->GetHealthComponent()->SetMaxHealthPreservePercent(1000000);
    int32 HealsBeforeUrn = 0; for (TActorIterator<AHealingPickup> It(W); It; ++It) ++HealsBeforeUrn;
    TestFalse(TEXT("Non-damaging contact does not break urn"),Urn->ApplyPlayerDamage(0,EPlayerAttackSource::Ninja));
    TestTrue(TEXT("Tiny Ninja hit breaks urn regardless of health scaling"),Urn->ApplyPlayerDamage(.001f,EPlayerAttackSource::Ninja));
    int32 Heals=0;for(TActorIterator<AHealingPickup> It(W);It;++It)++Heals;
    TestTrue(TEXT("Weapons can break urn"),Urn->IsDead());TestEqual(TEXT("Breaking urn creates one heal"),Heals-HealsBeforeUrn,1);
    Urn->ApplyPlayerDamage(1000,EPlayerAttackSource::Samurai);
    Heals=0;for(TActorIterator<AHealingPickup> It(W);It;++It)++Heals;
    TestEqual(TEXT("Repeated damage cannot duplicate healing drop"),Heals-HealsBeforeUrn,1);
    // This fixture has no local viewport. Exercise victory state without creating its screen widget.
    FindFProperty<FObjectPropertyBase>(ASurvivorPlayerController::StaticClass(),TEXT("VictoryWidgetClass"))->SetObjectPropertyValue_InContainer(PC,nullptr);
    Boss->bOracleActive=true;Boss->CastAbility(0);Boss->GetHealthComponent()->ApplyDamage(100000);
    TestTrue(TEXT("Caster boss death reaches final boss death state"),Boss->GetBossState()==EFinalBossState::Dead);
    TestFalse(TEXT("Caster boss death completes the run"),PC->IsRunInProgress());
    TestEqual(TEXT("Victory leaves no boss hazards"),CountHazards(),0);
    W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
}
#endif
