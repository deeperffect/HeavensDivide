// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemyStatusEffectComponent.h"

#include "EnemyBase.h"
#include "SurvivorAbilityComponent.h"
#include "PlayerUpgradeComponent.h"
#include "TimerManager.h"
#include "UpgradeDefinition.h"

namespace StatusUpgradeIds
{
	static const FName VenomousKunai(TEXT("VenomousKunai"));
}

UEnemyStatusEffectComponent::UEnemyStatusEffectComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UEnemyStatusEffectComponent::ApplyStatus(EEnemyStatusEffect Status, UPlayerUpgradeComponent* SourceUpgrades, EPlayerAttackSource Source, bool bIntrinsicStatus, float ApplyingHitDamage)
{
	AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner());
	if (!Enemy || Enemy->IsDead() || !SourceUpgrades || !Enemy->CanReceivePlayerDamage(Source)) return false;
	const bool bCorrectSource = (Status == EEnemyStatusEffect::Bleed && Source == EPlayerAttackSource::Samurai)
		|| (Status == EEnemyStatusEffect::Poison && Source == EPlayerAttackSource::Ninja);
	const FName StarterId = Status == EEnemyStatusEffect::Bleed ? FName(TEXT("BattleStance")) : StatusUpgradeIds::VenomousKunai;
	if (!bCorrectSource || ((Status == EEnemyStatusEffect::Bleed || !bIntrinsicStatus) && !SourceUpgrades->HasUpgradeId(StarterId))) return false;

	FEnemyDamageStatusState& State = GetState(Status);
	const int32 PreviousStacks = State.Stacks;
	if(PreviousStacks==0)
		if(auto* FX=SourceUpgrades->GetOwner()->FindComponentByClass<USurvivorAbilityComponent>())
			FX->UpgradeAccent(StarterId,Enemy->GetActorLocation(),45.f,Status==EEnemyStatusEffect::Bleed?FLinearColor(1,.05f,.08f):FLinearColor(.2f,1,.05f));
	State.SourceUpgrades = SourceUpgrades;
    const bool bBleed = Status == EEnemyStatusEffect::Bleed;
    const int32 Cap = bBleed ? 5 + FMath::Clamp(SourceUpgrades->GetUpgradeLevelById(TEXT("BloodCapacity")), 0, 5) : MAX_int32;
    const int32 AddedStacks = bBleed && !bIntrinsicStatus ? 1 + FMath::Clamp(SourceUpgrades->GetUpgradeLevelById(TEXT("Bloodletting")), 0, 5) : 1;
    const int32 AcceptedStacks = FMath::Clamp(AddedStacks, 0, FMath::Max(0, Cap - State.Stacks));
    State.Stacks += AcceptedStacks;
    if (bBleed)
    {
        const auto* Stance = SourceUpgrades->FindUpgradeDefinition(TEXT("BattleStance"));
        const float Fraction = Stance ? Stance->GetBalanceValue(TEXT("BleedHitFraction"), .125f) : .125f;
        // 12.5% over six base ticks. Duration upgrades add ticks at this same rate.
        State.BleedHitBonusPerTick += FMath::Max(0.f, ApplyingHitDamage) * FMath::Max(0.f, Fraction) / 6.f * AcceptedStacks;
    }
    State.RemainingDuration = bBleed ? 3.f + FMath::Clamp(SourceUpgrades->GetUpgradeLevelById(TEXT("LingeringWounds")), 0, 3) : PoisonDuration;

	if (!GetWorld()->GetTimerManager().IsTimerActive(State.TickTimer))
	{
		FTimerDelegate TickDelegate;
		TickDelegate.BindUObject(this, Status == EEnemyStatusEffect::Bleed
			? &UEnemyStatusEffectComponent::TickBleed
			: &UEnemyStatusEffectComponent::TickPoison);
		State.ActiveTickInterval = GetEffectiveTickInterval(Status, State);
		GetWorld()->GetTimerManager().SetTimer(State.TickTimer, TickDelegate, State.ActiveTickInterval, true);
	}
	if (State.Stacks != PreviousStacks) OnStatusStacksChanged.Broadcast(Status, State.Stacks);
	if (Status == EEnemyStatusEffect::Bleed) TryBloodDetonation();
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
float UEnemyStatusEffectComponent::GetTickInterval(EEnemyStatusEffect Status) const { return Status == EEnemyStatusEffect::Bleed ? .5f : PoisonTickInterval; }
float UEnemyStatusEffectComponent::GetEffectiveTickInterval(EEnemyStatusEffect Status, const FEnemyDamageStatusState& State) const
{
	if (Status == EEnemyStatusEffect::Poison && State.bBarragePoison) return .5f;
	return GetTickInterval(Status);
}

float UEnemyStatusEffectComponent::CalculateStatusDamagePerTick(EEnemyStatusEffect Status, const FEnemyDamageStatusState& State) const
{
	const UPlayerUpgradeComponent* Upgrades = State.SourceUpgrades.Get();
	if (!Upgrades || State.Stacks <= 0) return 0.0f;
	if (Status == EEnemyStatusEffect::Poison && State.bBarragePoison) return State.PoisonDamagePerTick;
	if (Status == EEnemyStatusEffect::Bleed)
	{
		return Upgrades->HasUpgradeId(TEXT("BattleStance")) ? State.BleedHitBonusPerTick
			* (Upgrades->HasUpgradeId(TEXT("BloodPactPower")) ? 1.7f : 1.f)
			* (1.f + Upgrades->GetMetaSkillBonus(TEXT("Bleed"))) : 0.f;
	}
	return BasePoisonDamagePerTick * FMath::Max(0.f, Upgrades->GetNinjaPowerMultiplier()) * State.Stacks
		* (1.f + Upgrades->GetMetaSkillBonus(TEXT("Poison")));
}

int32 UEnemyStatusEffectComponent::CalculateRemainingTickCount(EEnemyStatusEffect Status, const FEnemyDamageStatusState& State) const
{
	if (!GetWorld() || State.Stacks <= 0 || State.RemainingDuration <= KINDA_SMALL_NUMBER) return 0;
	const float Interval = State.ActiveTickInterval > KINDA_SMALL_NUMBER ? State.ActiveTickInterval : GetEffectiveTickInterval(Status, State);
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
    // Spend this tick before damage can invoke death and transfer the remaining budget.
    State.RemainingDuration -= State.ActiveTickInterval > KINDA_SMALL_NUMBER ? State.ActiveTickInterval : GetTickInterval(Status);
	Enemy->ApplyStatusDamage(Damage, Source);

	if (Enemy->IsDead() || State.RemainingDuration <= KINDA_SMALL_NUMBER) ClearStatus(Status);
}

void UEnemyStatusEffectComponent::ClearStatus(EEnemyStatusEffect Status)
{
	FEnemyDamageStatusState& State = GetState(Status);
	const bool bWasActive = State.Stacks > 0;
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(State.TickTimer);
	State.Stacks = 0;
 State.bBarragePoison = State.bDirectBarragePoison = false; State.PoisonDamagePerTick = 0.f;
	State.BleedHitBonusPerTick = 0.f;
	State.RemainingDuration = 0.0f;
	State.ActiveTickInterval = 0.0f;
	State.SourceUpgrades.Reset();
	if (bWasActive) OnStatusStacksChanged.Broadcast(Status, 0);
}
