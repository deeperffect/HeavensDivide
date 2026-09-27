#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "ComboAbilityComponent.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FComboAbilityTest, "HeavensDivide.Combat.ComboAbility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FComboAbilityTest::RunTest(const FString&)
{
    auto* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    auto* PC = World->SpawnActor<ASurvivorPlayerController>();
    auto* Combo = PC->FindComponentByClass<UComboAbilityComponent>();
    auto* Party = PC->GetCharacterManager();
    Combo->BeginPlay();
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Samurai = World->SpawnActor<ASamuraiCharacter>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
    auto* Ninja = World->SpawnActor<ANinjaCharacter>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
    Samurai->SetOwner(PC); Ninja->SetOwner(PC);
    // Exercise pulse timing independently, then cover the real-time activation freeze below.
    Samurai->ComboAbility.bFreezeOnActivation = false;
    Ninja->ComboAbility.bFreezeOnActivation = false;
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
    TestFalse(TEXT("Empty meter rejects activation"), Combo->TryActivateAbility());
    Party->OnCharacterSwapped.Broadcast(Samurai, Ninja);
    TestEqual(TEXT("Successful swap gives charge"), Combo->GetCombo(), 20.f);
    Party->OnCharacterSwapped.Broadcast(Samurai, Samurai);
    TestEqual(TEXT("No-op swap gives no charge"), Combo->GetCombo(), 20.f);
    PC->FindComponentByClass<UInactiveCharacterAssistComponent>()->OnAssistAttackExecuted.Broadcast(Ninja, Samurai, Near);
    TestEqual(TEXT("Tag Team event gives charge once"), Combo->GetCombo(), 45.f);
    Combo->AddCombo(-100.f);
    TestEqual(TEXT("Negative gain ignored"), Combo->GetCombo(), 45.f);
    TestFalse(TEXT("Partial meter rejects activation"), Combo->TryActivateAbility());
    Combo->AddCombo(1000.f);
    TestEqual(TEXT("Charge caps at max"), Combo->GetCombo(), 100.f);
    Samurai->ComboAbility.bEnabled = false;
    TestFalse(TEXT("Disabled ability preserves full meter"), Combo->TryActivateAbility());
    TestTrue(TEXT("Rejected activation does not spend charge"), Combo->IsFull());
    Samurai->ComboAbility.bEnabled = true;
    const FRotator InitialRotation = Samurai->GetMesh()->GetRelativeRotation();
    TestTrue(TEXT("Full meter activates Tornado"), Combo->TryActivateAbility());
    TestEqual(TEXT("Activation consumes full meter"), Combo->GetCombo(), 0.f);
    TestTrue(TEXT("Tornado initial pulse damages nearby enemy"), Near->GetHealthComponent()->GetCurrentHealth() < 10000.f);
    TestEqual(TEXT("Tornado starts too small for distant enemy"), Far->GetHealthComponent()->GetCurrentHealth(), 10000.f);
    TestEqual(TEXT("Samurai source respects Ninja-only enemy"), NinjaOnly->GetHealthComponent()->GetCurrentHealth(), 10000.f);
    Combo->AddCombo(100.f);
    TestFalse(TEXT("Reentry is blocked even when refilled"), Combo->TryActivateAbility());
    TestFalse(TEXT("Dash blocked during ability"), PC->CanDash());
    TestFalse(TEXT("Swap blocked during ability"), PC->CanSwap());
    Combo->TickComponent(.7f, LEVELTICK_All, nullptr);
    TestTrue(TEXT("Tornado leaves spinning to the montage"), Samurai->GetMesh()->GetRelativeRotation().Equals(InitialRotation));
    Combo->TickComponent(2.4f, LEVELTICK_All, nullptr);
    TestTrue(TEXT("Expanding Tornado reaches distant enemy"), Far->GetHealthComponent()->GetCurrentHealth() < 10000.f);
    TestFalse(TEXT("Ability finishes"), Combo->IsAbilityActive());
    TestTrue(TEXT("Mesh orientation restored"), Samurai->GetMesh()->GetRelativeRotation().Equals(InitialRotation));
    SetActive(Ninja);
    auto* NinjaBP = LoadClass<ACharacterBase>(nullptr, TEXT("/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Ninja.BP_Ninja_C"));
    if (TestNotNull(TEXT("Ninja blueprint"), NinjaBP))
    {
        const auto& Settings = NinjaBP->GetDefaultObject<ACharacterBase>()->ComboAbility;
        TestTrue(TEXT("Saved Ninja inherits per-pulse VFX"), Settings.bSpawnVFXEveryPulse);
        Ninja->ComboAbility.VFX = Settings.VFX;
        TestNotNull(TEXT("Ninja has an authored pulse effect"), Settings.VFX.Get());
    }
    for (int32 Index = 0; Index < 4; ++Index)
    {
        auto* PackEnemy = SpawnEnemy(800.f, EPlayerAttackSource::Other);
        PackEnemy->SetActorLocation(FVector(-60.f + Index * 40.f, 800.f, 0.f));
    }
    // A larger group outside the search radius must not win.
    for (int32 Index = 0; Index < 6; ++Index)
    {
        auto* DistantEnemy = SpawnEnemy(2500.f, EPlayerAttackSource::Other);
        DistantEnemy->SetActorLocation(FVector(2500.f, Index * 20.f, 0.f));
    }
    const float Before = NinjaOnly->GetHealthComponent()->GetCurrentHealth();
    TestTrue(TEXT("Shared meter activates Ninja ability"), Combo->TryActivateAbility());
    TestEqual(TEXT("First pulse spawns one VFX instance"), Combo->PendingVFX.Num(), 1);
    TWeakObjectPtr<UNiagaraComponent> FirstPulseVFX = Combo->ActiveVFX;
    TestTrue(TEXT("Ninja faces the largest nearby pack instead of the nearest enemy"), Ninja->GetVisualForwardVector().Equals(FVector::RightVector, .01f));
    Ninja->SetFacingTarget(FVector(0.f, -1000.f, 0.f));
    Ninja->Tick(.1f);
    TestTrue(TEXT("Cursor does not override ability pack facing"), Ninja->GetVisualForwardVector().Equals(FVector::RightVector, .01f));
    Combo->TickComponent(.4f, LEVELTICK_All, nullptr);
    TestEqual(TEXT("Five subsequent pulses each spawn a separate effect"), Combo->PendingVFX.Num(), 6);
    TestTrue(TEXT("Latest pulse uses a fresh Niagara instance"), Combo->ActiveVFX != FirstPulseVFX.Get());
    TestTrue(TEXT("Fresh pulse effect is activated"), Combo->ActiveVFX && Combo->ActiveVFX->IsActive());
    TestTrue(TEXT("Thousand Cuts applies multiple rapid pulses"), NinjaOnly->GetHealthComponent()->GetCurrentHealth() < Before - Ninja->ComboAbility.DamagePerPulse);
    TestEqual(TEXT("Ability pulses do not recharge their own meter"), Combo->GetCombo(), 0.f);
    FindFProperty<FBoolProperty>(ASurvivorPlayerController::StaticClass(), TEXT("bIsPlayerDead"))->SetPropertyValue_InContainer(PC, true);
    const float BeforeDeathTick = NinjaOnly->GetHealthComponent()->GetCurrentHealth();
    Combo->TickComponent(1.f, LEVELTICK_All, nullptr);
    TestFalse(TEXT("Death cancels ability"), Combo->IsAbilityActive());
    TestTrue(TEXT("Death removes all pulse effects"), Combo->PendingVFX.IsEmpty());
    TestFalse(TEXT("Death clears character lock"), Ninja->bComboAbilityActive);
    Ninja->SetFacingTarget(FVector(1000.f, 0.f, 0.f));
    Ninja->Tick(1.f);
    TestTrue(TEXT("Cancellation releases pack facing"), Ninja->GetVisualForwardVector().Equals(FVector::ForwardVector, .01f));
    TestEqual(TEXT("No damage after death"), NinjaOnly->GetHealthComponent()->GetCurrentHealth(), BeforeDeathTick);
    Combo->NotifySynergyAttack();
    TestEqual(TEXT("Dead player cannot gain combo"), Combo->GetCombo(), 0.f);
    FindFProperty<FBoolProperty>(ASurvivorPlayerController::StaticClass(), TEXT("bIsPlayerDead"))->SetPropertyValue_InContainer(PC, false);
    Ninja->ComboAbility.bFreezeOnActivation = true;
    Ninja->SwapPresentation->SwapFreezeDuration = 0.f;
    Ninja->ComboAbility.FreezeDuration = .9f;
    Ninja->ComboAbility.FreezeEaseInDuration = 0.f;
    Ninja->ComboAbility.PackSearchRadius = 1.f;
    Ninja->SetVisualFacingRotation(FRotator(0.f, 40.f, 0.f));
    World->GetWorldSettings()->SetTimeDilation(.75f);
    Combo->RestoreCombo(100.f);
    TestTrue(TEXT("Ability starts its activation freeze"), Combo->TryActivateAbility());
    TestTrue(TEXT("No nearby pack preserves initial facing"), Ninja->GetVisualFacingRotation().Equals(FRotator(0.f, 40.f, 0.f), .01f));
    TestTrue(TEXT("Ability uses swap freeze machinery"), Ninja->SwapPresentation->IsSwapFreezeActive());
    TestTrue(TEXT("Ability freezes the world"), World->GetWorldSettings()->TimeDilation <= .001f);
    const float BeforeFrozenTick = NinjaOnly->GetHealthComponent()->GetCurrentHealth();
    const double PreviousDelta = FApp::GetDeltaTime();
    FApp::SetDeltaTime(.1);
    Combo->TickComponent(.00001f, LEVELTICK_All, nullptr);
    FApp::SetDeltaTime(PreviousDelta);
    TestTrue(TEXT("Ability pulses advance in real time while enemies are frozen"), NinjaOnly->GetHealthComponent()->GetCurrentHealth() < BeforeFrozenTick);
    FindFProperty<FBoolProperty>(ASurvivorPlayerController::StaticClass(), TEXT("bIsPlayerDead"))->SetPropertyValue_InContainer(PC, true);
    Combo->TickComponent(.001f, LEVELTICK_All, nullptr);
    TestFalse(TEXT("Death cancels ability freeze"), Ninja->SwapPresentation->IsSwapFreezeActive());
    TestEqual(TEXT("Cancellation restores previous world speed"), World->GetWorldSettings()->TimeDilation, .75f);
    Combo->EndPlay(EEndPlayReason::Destroyed);
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}
#endif
