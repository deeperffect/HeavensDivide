#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "AutoAttackComponent.h"
#include "AttackProjectileBase.h"
#include "EngineUtils.h"
#include "InactiveCharacterAssistComponent.h"
#include "FinalBossBase.h"
#include "RunTravelSubsystem.h"
#include "SwapPresentationComponent.h"
#include "CharacterSmokeBurst.h"
#include "NiagaraComponent.h"
#include "SurvivorAbilityComponent.h"
#include "SurvivorPlayerController.h"
#include "CharacterManagerComponent.h"
#include "PlayerUpgradeComponent.h"
#include "SamuraiCharacter.h"
#include "NinjaCharacter.h"
#include "NinjaBuildComponent.h"
#include "NinjaBuildProjectile.h"
#include "SamuraiBladeWave.h"
#include "SamuraiIaijutsu.h"
#include "TimerManager.h"
#include "EnemyStatusEffectComponent.h"
#include "HealthComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTagTeamRegressionTest, "HeavensDivide.Combat.TagTeamRegression",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTagTeamRegressionTest::RunTest(const FString&)
{
    auto* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    auto* PCClass = LoadClass<ASurvivorPlayerController>(nullptr,
        TEXT("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController.BP_SurvivorPlayerController_C"));
    auto* PC = World->SpawnActor<ASurvivorPlayerController>(PCClass);
    if (!TestNotNull(TEXT("Saved controller"), PC)) return false;
    PC->GetPlayerHealthComponent()->RestoreCurrentHealth(100);
    auto* Manager = PC->GetCharacterManager();
    auto* Upgrades = PC->GetPlayerUpgrades();
    auto* NinjaClass = Cast<UClass>(FindFProperty<FClassProperty>(UCharacterManagerComponent::StaticClass(),
        TEXT("NinjaClass"))->GetObjectPropertyValue_InContainer(Manager));
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* SamuraiClass = Cast<UClass>(FindFProperty<FClassProperty>(UCharacterManagerComponent::StaticClass(),
        TEXT("SamuraiClass"))->GetObjectPropertyValue_InContainer(Manager));
    auto* Samurai = World->SpawnActor<ASamuraiCharacter>(SamuraiClass, FVector(10000,0,0), FRotator::ZeroRotator, Params);
    auto* Ninja = World->SpawnActor<ANinjaCharacter>(NinjaClass, FVector::ZeroVector, FRotator::ZeroRotator, Params);
    Samurai->SetOwner(PC); Ninja->SetOwner(PC);
    auto SetParty = [&](const TCHAR* Name, ACharacterBase* Value) {
        FindFProperty<FObjectProperty>(UCharacterManagerComponent::StaticClass(), Name)->SetObjectPropertyValue_InContainer(Manager, Value);
    };
    SetParty(TEXT("SamuraiCharacter"), Samurai); SetParty(TEXT("NinjaCharacter"), Ninja); SetParty(TEXT("ActiveCharacter"), Samurai);
    Samurai->SetCharacterMode(ECharacterMode::Active);
    Ninja->SetCharacterMode(ECharacterMode::Inactive);
    Ninja->GetMesh()->InitAnim(true);
    Samurai->GetMesh()->InitAnim(true);
    auto* SamuraiAttack = Samurai->FindComponentByClass<UAutoAttackComponent>();
    auto* NinjaAttack = Ninja->FindComponentByClass<UAutoAttackComponent>();
    SamuraiAttack->OwnerCharacter = Samurai; NinjaAttack->OwnerCharacter = Ninja;
    auto* Assist = PC->FindComponentByClass<UInactiveCharacterAssistComponent>();
    Assist->SurvivorController = PC; Assist->PlayerUpgrades = Upgrades;
    auto* Ability = PC->FindComponentByClass<USurvivorAbilityComponent>();
    Ability->Controller = PC; Ability->Upgrades = Upgrades;
    auto SpawnEnemy = [&](FVector Position) {
        auto* Enemy = World->SpawnActor<AEnemyBase>(Position, FRotator::ZeroRotator, Params);
        Enemy->ConfigureObjectiveEnemy(10000, EPlayerAttackSource::Other, nullptr, FLinearColor::White);
        Enemy->GetHealthComponent()->RestoreCurrentHealth(10000);
        return Enemy;
    };
    auto* Enemy = SpawnEnemy(FVector(10200,0,0));
    auto* Neighbor = SpawnEnemy(FVector(10300,0,0));
    auto* OldLocationEnemy = SpawnEnemy(FVector(200,0,0));
    const auto SmokeCount=[&](ACharacterBase* Hero) {
        int32 Count=0;for(TActorIterator<ACharacterSmokeBurst> It(World);It;++It)
            if(It->GetEffect()->GetAsset()==Hero->SwapPresentation->CharacterSmokeVFX)++Count;
        return Count;
    };
    TestNotNull(TEXT("Samurai smoke assigned"),Samurai->SwapPresentation->CharacterSmokeVFX.Get());
    TestNotNull(TEXT("Ninja smoke assigned"),Ninja->SwapPresentation->CharacterSmokeVFX.Get());
    TestTrue(TEXT("Heroes have distinct red and purple smoke"),Samurai->SwapPresentation->CharacterSmokeVFX!=Ninja->SwapPresentation->CharacterSmokeVFX);
    FPlayerUpgradeRunState Empty; Upgrades->CaptureRunState(Empty);
    for (const TCHAR* Stance : {TEXT("BladeWave"), TEXT("Iaijutsu"), TEXT("BattleStance")})
    {
        Assist->DeactivateAssistEffect(true);
        Upgrades->RestoreRunState(Empty);
        TestTrue(Stance, Upgrades->AcquireUpgrade(Upgrades->FindUpgradeDefinition(Stance)));
        TestTrue(TEXT("Tag Team acquired"), Upgrades->AcquireUpgrade(Upgrades->FindUpgradeDefinition(TEXT("TagTeam"))));
        Ninja->SetActorLocation(FVector::ZeroVector);
        Ninja->SetCharacterMode(ECharacterMode::Inactive);
        NinjaAttack->StopAutoAttack();
        Assist->RefreshAssistEffectState();
        SamuraiAttack->OnAutoAttack.Broadcast(SamuraiAttack, EAutoAttackSource::Assist);
        TestEqual(TEXT("Assist attacks cannot recursively count"), Assist->CurrentAttackCount, 0);
        const int32 BeforeSmoke=SmokeCount(Ninja);
        for (int32 Index=0; Index<Assist->AttacksPerAssist; ++Index)
            SamuraiAttack->OnAutoAttack.Broadcast(SamuraiAttack, EAutoAttackSource::NormalAutoAttack);
        TestTrue(TEXT("Stance attacks call Ninja from her old distant location"), Assist->bAssistActive);
        TestEqual(TEXT("Each committed Ninja assist arrives in purple smoke"),SmokeCount(Ninja),BeforeSmoke+1);
        TestTrue(TEXT("Ninja targets an enemy near her new arrival location"),
            NinjaAttack->CurrentAttackTarget == Enemy || NinjaAttack->CurrentAttackTarget == Neighbor);
        TestTrue(TEXT("Old location does not choose the target"), NinjaAttack->CurrentAttackTarget != OldLocationEnemy);
        TestEqual(TEXT("Successful assist resets progress"), Assist->CurrentAttackCount, 0);
        NinjaAttack->CurrentAttackTarget = OldLocationEnemy;
        OldLocationEnemy->GetHealthComponent()->RestoreCurrentHealth(0);
        NinjaAttack->bAutoAttackEnabled = true;
        TSet<AAttackProjectileBase*> ExistingProjectiles;
        for (TActorIterator<AAttackProjectileBase> It(World); It; ++It) ExistingProjectiles.Add(*It);
        NinjaAttack->SpawnAutoAttackProjectile();
        int32 NewProjectiles = 0;
        for (TActorIterator<AAttackProjectileBase> It(World); It; ++It)
            if (!ExistingProjectiles.Contains(*It))
            {
                ++NewProjectiles;
                const FVector ToLiveEnemy = (Enemy->GetActorLocation() - It->GetActorLocation()).GetSafeNormal2D();
                TestTrue(TEXT("Assist projectile aims toward live enemies instead of its dead target"),
                    FVector::DotProduct(It->GetActorForwardVector().GetSafeNormal2D(), ToLiveEnemy) > 0.5f);
            }
        TestTrue(TEXT("Retargeted assist actually launches projectiles"), NewProjectiles > 0);
        OldLocationEnemy->GetHealthComponent()->RestoreCurrentHealth(10000);
        Assist->FinishCurrentAssist();
        TestEqual(TEXT("Ninja assist departs in independent smoke"),SmokeCount(Ninja),BeforeSmoke+2);
    }
    Assist->DeactivateAssistEffect(true);
    Upgrades->RestoreRunState(Empty);
    Upgrades->AcquireUpgrade(Upgrades->FindUpgradeDefinition(TEXT("BattleStance")));
    Upgrades->AcquireUpgrade(Upgrades->FindUpgradeDefinition(TEXT("BloodAssist")));
    TestFalse(TEXT("Blood assist needs no Tag Team unlock"),Upgrades->HasUpgradeId(TEXT("TagTeam")));
    TestTrue(TEXT("Blood upgrade can start a real Ninja assist"),Assist->TryBloodAssist());
    Assist->FinishCurrentAssist();
    Upgrades->RestoreRunState(Empty);
    Upgrades->AcquireUpgrade(Upgrades->FindUpgradeDefinition(TEXT("Iaijutsu")));
    Upgrades->AcquireUpgrade(Upgrades->FindUpgradeDefinition(TEXT("IaijutsuAssist")));
    auto* IaijutsuAssist = Upgrades->FindUpgradeDefinition(TEXT("IaijutsuAssist"));
    const auto SavedAssistBalance = IaijutsuAssist->BalanceParameters;
    SamuraiAttack->bAutoAttackEnabled = true;
    IaijutsuAssist->BalanceParameters.Add(TEXT("Chance"), 0.f);
    SamuraiAttack->RollIaijutsuAssist();
    TestFalse(TEXT("Failed Iaijutsu RNG roll does not start an assist"), Assist->bAssistActive);
    IaijutsuAssist->BalanceParameters.Add(TEXT("Chance"), 1.f);
    SamuraiAttack->RollIaijutsuAssist();
    TestTrue(TEXT("Successful Iaijutsu RNG roll starts real Ninja assist without Tag Team"), Assist->bAssistActive);
    TestFalse(TEXT("Busy RNG assist cannot overlap itself"), Assist->TryBloodAssist());
    IaijutsuAssist->BalanceParameters = SavedAssistBalance;
    Assist->FinishCurrentAssist();
    Upgrades->RestoreRunState(Empty);
    Samurai->SetCharacterMode(ECharacterMode::Assisting);
    Samurai->SetVisualFacingRotation(FRotator::ZeroRotator);
    TestTrue(TEXT("Samurai assist hits"), Ability->ExecuteSetupAssist(Samurai));
    TestFalse(TEXT("Samurai assist cannot grant free Bleed"), Enemy->HasStatus(EEnemyStatusEffect::Bleed));
    Upgrades->AcquireUpgrade(Upgrades->FindUpgradeDefinition(TEXT("BattleStance")));
    Ability->ExecuteSetupAssist(Samurai);
    TestTrue(TEXT("Samurai assist retains unlocked Bleed"), Enemy->HasStatus(EEnemyStatusEffect::Bleed));
    Neighbor->GetStatusEffectComponent()->ClearAllStatuses();
    Ability->BladeWaveImpact(Enemy, 100, false);
    const float HealthBefore = Enemy->GetHealthComponent()->GetCurrentHealth();
    Ninja->FindComponentByClass<UNinjaBuildComponent>()->Hit(Enemy, 10, false, true);
    TestEqual(TEXT("Ninja assist adds no Prepare bonus damage"), Enemy->GetHealthComponent()->GetCurrentHealth(), HealthBefore - 10);
    TestFalse(TEXT("Ninja assist cannot spread Bleed"), Neighbor->HasStatus(EEnemyStatusEffect::Bleed));
    Enemy->GetStatusEffectComponent()->ClearAllStatuses();
    Upgrades->RestoreRunState(Empty);
    Ninja->FindComponentByClass<UNinjaBuildComponent>()->Hit(Enemy, 10, false, true);
    TestFalse(TEXT("Ninja assist creates no Bleed without upgrades"), Enemy->HasStatus(EEnemyStatusEffect::Bleed));
    // Exercise the real counter -> placement -> attack dispatch for all Samurai
    // stances while the Ninja is active. A normal attack is still cooling down.
    Assist->DeactivateAssistEffect(true);
    SetParty(TEXT("ActiveCharacter"), Ninja);
    Samurai->SetCharacterMode(ECharacterMode::Inactive);
    Ninja->SetCharacterMode(ECharacterMode::Active);
    Ninja->SetActorLocation(FVector(20000,0,0));
    Ninja->SetVisualFacingRotation(FRotator::ZeroRotator);
    auto* AssistEnemy = SpawnEnemy(FVector(20500,0,0));
    for (const TCHAR* Stance : {TEXT("BattleStance"), TEXT("BladeWave"), TEXT("Iaijutsu")})
    {
        Assist->DeactivateAssistEffect(true);
        Upgrades->RestoreRunState(Empty);
        TestTrue(TEXT("Assistant stance acquired"), Upgrades->AcquireUpgrade(Upgrades->FindUpgradeDefinition(Stance)));
        TestTrue(TEXT("Tag Team acquired for Samurai assist"), Upgrades->AcquireUpgrade(Upgrades->FindUpgradeDefinition(TEXT("TagTeam"))));
        const FName ExtraCut = FString(Stance)==TEXT("BattleStance") ? TEXT("DoubleCut")
            : FString(Stance)==TEXT("BladeWave") ? TEXT("CrescentDoubleCut") : TEXT("IaijutsuDoubleCut");
        TestTrue(TEXT("Assistant has a stored extra-hit mechanic"), Upgrades->AcquireUpgrade(Upgrades->FindUpgradeDefinition(ExtraCut)));
        AssistEnemy->GetStatusEffectComponent()->ClearAllStatuses();
        AssistEnemy->GetHealthComponent()->RestoreCurrentHealth(10000);
        Samurai->SetCharacterMode(ECharacterMode::Inactive);
        Samurai->SetActorLocation(FVector::ZeroVector);
        SamuraiAttack->StopAutoAttack();
        SamuraiAttack->LastAttackStartTime = 100;
        SamuraiAttack->NextAttackReadyTime = 200;
        SamuraiAttack->AttackIntervalAtLastAttackStart = 1.75f;
        SamuraiAttack->DoubleCutPrimaryAttackCounter = 2;
        SamuraiAttack->bDoubleCutReady = true;
        SamuraiAttack->IaijutsuAttackCounter = 2;
        SamuraiAttack->CrossingBladesAttackCounter = 2;
        Assist->RefreshAssistEffectState();
        for (int32 I=0; I<Assist->AttacksPerAssist; ++I)
            NinjaAttack->OnAutoAttack.Broadcast(NinjaAttack, EAutoAttackSource::NormalAutoAttack);
        TestTrue(TEXT("Real counter summons Samurai"), Assist->bAssistActive);
        TestTrue(TEXT("Samurai chooses the nearby live target"), SamuraiAttack->CurrentAttackTarget == AssistEnemy);
        TestEqual(TEXT("Assist preserves last normal attack time"), SamuraiAttack->LastAttackStartTime, 100.);
        TestEqual(TEXT("Assist preserves next normal attack time"), SamuraiAttack->NextAttackReadyTime, 200.);
        TestEqual(TEXT("Assist preserves normal interval"), SamuraiAttack->AttackIntervalAtLastAttackStart, 1.75f);
        TArray<ASamuraiBladeWave*> Waves;
        TArray<ASamuraiIaijutsu*> Lanes;
        for (TActorIterator<ASamuraiBladeWave> It(World); It; ++It) Waves.Add(*It);
        for (TActorIterator<ASamuraiIaijutsu> It(World); It; ++It) Lanes.Add(*It);
        const bool bCrescent = FString(Stance)==TEXT("BladeWave"), bIaijutsu = FString(Stance)==TEXT("Iaijutsu");
        TestEqual(TEXT("Crescent assist launches exactly one wave"), Waves.Num(), bCrescent ? 1 : 0);
        TestEqual(TEXT("Iaijutsu assist launches exactly one charged lane"), Lanes.Num(), bIaijutsu ? 1 : 0);
        if (bCrescent || bIaijutsu)
        {
            TestTrue(TEXT("Ranged stance uses ranged arrival placement"), SamuraiAttack->UsesRangedAssistPlacement());
            TestEqual(TEXT("No generic melee damage on stance launch"), AssistEnemy->GetHealthComponent()->GetCurrentHealth(), 10000.f);
            SamuraiAttack->PerformAttackTrace();
            TestEqual(TEXT("Stance montage cannot add a melee hit"), AssistEnemy->GetHealthComponent()->GetCurrentHealth(), 10000.f);
            TestFalse(TEXT("Legacy setup cannot bypass the equipped stance"), Ability->ExecuteSetupAssist(Samurai));
            if (bCrescent && !Waves.IsEmpty())
            {
                Waves[0]->HandleOverlap(nullptr, AssistEnemy, nullptr, 0, false, FHitResult());
                TestTrue(TEXT("Crescent assist deals wave damage"), AssistEnemy->GetHealthComponent()->GetCurrentHealth()<10000.f);
                TestTrue(TEXT("Crescent assist applies stance slow"), AssistEnemy->GetCrescentMovementMultiplier()<1.f);
            }
            if (bIaijutsu && !Lanes.IsEmpty())
            {
                TestTrue(TEXT("Lane retains assist identity"), Lanes[0]->bAssistAttack);
                const float Charge = SamuraiAttack->GetIaijutsuChargeDuration();
                TestTrue(TEXT("Assistant remains visible beyond charge completion"), World->GetTimerManager().GetTimerRemaining(Assist->AssistCleanupTimerHandle)>Charge);
                Lanes[0]->Tick(Charge+.01f);
                TestTrue(TEXT("Iaijutsu assist deals charged lane damage"), AssistEnemy->GetHealthComponent()->GetCurrentHealth()<10000.f);
                TestTrue(TEXT("Iaijutsu assist applies its mark"), AssistEnemy->GetIaijutsuDamageMultiplier()>1.f);
                TestEqual(TEXT("Resolving assist cannot count as a normal attack"), Assist->CurrentAttackCount, 0);
            }
        }
        else
        {
            TestFalse(TEXT("Blood assist keeps melee placement"), SamuraiAttack->UsesRangedAssistPlacement());
            SamuraiAttack->PerformAttackTrace();
            TestTrue(TEXT("Blood assist deals its melee hit"), AssistEnemy->GetHealthComponent()->GetCurrentHealth()<10000.f);
            TestTrue(TEXT("Blood assist applies Bleed"), AssistEnemy->HasStatus(EEnemyStatusEffect::Bleed));
        }
        const float AfterHit = AssistEnemy->GetHealthComponent()->GetCurrentHealth();
        SamuraiAttack->PerformAttackTrace();
        TestEqual(TEXT("Repeated notify cannot duplicate assist damage"), AssistEnemy->GetHealthComponent()->GetCurrentHealth(), AfterHit);
        TestEqual(TEXT("Assist preserves Blood cadence"), SamuraiAttack->DoubleCutPrimaryAttackCounter, 2);
        TestTrue(TEXT("Assist preserves stored Blood Double Cut"), SamuraiAttack->bDoubleCutReady);
        TestEqual(TEXT("Assist preserves Iaijutsu cadence"), SamuraiAttack->IaijutsuAttackCounter, 2);
        TestEqual(TEXT("Assist preserves Crescent cadence"), SamuraiAttack->CrossingBladesAttackCounter, 2);
        for (auto* Wave : Waves) Wave->Destroy();
        for (auto* Lane : Lanes) Lane->Destroy();
        Assist->FinishCurrentAssist();
    }
    // The same real assist path must retain all three Ninja stance projectiles.
    Assist->DeactivateAssistEffect(true);
    SetParty(TEXT("ActiveCharacter"), Samurai);
    Samurai->SetCharacterMode(ECharacterMode::Active);
    Samurai->SetActorLocation(FVector(30000,0,0));
    Samurai->SetVisualFacingRotation(FRotator::ZeroRotator);
    Ninja->SetCharacterMode(ECharacterMode::Inactive);
    auto* NinjaTarget = SpawnEnemy(FVector(30400,0,0));
    for (const TCHAR* Stance : {TEXT("ReturningFang"), TEXT("BarrageStance"), TEXT("GreatShuriken")})
    {
        Assist->DeactivateAssistEffect(true);
        Upgrades->RestoreRunState(Empty);
        Upgrades->AcquireUpgrade(Upgrades->FindUpgradeDefinition(TEXT("BattleStance")));
        TestTrue(TEXT("Ninja assistant stance acquired"), Upgrades->AcquireUpgrade(Upgrades->FindUpgradeDefinition(Stance)));
        Upgrades->AcquireUpgrade(Upgrades->FindUpgradeDefinition(TEXT("TagTeam")));
        Ninja->SetCharacterMode(ECharacterMode::Inactive);
        NinjaAttack->StopAutoAttack();
        Assist->RefreshAssistEffectState();
        for (int32 I=0; I<Assist->AttacksPerAssist; ++I)
            SamuraiAttack->OnAutoAttack.Broadcast(SamuraiAttack, EAutoAttackSource::NormalAutoAttack);
        TestTrue(TEXT("Counter summons Ninja in selected stance"), Assist->bAssistActive);
        TestTrue(TEXT("Ninja assistant uses the nearby target"), NinjaAttack->CurrentAttackTarget==NinjaTarget);
        TSet<AActor*> Before;
        for (TActorIterator<ANinjaBuildProjectile> It(World); It; ++It) Before.Add(*It);
        for (TActorIterator<AAttackProjectileBase> It(World); It; ++It) Before.Add(*It);
        NinjaAttack->SpawnAutoAttackProjectile();
        NinjaAttack->SpawnAutoAttackProjectile();
        int32 Blades=0, Kunai=0;
        for (TActorIterator<ANinjaBuildProjectile> It(World); It; ++It)
            if (!Before.Contains(*It) && It->bAssistProjectile)
            {
                ++Blades;
                TestEqual(TEXT("Assist blade matches equipped stance"), It->Kind,
                    FString(Stance)==TEXT("ReturningFang") ? ENinjaProjectileKind::ReturningFang : ENinjaProjectileKind::GreatShuriken);
            }
        for (TActorIterator<AAttackProjectileBase> It(World); It; ++It)
            if (!Before.Contains(*It) && It->bAssistProjectile) ++Kunai;
        const bool bBarrage=FString(Stance)==TEXT("BarrageStance");
        TestEqual(TEXT("Exactly one Fang/Shuriken, no duplicate notify"), Blades, bBarrage ? 0 : 1);
        TestTrue(TEXT("Only Barrage fires a kunai volley"), bBarrage ? Kunai==3 : Kunai==0);
        Assist->FinishCurrentAssist();
        for (TActorIterator<ANinjaBuildProjectile> It(World); It; ++It) if (!Before.Contains(*It)) It->Destroy();
        for (TActorIterator<AAttackProjectileBase> It(World); It; ++It) if (!Before.Contains(*It)) It->Destroy();
    }
    // Reproduce the arena handoff: upgrades are restored without acquisition
    // events, and keeping Samurai active does not perform a swap to refresh assists.
    auto* Travel = NewObject<URunTravelSubsystem>(NewObject<UGameInstance>(GEngine));
    auto* Floor = World->SpawnActor<AActor>();
    auto* FloorBox = NewObject<UBoxComponent>(Floor);
    Floor->SetRootComponent(FloorBox);
    FloorBox->SetBoxExtent(FVector(2500, 2500, 10));
    FloorBox->SetCollisionProfileName(TEXT("BlockAll"));
    FloorBox->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Block);
    FloorBox->RegisterComponent();
    Floor->SetActorLocation(FVector(40000, 0, 170)); // The collision surface is exactly Z=180.
    Ninja->GetCapsuleComponent()->SetCapsuleHalfHeight(110);
    auto* Boss = World->SpawnActor<AFinalBossBase>(FVector(40800, 0, 630), FRotator::ZeroRotator, Params);
    if (TestNotNull(TEXT("Large boss assist target"), Boss))
    {
        Boss->ConfigureObjectiveEnemy(10000, EPlayerAttackSource::Other, nullptr, FLinearColor::White);
        Boss->GetCapsuleComponent()->SetCapsuleSize(320, 450);
        Samurai->SwapPresentation->bEnabled = false;
        Ninja->SwapPresentation->bEnabled = false;
        for (bool bNinjaActive : {false, true})
        {
            Upgrades->RestoreRunState(Empty);
            Upgrades->AcquireUpgrade(Upgrades->FindUpgradeDefinition(TEXT("BattleStance")));
            Upgrades->AcquireUpgrade(Upgrades->FindUpgradeDefinition(TEXT("BarrageStance")));
            Upgrades->AcquireUpgrade(Upgrades->FindUpgradeDefinition(TEXT("TagTeam")));
            ACharacterBase* SavedActive = bNinjaActive ? static_cast<ACharacterBase*>(Ninja) : Samurai;
            SetParty(TEXT("ActiveCharacter"), SavedActive);
            Samurai->SetCharacterMode(bNinjaActive ? ECharacterMode::Inactive : ECharacterMode::Active);
            Ninja->SetCharacterMode(bNinjaActive ? ECharacterMode::Active : ECharacterMode::Inactive);
            SavedActive->SetActorLocation(FVector(40000, 0, 180 + SavedActive->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
            PC->Possess(SavedActive);
            TestTrue(TEXT("Arena travel captures equipped Tag Team"), Travel->CaptureFromController(PC));
            Upgrades->RestoreRunState(Empty);
            TestFalse(TEXT("Empty snapshot disables old Tag Team bindings"), Assist->bAssistEffectActive);
            SetParty(TEXT("ActiveCharacter"), Samurai);
            Samurai->SetCharacterMode(ECharacterMode::Active);
            Ninja->SetCharacterMode(ECharacterMode::Inactive);
            Samurai->SetActorLocation(FVector(40000, 0, 180 + Samurai->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
            PC->Possess(Samurai);
            TestTrue(TEXT("Arena travel restores the run"), Travel->RestoreToController(PC));
            TestTrue(TEXT("Restored Tag Team is enabled without a new upgrade"), Assist->bAssistEffectActive);
            TestTrue(TEXT("Restored active character is correct"), Manager->GetActiveCharacter() == SavedActive);
            auto* Assistant = Manager->GetInactiveCharacter();
            auto* ActiveAttack = SavedActive->FindComponentByClass<UAutoAttackComponent>();
            auto* AssistantAttack = Assistant->FindComponentByClass<UAutoAttackComponent>();
            Assistant->SetActorLocation(FVector(0, 0, -6000));
            SamuraiAttack->StopAutoAttack();
            NinjaAttack->StopAutoAttack();
            Boss->GetHealthComponent()->RestoreCurrentHealth(10000);
            // Travel intentionally suppressed swap presentation; enable the
            // assisting hero before checking its shared smoke presentation.
            Assistant->SwapPresentation->bEnabled=true;
            const int32 BeforeSmoke=SmokeCount(Assistant);
            for (int32 I = 0; I < Assist->AttacksPerAssist; ++I)
                ActiveAttack->OnAutoAttack.Broadcast(ActiveAttack, EAutoAttackSource::NormalAutoAttack);
            TestTrue(TEXT("Restored counter triggers against a boss"), Assist->bAssistActive);
            TestEqual(TEXT("Both heroes use their smoke on successful assist"),SmokeCount(Assistant),BeforeSmoke+1);
            TestTrue(TEXT("Assist selects the boss"), AssistantAttack->CurrentAttackTarget == Boss);
            // Possession can lift the player's capsule slightly off the floor.
            // Compare against the fixture's actual surface, not that stand-off.
            const double ExpectedZ = 180.0 + Assistant->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
            TestEqual(TEXT("Assist uses current floor height rather than stale hidden pawn height"),
                Assistant->GetActorLocation().Z, ExpectedZ + 2.f, .01);
            if (bNinjaActive)
            {
                TestTrue(TEXT("Melee assist stays outside the large boss capsule"),
                    FVector::Dist2D(Assistant->GetActorLocation(), Boss->GetActorLocation()) >
                    Boss->GetCapsuleComponent()->GetScaledCapsuleRadius() + Assistant->GetCapsuleComponent()->GetScaledCapsuleRadius());
                SamuraiAttack->PerformAttackTrace();
                TestTrue(TEXT("Melee assist actually damages the large boss"), Boss->GetHealthComponent()->GetCurrentHealth() < 10000);
                Assist->FinishCurrentAssist();
            }
            else
            {
                Assist->DeactivateAssistEffect(false);
                TestTrue(TEXT("Non-canceling deactivation preserves assist cleanup"), World->GetTimerManager().IsTimerActive(Assist->AssistCleanupTimerHandle));
                // A timer created before this fixture's first timer tick is
                // pending until that tick; advance it before measuring expiry.
                ++GFrameCounter;
                World->GetTimerManager().Tick(.01f);
                const float Remaining = World->GetTimerManager().GetTimerRemaining(Assist->AssistCleanupTimerHandle);
                ++GFrameCounter;
                World->GetTimerManager().Tick(FMath::Max(.01f, Remaining) + .1f);
                TestFalse(TEXT("An allowed assist finishes after deactivation"), Assist->bAssistActive);
                TestEqual(TEXT("Completed assistant returns to inactive"), Assistant->GetCharacterMode(), ECharacterMode::Inactive);
            }
            Assistant->SwapPresentation->bEnabled=false;
        }
    }
    Assist->DeactivateAssistEffect(true);
    World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
    return !HasAnyErrors();
}
#endif
