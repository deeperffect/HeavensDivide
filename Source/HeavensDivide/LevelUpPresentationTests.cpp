#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "SurvivorPlayerController.h"
#include "CharacterManagerComponent.h"
#include "CharacterBase.h"
#include "SwapPresentationComponent.h"
#include "Components/CapsuleComponent.h"
#include "CombatAudio.h"
#include "LevelUpWidget.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLevelUpPresentationTest, "HeavensDivide.UI.LevelUpPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLevelUpPresentationTest::RunTest(const FString&)
{
	auto* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->SetGameInstance(NewObject<UGameInstance>(GEngine));
	World->SetGameMode(FURL());
	World->InitializeActorsForPlay(FURL());
	auto* Class = LoadClass<ASurvivorPlayerController>(nullptr,
		TEXT("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController.BP_SurvivorPlayerController_C"));
	auto* PC = Class ? World->SpawnActor<ASurvivorPlayerController>(Class) : nullptr;
	if (!TestNotNull(TEXT("Saved player controller loads"), PC))
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		return false;
	}
	auto* Player = NewObject<ULocalPlayer>(GEngine);
	Player->SetControllerId(0);
	PC->SetPlayer(Player);
	PC->PlayerState = World->SpawnActor<APlayerState>();
	PC->CharacterManager->InitializeParty();
	TestNotNull(TEXT("Active character exists"), PC->CharacterManager->GetActiveCharacter());
	TestNotNull(TEXT("Niagara default is inherited by the saved Blueprint"), PC->LevelUpVFX.Get());
	auto* Audio = World->GetSubsystem<UCombatAudioSubsystem>();
	TestNotNull(TEXT("Existing level-up sound is configured"), Audio ? Audio->FindSound(TEXT("LevelUp")) : nullptr);
	PC->LevelUpPresentationDuration = 1.0f;
	World->GetWorldSettings()->SetTimeDilation(0.7f);
	PC->HandlePlayerLevelUp(2);
	TestTrue(TEXT("Level-up starts presentation"), PC->bLevelUpPresentationActive);
	TestFalse(TEXT("Gameplay input stays available during the effects"), PC->IsSelectingUpgrade());
	TestTrue(TEXT("Player can dash during the effects"), PC->CanDash());
	TestTrue(TEXT("Player can swap during the effects"), PC->CanSwap());
	TestEqual(TEXT("Effects preserve the current gameplay speed"), World->GetWorldSettings()->TimeDilation, 0.7f);
	TestFalse(TEXT("Effects do not apply the upgrade freeze"), PC->bLevelUpTimeDilationApplied);
	TestNull(TEXT("Menu is not created before the burst"), PC->LevelUpWidget.Get());
	UNiagaraComponent* FirstEffect = PC->LevelUpEffect;
	TestNotNull(TEXT("Niagara burst spawns"), FirstEffect);
	PC->UpdateLevelUpPresentation(0.4f);
	if (FirstEffect)
	{
        TestTrue(TEXT("Niagara effect is active before the menu"), FirstEffect->IsActive());
        auto* ActiveCharacter = PC->CharacterManager->GetActiveCharacter();
        TestEqual(TEXT("Burst is attached to the player"), FirstEffect->GetAttachParent(), ActiveCharacter->GetRootComponent());
		const FVector OldLocation = FirstEffect->GetComponentLocation();
		const FVector Movement(420.0f, -180.0f, 0.0f);
		ActiveCharacter->AddActorWorldOffset(Movement, false, nullptr, ETeleportType::TeleportPhysics);
		TestTrue(TEXT("Moving or dashing carries the burst immediately"),
			FirstEffect->GetComponentLocation().Equals(OldLocation + Movement, 0.01f));
		TestTrue(TEXT("Player can swap while the burst plays"), PC->ForceNinjaActive());
		auto* SwappedCharacter = PC->CharacterManager->GetActiveCharacter();
		if (SwappedCharacter->SwapPresentation) SwappedCharacter->SwapPresentation->FinishSwapFreeze();
		SwappedCharacter->AddActorWorldOffset(Movement, false, nullptr, ETeleportType::TeleportPhysics);
		PC->UpdateLevelUpPresentation(0.0f);
		TestEqual(TEXT("The same burst follows the new active character"), FirstEffect->GetAttachParent(), SwappedCharacter->GetRootComponent());
		const FVector NewFeet = SwappedCharacter->GetActorLocation()
			- FVector(0.0f, 0.0f, SwappedCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		TestTrue(TEXT("Burst keeps its offset at the active player's feet"),
			FirstEffect->GetComponentLocation().Equals(NewFeet + PC->LevelUpVFXOffset, 0.01f));
	}
	PC->HandlePlayerLevelUp(3);
	TestEqual(TEXT("Multiple levels are queued"), PC->PendingLevelUpChoices, 2);
	TestEqual(TEXT("Multiple levels share one burst"), PC->LevelUpEffect.Get(), FirstEffect);
	TestTrue(TEXT("Extra XP does not restart the delay"), FMath::IsNearlyEqual(PC->LevelUpPresentationRemaining, 0.6f));
	PC->RequestBloodShrineUpgradeReward();
	TestEqual(TEXT("Shrine reward is queued during presentation"), PC->PendingBloodShrineRewards, 1);
	PC->StartNextUpgradeSelection();
	TestNull(TEXT("Other rewards cannot interrupt the burst"), PC->LevelUpWidget.Get());
	PC->TogglePauseMenu();
	TestTrue(TEXT("Player can pause during the effects"), PC->IsPauseMenuOpen());
	PC->UpdateLevelUpPresentation(2.0f);
	TestTrue(TEXT("Pausing suspends the effect delay"), FMath::IsNearlyEqual(PC->LevelUpPresentationRemaining, 0.6f));
	TestNull(TEXT("Upgrade menu cannot open over the pause menu"), PC->LevelUpWidget.Get());
	PC->ResumePausedRun();
	PC->UpdateLevelUpPresentation(0.61f);
	TestFalse(TEXT("Presentation completes using real time"), PC->bLevelUpPresentationActive);
	TestNull(TEXT("Burst is cleaned up before showing choices"), PC->LevelUpEffect.Get());
	TestNotNull(TEXT("Upgrade menu opens after presentation"), PC->LevelUpWidget.Get());
	TestTrue(TEXT("Gameplay freezes when upgrade choices open"), PC->bLevelUpTimeDilationApplied);
	TestTrue(TEXT("World freezes only once choices open"), World->GetWorldSettings()->TimeDilation <= 0.001f);
	TestTrue(TEXT("Upgrade menu reserves selection input"), PC->IsSelectingUpgrade());
	TestFalse(TEXT("Dash is blocked while choosing an upgrade"), PC->CanDash());
	PC->HandleLevelUpSelectionCompleted();
	TestEqual(TEXT("First choice consumes only one queued level"), PC->PendingLevelUpChoices, 1);
	TestEqual(TEXT("Shrine reward waits for all queued levels"), PC->PendingBloodShrineRewards, 1);
	TestFalse(TEXT("Consecutive choices do not replay presentation"), PC->bLevelUpPresentationActive);
	PC->HandleLevelUpSelectionCompleted();
	TestEqual(TEXT("Both level-up choices finish"), PC->PendingLevelUpChoices, 0);
	// The saved pool may have no eligible Blood Pacts, in which case the existing
	// reward flow skips it automatically instead of leaving the player frozen.
	if (PC->bCurrentSelectionIsBloodShrineReward) PC->HandleLevelUpSelectionCompleted();
	TestEqual(TEXT("Queued shrine reward is resolved after level-ups"), PC->PendingBloodShrineRewards, 0);
	TestFalse(TEXT("Final choice releases input"), PC->IsSelectingUpgrade());
	TestEqual(TEXT("Original world speed is restored"), World->GetWorldSettings()->TimeDilation, 0.7f);

	auto* SavedVFX = PC->LevelUpVFX.Get();
	PC->LevelUpVFX = nullptr;
	PC->HandlePlayerLevelUp(4);
	PC->UpdateLevelUpPresentation(1.01f);
	TestFalse(TEXT("Missing VFX does not stall choices"), PC->bLevelUpPresentationActive);
	TestTrue(TEXT("Missing VFX still opens choices"), PC->IsSelectingUpgrade());
	PC->HandleLevelUpSelectionCompleted();
	PC->LevelUpPresentationDuration = 0.0f;
	PC->HandlePlayerLevelUp(5);
	TestFalse(TEXT("Zero delay opens choices immediately"), PC->bLevelUpPresentationActive);
	PC->HandleLevelUpSelectionCompleted();
	PC->LevelUpPresentationDuration = 1.0f;
	PC->LevelUpVFX = SavedVFX;
	for (ERunEndState EndState : {ERunEndState::Defeat, ERunEndState::Victory})
	{
		PC->RunEndState = ERunEndState::Playing;
		PC->HandlePlayerLevelUp(6);
		PC->RunEndState = EndState;
		PC->StopRunGameplay(false);
		PC->UpdateLevelUpPresentation(2.0f);
		TestFalse(TEXT("Run end cancels presentation"), PC->bLevelUpPresentationActive);
		TestEqual(TEXT("Run end clears queued levels"), PC->PendingLevelUpChoices, 0);
		TestFalse(TEXT("Run end cannot reopen reward choices"), PC->IsSelectingUpgrade());
		TestNull(TEXT("Run end stops the presentation sound"), PC->LevelUpAudio.Get());
		TestNull(TEXT("Run end destroys the active burst"), PC->LevelUpEffect.Get());
	}
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}
#endif
