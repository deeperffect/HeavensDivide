#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "AutoAttackComponent.h"
#include "AttackProjectileBase.h"
#include "EngineUtils.h"
#include "InactiveCharacterAssistComponent.h"
#include "SurvivorAbilityComponent.h"
#include "SurvivorPlayerController.h"
#include "CharacterManagerComponent.h"
#include "PlayerUpgradeComponent.h"
#include "SamuraiCharacter.h"
#include "NinjaCharacter.h"
#include "NinjaBuildComponent.h"
#include "EnemyStatusEffectComponent.h"
#include "HealthComponent.h"
#include "Components/SkeletalMeshComponent.h"
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
    auto* Samurai = World->SpawnActor<ASamuraiCharacter>(FVector(10000,0,0), FRotator::ZeroRotator, Params);
    auto* Ninja = World->SpawnActor<ANinjaCharacter>(NinjaClass, FVector::ZeroVector, FRotator::ZeroRotator, Params);
    Samurai->SetOwner(PC); Ninja->SetOwner(PC);
    auto SetParty = [&](const TCHAR* Name, ACharacterBase* Value) {
        FindFProperty<FObjectProperty>(UCharacterManagerComponent::StaticClass(), Name)->SetObjectPropertyValue_InContainer(Manager, Value);
    };
    SetParty(TEXT("SamuraiCharacter"), Samurai); SetParty(TEXT("NinjaCharacter"), Ninja); SetParty(TEXT("ActiveCharacter"), Samurai);
    Samurai->SetCharacterMode(ECharacterMode::Active);
    Ninja->SetCharacterMode(ECharacterMode::Inactive);
    Ninja->GetMesh()->InitAnim(true);
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
    FPlayerUpgradeRunState Empty; Upgrades->CaptureRunState(Empty);
    for (const TCHAR* Stance : {TEXT("ExecutionStance"), TEXT("BloodStance"), TEXT("WaveStance")})
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
        for (int32 Index=0; Index<Assist->AttacksPerAssist; ++Index)
            SamuraiAttack->OnAutoAttack.Broadcast(SamuraiAttack, EAutoAttackSource::NormalAutoAttack);
        TestTrue(TEXT("Stance attacks call Ninja from her old distant location"), Assist->bAssistActive);
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
    }
    Assist->DeactivateAssistEffect(true);
    Upgrades->RestoreRunState(Empty);
    Samurai->SetCharacterMode(ECharacterMode::Assisting);
    Samurai->SetVisualFacingRotation(FRotator::ZeroRotator);
    TestTrue(TEXT("Samurai assist hits"), Ability->ExecuteSetupAssist(Samurai));
    TestFalse(TEXT("Samurai assist cannot grant free Bleed"), Enemy->HasStatus(EEnemyStatusEffect::Bleed));
    Upgrades->AcquireUpgrade(Upgrades->FindUpgradeDefinition(TEXT("BleedingEdge")));
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
    World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
    return !HasAnyErrors();
}
#endif
