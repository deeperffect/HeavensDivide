// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemyStatusEffectComponent.h"

#include "CharacterManagerComponent.h"
#include "CharacterStatsComponent.h"
#include "EnemyBase.h"
#include "Engine/OverlapResult.h"
#include "HealthComponent.h"
#include "NinjaCharacter.h"
#include "PlayerUpgradeComponent.h"
#include "SurvivorPlayerController.h"
#include "TimerManager.h"
#include "UpgradeDefinition.h"

namespace StatusUpgradeIds
{
	static const FName BleedingEdge(TEXT("BleedingEdge"));
	static const FName DeepCuts(TEXT("DeepCuts"));
	static const FName VenomousKunai(TEXT("VenomousKunai"));
	static const FName PotentVenom(TEXT("PotentVenom"));
	static const FName VirulentStrain(TEXT("VirulentStrain"));
	static const FName AcceleratedVenom(TEXT("AcceleratedVenom"));
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
	EnsureUpgradeListener(SourceUpgrades);
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
		State.ActiveTickInterval = GetEffectiveTickInterval(Status, State);
		GetWorld()->GetTimerManager().SetTimer(State.TickTimer, TickDelegate, State.ActiveTickInterval, true);
	}
	if (State.Stacks != PreviousStacks) OnStatusStacksChanged.Broadcast(Status, State.Stacks);
	if (Status == EEnemyStatusEffect::Bleed) RefreshPoisonTickRate();
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
float UEnemyStatusEffectComponent::GetEffectiveTickInterval(EEnemyStatusEffect Status, const FEnemyDamageStatusState& State) const
{
	const float BaseInterval = GetTickInterval(Status);
	if (Status != EEnemyStatusEffect::Poison || !HasStatus(EEnemyStatusEffect::Bleed)) return BaseInterval;
	const UPlayerUpgradeComponent* Upgrades = State.SourceUpgrades.Get();
	const UUpgradeDefinition* Upgrade = Upgrades ? Upgrades->GetAcquiredUpgradeWithSpecialEffect(EUpgradeSpecialEffect::AcceleratedVenom) : nullptr;
	return Upgrade ? BaseInterval / FMath::Max(1.0f, Upgrade->AcceleratedVenomTickRateMultiplier) : BaseInterval;
}
float UEnemyStatusEffectComponent::GetDuration(EEnemyStatusEffect Status) const { return Status == EEnemyStatusEffect::Bleed ? BleedDuration : PoisonDuration; }

void UEnemyStatusEffectComponent::RefreshPoisonTickRate()
{
	if (!GetWorld() || PoisonState.Stacks <= 0 || !GetWorld()->GetTimerManager().IsTimerActive(PoisonState.TickTimer)) return;
	const float NewInterval = GetEffectiveTickInterval(EEnemyStatusEffect::Poison, PoisonState);
	const float OldInterval = PoisonState.ActiveTickInterval > KINDA_SMALL_NUMBER ? PoisonState.ActiveTickInterval : PoisonTickInterval;
	if (FMath::IsNearlyEqual(NewInterval, OldInterval)) return;
	const float OldRemaining = GetWorld()->GetTimerManager().GetTimerRemaining(PoisonState.TickTimer);
	const float NormalizedPhaseRemaining = FMath::Clamp(OldRemaining / OldInterval, 0.0f, 1.0f);
	FTimerDelegate TickDelegate;
	TickDelegate.BindUObject(this, &UEnemyStatusEffectComponent::TickPoison);
	PoisonState.ActiveTickInterval = NewInterval;
	GetWorld()->GetTimerManager().SetTimer(PoisonState.TickTimer, TickDelegate, NewInterval, true,
		FMath::Max(KINDA_SMALL_NUMBER, NormalizedPhaseRemaining * NewInterval));
}

void UEnemyStatusEffectComponent::EnsureUpgradeListener(UPlayerUpgradeComponent* Upgrades)
{
	if (Upgrades) Upgrades->OnUpgradeAcquired.AddUniqueDynamic(this, &UEnemyStatusEffectComponent::HandleSourceUpgradeAcquired);
}

void UEnemyStatusEffectComponent::HandleSourceUpgradeAcquired(UUpgradeDefinition* Upgrade, int32 NewLevel)
{
	if (Upgrade && Upgrade->SpecialEffects.Contains(EUpgradeSpecialEffect::AcceleratedVenom)) RefreshPoisonTickRate();
}

float UEnemyStatusEffectComponent::CalculateStatusDamagePerTick(EEnemyStatusEffect Status, const FEnemyDamageStatusState& State) const
{
	const UPlayerUpgradeComponent* Upgrades = State.SourceUpgrades.Get();
	if (!Upgrades || State.Stacks <= 0) return 0.0f;
	const float Power = Status == EEnemyStatusEffect::Bleed ? Upgrades->GetSamuraiPowerMultiplier() : Upgrades->GetNinjaPowerMultiplier();
	const FName SupportId = Status == EEnemyStatusEffect::Bleed ? StatusUpgradeIds::DeepCuts : StatusUpgradeIds::PotentVenom;
	const int32 SupportLevel = Upgrades->GetUpgradeLevelById(SupportId);
	const float LegacyPerLevel = Status == EEnemyStatusEffect::Bleed ? DeepCutsDamagePerLevel : PotentVenomDamagePerLevel;
	const float StoredMagnitude = Upgrades->GetAccumulatedUpgradeMagnitude(SupportId);
	const float SupportMagnitude = StoredMagnitude > 0.0f ? StoredMagnitude : SupportLevel * FMath::Max(0.0f, LegacyPerLevel);
	const float BaseTick = Status == EEnemyStatusEffect::Bleed ? BaseBleedDamagePerTick : BasePoisonDamagePerTick;
	return BaseTick * FMath::Max(0.0f, Power) * (1.0f + SupportMagnitude) * State.Stacks;
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
	const int32 StacksAtTick = State.Stacks;
	const bool bApplied = Enemy->ApplyPlayerDamage(Damage, Source);
	if (bApplied && Enemy->IsDead() && Status == EEnemyStatusEffect::Poison) GrantPoisonKillHealing(Upgrades);
	if (bApplied && !Enemy->IsDead() && Status == EEnemyStatusEffect::Poison)
	{
		TryTriggerVirulentStrain(Enemy, Upgrades, StacksAtTick, Damage);
	}

	State.RemainingDuration -= State.ActiveTickInterval > KINDA_SMALL_NUMBER ? State.ActiveTickInterval : GetTickInterval(Status);
	if (Enemy->IsDead() || State.RemainingDuration <= KINDA_SMALL_NUMBER) ClearStatus(Status);
}

void UEnemyStatusEffectComponent::ClearStatus(EEnemyStatusEffect Status)
{
	FEnemyDamageStatusState& State = GetState(Status);
	const bool bWasActive = State.Stacks > 0;
	UPlayerUpgradeComponent* SourceUpgrades = State.SourceUpgrades.Get();
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(State.TickTimer);
	State.Stacks = 0;
	State.RemainingDuration = 0.0f;
	State.ActiveTickInterval = 0.0f;
	State.SourceUpgrades.Reset();
	if (bWasActive) OnStatusStacksChanged.Broadcast(Status, 0);
	if (Status == EEnemyStatusEffect::Bleed) RefreshPoisonTickRate();
	if (BleedState.Stacks <= 0 && PoisonState.Stacks <= 0 && SourceUpgrades)
	{
		SourceUpgrades->OnUpgradeAcquired.RemoveDynamic(this, &UEnemyStatusEffectComponent::HandleSourceUpgradeAcquired);
	}
}

void UEnemyStatusEffectComponent::TryTriggerVirulentStrain(AEnemyBase* SourceEnemy, UPlayerUpgradeComponent* Upgrades, int32 PoisonStacks, float ResolvedPoisonTickDamage)
{
	const UUpgradeDefinition* Upgrade = Upgrades ? Upgrades->GetAcquiredUpgradeWithSpecialEffect(EUpgradeSpecialEffect::VirulentStrain) : nullptr;
	if (!Upgrade || !SourceEnemy || PoisonStacks < FMath::Max(1, Upgrade->VirulentStrainThreshold) || !GetWorld()) return;
	const float Radius = FMath::Max(0.0f, Upgrade->VirulentStrainRadius);
	const float PulseDamage = ResolvedPoisonTickDamage * FMath::Max(0.0f, Upgrade->VirulentStrainDamageMultiplier);
	if (Radius <= 0.0f || PulseDamage <= 0.0f) return;
	FCollisionObjectQueryParams ObjectTypes;
	ObjectTypes.AddObjectTypesToQuery(ECC_Pawn);
	ObjectTypes.AddObjectTypesToQuery(ECC_GameTraceChannel1);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(VirulentStrain), false, SourceEnemy);
	Params.AddIgnoredActor(SourceEnemy);
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, SourceEnemy->GetActorLocation(), FQuat::Identity, ObjectTypes, FCollisionShape::MakeSphere(Radius), Params);
	TSet<AEnemyBase*> Damaged;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AEnemyBase* Target = Cast<AEnemyBase>(Overlap.GetActor());
		if (!Target || Target == SourceEnemy || Target->IsDead() || Damaged.Contains(Target)) continue;
		Damaged.Add(Target);
		Target->ApplyPlayerDamage(PulseDamage, EPlayerAttackSource::Ninja);
	}
	OnVirulentStrainPulse.Broadcast(SourceEnemy, SourceEnemy->GetActorLocation(), Radius, PulseDamage, PoisonStacks);
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
