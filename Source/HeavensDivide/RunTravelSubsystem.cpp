#include "RunTravelSubsystem.h"

#include "CharacterManagerComponent.h"
#include "ExperienceComponent.h"
#include "HealthComponent.h"
#include "NinjaCharacter.h"
#include "SamuraiCharacter.h"
#include "SurvivorPlayerController.h"

bool URunTravelSubsystem::CaptureFromController(ASurvivorPlayerController* Controller)
{
	if (!IsValid(Controller) || Controller->IsPlayerDead()) return false;
	Snapshot = FRunTravelSnapshot();
	Snapshot.RunTimeSeconds = Controller->GetRunTimeSeconds();
	if (UHealthComponent* Health = Controller->GetPlayerHealthComponent())
	{
		Snapshot.CurrentHealth = Health->GetCurrentHealth();
		Snapshot.ExpectedMaxHealth = Health->GetMaxHealth();
	}
	if (UExperienceComponent* XP = Controller->GetExperienceComponent())
	{
		Snapshot.CurrentXP = XP->GetCurrentXP();
		Snapshot.CurrentLevel = XP->GetCurrentLevel();
	}
	Snapshot.CurrentDashCharges = Controller->GetCurrentDashCharges();
	Snapshot.ExpectedMaxDashCharges = Controller->GetMaxDashCharges();
	if (UCharacterManagerComponent* Party = Controller->GetCharacterManager())
	{
		Snapshot.bNinjaActive = Party->GetActiveCharacter() == Party->GetNinja();
		if (Party->GetSamurai()) if (UAutoAttackComponent* Attack = Party->GetSamurai()->FindComponentByClass<UAutoAttackComponent>()) Attack->CaptureRunState(Snapshot.SamuraiAttack);
		if (Party->GetNinja()) if (UAutoAttackComponent* Attack = Party->GetNinja()->FindComponentByClass<UAutoAttackComponent>()) Attack->CaptureRunState(Snapshot.NinjaAttack);
	}
	if (UPlayerUpgradeComponent* Upgrades = Controller->GetPlayerUpgrades()) Upgrades->CaptureRunState(Snapshot.Upgrades);
	Snapshot.bValid = true;
	bBossEntryArrival = true;
	UE_LOG(LogTemp, Log, TEXT("[BossGate] Captured run: Time=%.1f HP=%.1f/%.1f Level=%d XP=%d Upgrades=%d Dash=%d/%d Active=%s"), Snapshot.RunTimeSeconds, Snapshot.CurrentHealth, Snapshot.ExpectedMaxHealth, Snapshot.CurrentLevel, Snapshot.CurrentXP, Snapshot.Upgrades.Levels.Num(), Snapshot.CurrentDashCharges, Snapshot.ExpectedMaxDashCharges, Snapshot.bNinjaActive ? TEXT("Ninja") : TEXT("Samurai"));
	return true;
}

bool URunTravelSubsystem::RestoreToController(ASurvivorPlayerController* Controller)
{
	if (!Snapshot.bValid || !IsValid(Controller)) return false;
	if (UPlayerUpgradeComponent* Upgrades = Controller->GetPlayerUpgrades()) Upgrades->RestoreRunState(Snapshot.Upgrades);
	Controller->RefreshRunTravelDerivedStats();
	if (UExperienceComponent* XP = Controller->GetExperienceComponent()) XP->RestoreRunState(Snapshot.CurrentLevel, Snapshot.CurrentXP);
	Snapshot.bNinjaActive ? Controller->ForceNinjaActive() : Controller->ForceSamuraiActive();
	if (UCharacterManagerComponent* Party = Controller->GetCharacterManager())
	{
		if (Party->GetSamurai()) if (UAutoAttackComponent* Attack = Party->GetSamurai()->FindComponentByClass<UAutoAttackComponent>()) Attack->RestoreRunState(Snapshot.SamuraiAttack);
		if (Party->GetNinja()) if (UAutoAttackComponent* Attack = Party->GetNinja()->FindComponentByClass<UAutoAttackComponent>()) Attack->RestoreRunState(Snapshot.NinjaAttack);
	}
	Controller->RestoreRunTravelDashCharges(Snapshot.CurrentDashCharges);
	if (UHealthComponent* Health = Controller->GetPlayerHealthComponent()) Health->RestoreCurrentHealth(Snapshot.CurrentHealth);
	UE_LOG(LogTemp, Log, TEXT("[BossGate] Restored run: Time=%.1f HP=%.1f/%.1f Level=%d XP=%d Upgrades=%d Dash=%d/%d Active=%s"), Snapshot.RunTimeSeconds, Controller->GetPlayerHealthComponent() ? Controller->GetPlayerHealthComponent()->GetCurrentHealth() : 0.0f, Controller->GetPlayerHealthComponent() ? Controller->GetPlayerHealthComponent()->GetMaxHealth() : 0.0f, Snapshot.CurrentLevel, Snapshot.CurrentXP, Snapshot.Upgrades.Levels.Num(), Controller->GetCurrentDashCharges(), Controller->GetMaxDashCharges(), Snapshot.bNinjaActive ? TEXT("Ninja") : TEXT("Samurai"));
	Snapshot.bValid = false;
	return true;
}

void URunTravelSubsystem::ClearSnapshot()
{
	Snapshot = FRunTravelSnapshot();
	bBossEntryArrival = false;
}

bool URunTravelSubsystem::ConsumeBossEntryArrival()
{
	const bool bWasArrival = bBossEntryArrival;
	bBossEntryArrival = false;
	return bWasArrival;
}
