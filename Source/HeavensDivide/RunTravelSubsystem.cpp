#include "RunTravelSubsystem.h"
#include "RunChallengeSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "Misc/PackageName.h"

namespace
{
struct FRunDestination { const TCHAR* Id; const TCHAR* Level; };
const FRunDestination Destinations[] = {
    {TEXT("FallenGrove"), TEXT("/Game/Maps/Lvl_OldVillage")},
    {TEXT("MoonlitForest"), TEXT("/Game/Maps/Lvl_MoonlitForest")},
    {TEXT("HollowMarsh"), TEXT("/Game/Maps/Lvl_SunkenSanctuary")},
    {TEXT("CinderCitadel"), TEXT("/Game/Maps/Lvl_EmberQuarry")}
};
}

FName URunTravelSubsystem::GetStartingLevel(const UObject* Context)
{
    const auto* GI = UGameplayStatics::GetGameInstance(Context);
    const auto* Challenges = GI ? GI->GetSubsystem<URunChallengeSubsystem>() : nullptr;
    const FName Id = Challenges ? Challenges->GetMapId() : FName(TEXT("FallenGrove"));
    for (const auto& Destination : Destinations)
        if (Id == Destination.Id) return FName(Destination.Level);
    return FName(StartingLevel);
}

FName URunTravelSubsystem::GetMapIdForLevel(const FString& LevelName)
{
    for (const auto& Destination : Destinations)
        if (LevelName == FPackageName::GetShortName(Destination.Level)) return FName(Destination.Id);
    return NAME_None;
}

#include "CharacterManagerComponent.h"
#include "ComboAbilityComponent.h"
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
	if (auto* Combo = Controller->FindComponentByClass<UComboAbilityComponent>()) Snapshot.Combo = Combo->GetCombo();
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
	if (auto* Combo = Controller->FindComponentByClass<UComboAbilityComponent>()) Combo->RestoreCombo(Snapshot.Combo);
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
