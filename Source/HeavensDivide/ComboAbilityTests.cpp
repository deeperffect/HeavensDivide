#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "ComboAbilityComponent.h"
#include "JointComboActor.h"
#include "CharacterManagerComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnemyBase.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HealthComponent.h"
#include "InactiveCharacterAssistComponent.h"
#include "NinjaCharacter.h"
#include "SamuraiCharacter.h"
#include "SurvivorPlayerController.h"
#include "UObject/UnrealType.h"
#include "SwapPresentationComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/App.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "PlayerUpgradeComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FComboAbilityTest, "HeavensDivide.Combat.ComboAbility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)
bool FComboAbilityTest::RunTest(const FString&)
{
    auto* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    auto* PC = World->SpawnActor<ASurvivorPlayerController>();
    auto* Upgrades=PC->GetPlayerUpgrades();
    Upgrades->BalancePreviewMetaProfile=0; // Fresh, isolated profile; never read or write the player's save.
    auto* Combo = PC->FindComponentByClass<UComboAbilityComponent>();
    auto* Party = PC->GetCharacterManager();
    Combo->BeginPlay();
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Samurai = World->SpawnActor<ASamuraiCharacter>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
    auto* Ninja = World->SpawnActor<ANinjaCharacter>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
    Samurai->SetOwner(PC); Ninja->SetOwner(PC);
    FindFProperty<FObjectProperty>(UCharacterManagerComponent::StaticClass(), TEXT("SamuraiCharacter"))->SetObjectPropertyValue_InContainer(Party, Samurai);
    FindFProperty<FObjectProperty>(UCharacterManagerComponent::StaticClass(), TEXT("NinjaCharacter"))->SetObjectPropertyValue_InContainer(Party, Ninja);
    auto SetActive = [&](ACharacterBase* Character) {
        FindFProperty<FObjectProperty>(UCharacterManagerComponent::StaticClass(), TEXT("ActiveCharacter"))->SetObjectPropertyValue_InContainer(Party, Character);
        Character->SetCharacterMode(ECharacterMode::Active);
    };
    SetActive(Samurai);
    TestTrue(TEXT("Swap available before the ability"), PC->CanSwap());
    TestTrue(TEXT("Dash available before the ability"), PC->CanDash());
    auto SpawnEnemy = [&](float Distance, EPlayerAttackSource Source) {
        auto* Enemy = World->SpawnActor<AEnemyBase>(FVector(Distance, 0, 0), FRotator::ZeroRotator, Params);
        Enemy->ConfigureObjectiveEnemy(10000.f, Source, nullptr, FLinearColor::White);
        Enemy->GetHealthComponent()->RestoreCurrentHealth(10000.f);
        return Enemy;
    };
    auto* Near = SpawnEnemy(100.f, EPlayerAttackSource::Other);
    auto* Far = SpawnEnemy(900.f, EPlayerAttackSource::Other);
    auto* NinjaOnly = SpawnEnemy(120.f, EPlayerAttackSource::Ninja);
    TestFalse(TEXT("Fresh profile has not unlocked combo abilities"),Combo->IsComboUnlocked());
    Party->OnCharacterSwapped.Broadcast(Samurai,Ninja);
    Combo->NotifySynergyAttack();
    TestEqual(TEXT("Locked combo cannot fill from swaps or synergy"),Combo->GetCombo(),0.f);
    Combo->RestoreCombo(100.f);
    TestEqual(TEXT("Locked combo ignores a stale travel meter"),Combo->GetCombo(),0.f);
    TestFalse(TEXT("Locked ability cannot activate"),Combo->TryActivateAbility());
    TestTrue(TEXT("Core swapping remains available while combo is locked"),PC->CanSwap());
    Upgrades->BalancePreviewMetaProfile=1;
    TestTrue(TEXT("Full meta preview explicitly unlocks combo"),Combo->IsComboUnlocked());
    const float ChargeMultiplier=1.f+FMath::Max(0.f,Upgrades->GetMetaSkillBonus(TEXT("ComboCharge")));
    Combo->HandleAssist(Samurai,Ninja,Near);
    TestTrue(TEXT("Committed assist grants reduced meter once"),FMath::IsNearlyEqual(Combo->GetCombo(),10.f*ChargeMultiplier));
    Combo->HandleAssist(nullptr,Ninja,Near);
    TestTrue(TEXT("Invalid assist cannot add meter"),FMath::IsNearlyEqual(Combo->GetCombo(),10.f*ChargeMultiplier));
    Combo->SetCombo(0.f);
    Combo->NotifySynergyAttack();
    TestTrue(TEXT("Non-assist synergy retains its existing gain"),FMath::IsNearlyEqual(Combo->GetCombo(),Combo->SynergyGain*ChargeMultiplier));
    Combo->SetCombo(0.f);
    TestTrue(TEXT("Full charge training grants fifteen percent"),FMath::IsNearlyEqual(Upgrades->GetMetaSkillBonus(TEXT("ComboCharge")),.15f));
    TestFalse(TEXT("Empty meter rejects activation"), Combo->TryActivateAbility());
    Party->OnCharacterSwapped.Broadcast(Samurai, Ninja);
    TestEqual(TEXT("Successful swap includes charge training"), Combo->GetCombo(), 23.f);
    Party->OnCharacterSwapped.Broadcast(Samurai, Samurai);
    TestEqual(TEXT("No-op swap gives no charge"), Combo->GetCombo(), 23.f);
    PC->FindComponentByClass<UInactiveCharacterAssistComponent>()->OnAssistAttackExecuted.Broadcast(Ninja, Samurai, Near);
    TestEqual(TEXT("Tag Team event gives reduced boosted charge once"), Combo->GetCombo(), 34.5f);
    Combo->AddCombo(-100.f);
    TestEqual(TEXT("Negative gain ignored"), Combo->GetCombo(), 34.5f);
    TestFalse(TEXT("Partial meter rejects activation"), Combo->TryActivateAbility());
    Combo->AddCombo(1000.f);
    TestEqual(TEXT("Charge caps at max"), Combo->GetCombo(), 100.f);
    TestEqual(TEXT("BalanceLab uses the current shared recipe"), Combo->GetActiveAbilityId(), FName(TEXT("BloodMoon")));
    TestEqual(TEXT("Current recipe supplies its display name"), Combo->GetActiveAbilityName().ToString(), FString(TEXT("Crimson Tempest")));
    TestTrue(TEXT("Full meter activates joint combo"), Combo->TryActivateAbility());
    TestEqual(TEXT("Activation consumes full meter once"), Combo->GetCombo(), 0.f);
    TestNotNull(TEXT("Joint actor owns execution"), Combo->ExecutingJoint.Get());
    TestFalse(TEXT("Dash blocked during performance"), PC->CanDash());
    TestFalse(TEXT("Swap blocked during performance"), PC->CanSwap());
    Combo->RestoreCombo(100.f);
    TestFalse(TEXT("Reentry cannot spend charge"), Combo->TryActivateAbility());
    TestEqual(TEXT("Rejected reentry preserves meter"), Combo->GetCombo(), 100.f);
    auto* Joint = Combo->ExecutingJoint.Get();
    Joint->Tick(Joint->GetControlDuration());
    Combo->TickComponent(.001f, LEVELTICK_All, nullptr);
    TestFalse(TEXT("Recovery follows joint clock even with slowed component delta"), Combo->IsAbilityActive());
    TestFalse(TEXT("Recovery clears character lock"), Samurai->bComboAbilityActive);
    Joint->Destroy();
    SetActive(Ninja);
    TestEqual(TEXT("Either hero uses the same recipe"), Combo->GetActiveAbilityId(), FName(TEXT("BloodMoon")));
    TestTrue(TEXT("Ninja can activate shared combo"), Combo->TryActivateAbility());
    FindFProperty<FBoolProperty>(ASurvivorPlayerController::StaticClass(), TEXT("bIsPlayerDead"))->SetPropertyValue_InContainer(PC, true);
    Combo->TickComponent(.1f, LEVELTICK_All, nullptr);
    TestFalse(TEXT("Death cancels joint cast"), Combo->IsAbilityActive());
    TestFalse(TEXT("Death clears Ninja lock"), Ninja->bComboAbilityActive);
    TestEqual(TEXT("Death restores cinematic time"), World->GetWorldSettings()->CinematicTimeDilation, 1.f);
    Combo->NotifySynergyAttack();
    TestEqual(TEXT("Death prevents charge"), Combo->GetCombo(), 0.f);
    TestNull(TEXT("Retired character tuning is absent"), FindFProperty<FProperty>(ACharacterBase::StaticClass(), TEXT("ComboAbility")));
    TestNull(TEXT("Retired pulse execution is absent"), ACharacterBase::StaticClass()->FindFunctionByName(TEXT("ExecuteComboAbility")));
    Combo->EndPlay(EEndPlayReason::Destroyed);
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}
#endif
