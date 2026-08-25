// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemyStatusEffectComponent.h"

#include "CharacterManagerComponent.h"
#include "CharacterStatsComponent.h"
#include "EnemyBase.h"
#include "HealthComponent.h"
#include "NinjaCharacter.h"
#include "PlayerUpgradeComponent.h"
#include "SurvivorPlayerController.h"
#include "TimerManager.h"

namespace StatusUpgradeIds
{
	static const FName BleedingEdge(TEXT("BleedingEdge"));
	static const FName DeepCuts(TEXT("DeepCuts"));
	static const FName VenomousKunai(TEXT("VenomousKunai"));
	static const FName PotentVenom(TEXT("PotentVenom"));
}

UEnemyStatusEffectComponent::UEnemyStatusEffectComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UEnemyStatusEffectComponent::ApplyStatus(EEnemyStatusEffect Status, UPlayerUpgradeComponent* SourceUpgrades, EPlayerAttackSource Source)
{
	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy || Enemy->IsDead() || !SourceUpgrades || !Enemy->CanReceivePlayerDamage(Source)) return false;
	const bool bCorrectSource = (Status == EEnemyStatusEffect::Bleed && Source == EPlayerAttackSource::Samurai)
		|| (Status == EEnemyStatusEffect::Poison && Source == EPlayerAttackSource::Ninja);
	const FName StarterId = Status == EEnemyStatusEffect::Bleed ? StatusUpgradeIds::BleedingEdge : StatusUpgradeIds::VenomousKunai;
	if (!bCorrectSource || !SourceUpgrades->HasUpgradeId(StarterId)) return false;

	FEnemyDamageStatusState& State = GetState(Status);
	const int32 PreviousStacks = State.Stacks;
	State.SourceUpgrades = SourceUpgrades;
	// Statuses have no gameplay stack cap. Saturate only at int32's technical limit
	// so malformed input can never wrap the authoritative count negative.
	if (State.Stacks < MAX_int32) ++State.Stacks;
	State.RemainingDuration = GetDuration(Status);

	if (!GetWorld()->GetTimerManager().IsTimerActive(State.TickTimer))
	{
		FTimerDelegate TickDelegate;
		TickDelegate.BindUObject(this, Status == EEnemyStatusEffect::Bleed
			? &UEnemyStatusEffectComponent::TickBleed
			: &UEnemyStatusEffectComponent::TickPoison);
		GetWorld()->GetTimerManager().SetTimer(State.TickTimer, TickDelegate, GetTickInterval(Status), true);
	}
	if (State.Stacks != PreviousStacks) OnStatusStacksChanged.Broadcast(Status, State.Stacks);
	return true;
}

bool UEnemyStatusEffectComponent::HasStatus(EEnemyStatusEffect Status) const { return GetState(Status).Stacks > 0; }
int32 UEnemyStatusEffectComponent::GetStatusStacks(EEnemyStatusEffect Status) const { return GetState(Status).Stacks; }

float UEnemyStatusEffectComponent::CalculateRemainingStatusDamage(EEnemyStatusEffect Status) const
{
	const FEnemyDamageStatusState& State = GetState(Status);
	return CalculateStatusDamagePerTick(Status, State) * CalculateRemainingTickCount(Status, State);
}

bool UEnemyStatusEffectComponent::ConsumeStatus(EEnemyStatusEffect Status)
{
	if (!HasStatus(Status)) return false;
	ClearStatus(Status);
	return true;
}

void UEnemyStatusEffectComponent::ClearAllStatuses()
{
	ClearStatus(EEnemyStatusEffect::Bleed);
	ClearStatus(EEnemyStatusEffect::Poison);
}

void UEnemyStatusEffectComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearAllStatuses();
	Super::EndPlay(EndPlayReason);
}

FEnemyDamageStatusState& UEnemyStatusEffectComponent::GetState(EEnemyStatusEffect Status) { return Status == EEnemyStatusEffect::Bleed ? BleedState : PoisonState; }
const FEnemyDamageStatusState& UEnemyStatusEffectComponent::GetState(EEnemyStatusEffect Status) const { return Status == EEnemyStatusEffect::Bleed ? BleedState : PoisonState; }
float UEnemyStatusEffectComponent::GetTickInterval(EEnemyStatusEffect Status) const { return Status == EEnemyStatusEffect::Bleed ? BleedTickInterval : PoisonTickInterval; }
float UEnemyStatusEffectComponent::GetDuration(EEnemyStatusEffect Status) const { return Status == EEnemyStatusEffect::Bleed ? BleedDuration : PoisonDuration; }

float UEnemyStatusEffectComponent::CalculateStatusDamagePerTick(EEnemyStatusEffect Status, const FEnemyDamageStatusState& State) const
{
	const UPlayerUpgradeComponent* Upgrades = State.SourceUpgrades.Get();
	if (!Upgrades || State.Stacks <= 0) return 0.0f;
	const float Power = Status == EEnemyStatusEffect::Bleed ? Upgrades->GetSamuraiPowerMultiplier() : Upgrades->GetNinjaPowerMultiplier();
	const int32 SupportLevel = Upgrades->GetUpgradeLevelById(Status == EEnemyStatusEffect::Bleed ? StatusUpgradeIds::DeepCuts : StatusUpgradeIds::PotentVenom);
	const float PerLevel = Status == EEnemyStatusEffect::Bleed ? DeepCutsDamagePerLevel : PotentVenomDamagePerLevel;
	const float BaseTick = Status == EEnemyStatusEffect::Bleed ? BaseBleedDamagePerTick : BasePoisonDamagePerTick;
	return BaseTick * FMath::Max(0.0f, Power) * (1.0f + SupportLevel * FMath::Max(0.0f, PerLevel)) * State.Stacks;
}

int32 UEnemyStatusEffectComponent::CalculateRemainingTickCount(EEnemyStatusEffect Status, const FEnemyDamageStatusState& State) const
{
	if (!GetWorld() || State.Stacks <= 0 || State.RemainingDuration <= KINDA_SMALL_NUMBER) return 0;
	const float Interval = GetTickInterval(Status);
	if (Interval <= KINDA_SMALL_NUMBER || !GetWorld()->GetTimerManager().IsTimerActive(State.TickTimer)) return 0;
	// RemainingDuration is the component's authoritative remaining tick budget:
	// it is reduced by one interval after every real timer fire and refreshed as
	// a whole budget on application. Ceil therefore matches the exact number of
	// future callback ticks, including non-integral tuning values.
	return FMath::Max(0, FMath::CeilToInt((State.RemainingDuration - KINDA_SMALL_NUMBER) / Interval));
}

void UEnemyStatusEffectComponent::TickBleed() { TickStatus(EEnemyStatusEffect::Bleed); }
void UEnemyStatusEffectComponent::TickPoison() { TickStatus(EEnemyStatusEffect::Poison); }

void UEnemyStatusEffectComponent::TickStatus(EEnemyStatusEffect Status)
{
	FEnemyDamageStatusState& State = GetState(Status);
	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	UPlayerUpgradeComponent* Upgrades = State.SourceUpgrades.Get();
	const EPlayerAttackSource Source = Status == EEnemyStatusEffect::Bleed ? EPlayerAttackSource::Samurai : EPlayerAttackSource::Ninja;
	if (!Enemy || Enemy->IsDead() || State.Stacks <= 0 || !Upgrades || !Enemy->CanReceivePlayerDamage(Source))
	{
		ClearStatus(Status);
		return;
	}

	const float Damage = CalculateStatusDamagePerTick(Status, State);
	const bool bApplied = Enemy->ApplyPlayerDamage(Damage, Source);
	if (bApplied && Enemy->IsDead() && Status == EEnemyStatusEffect::Poison) GrantPoisonKillHealing(Upgrades);

	State.RemainingDuration -= GetTickInterval(Status);
	if (Enemy->IsDead() || State.RemainingDuration <= KINDA_SMALL_NUMBER) ClearStatus(Status);
}

void UEnemyStatusEffectComponent::ClearStatus(EEnemyStatusEffect Status)
{
	FEnemyDamageStatusState& State = GetState(Status);
	const bool bWasActive = State.Stacks > 0;
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(State.TickTimer);
	State.Stacks = 0;
	State.RemainingDuration = 0.0f;
	State.SourceUpgrades.Reset();
	if (bWasActive) OnStatusStacksChanged.Broadcast(Status, 0);
}

void UEnemyStatusEffectComponent::GrantPoisonKillHealing(const UPlayerUpgradeComponent* Upgrades) const
{
	const ASurvivorPlayerController* Controller = Upgrades ? Cast<ASurvivorPlayerController>(Upgrades->GetOwner()) : nullptr;
	const UCharacterManagerComponent* Manager = Controller ? Controller->GetCharacterManager() : nullptr;
	const ANinjaCharacter* Ninja = Manager ? Manager->GetNinja() : nullptr;
	const UCharacterStatsComponent* Stats = Ninja ? Ninja->GetCharacterStats() : nullptr;
	const float Healing = Stats ? Stats->GetFinalHealthOnKill() : 0.0f;
	if (Healing > 0.0f && Controller && Controller->GetPlayerHealthComponent()) Controller->GetPlayerHealthComponent()->Heal(Healing);
}
