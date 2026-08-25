// Copyright Epic Games, Inc. All Rights Reserved.

#include "AutoAttackComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AttackProjectileBase.h"
#include "CharacterStatsComponent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "EnemyBase.h"
#include "EnemyStatusEffectComponent.h"
#include "GameFramework/Character.h"
#include "HealthComponent.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "NinjaCharacter.h"
#include "PlayerUpgradeComponent.h"
#include "SamuraiCharacter.h"
#include "SharedPlayerStatsComponent.h"
#include "SurvivorPlayerController.h"
#include "TimerManager.h"
#include "Engine/OverlapResult.h"

static TAutoConsoleVariable<int32> CVarHDLogAutoAttackCooldown(
	TEXT("hd.LogAutoAttackCooldown"),
	0,
	TEXT("Logs player auto-attack cooldown transitions when enabled."));

static TAutoConsoleVariable<int32> CVarHDLogNinjaProjectileSpread(
	TEXT("hd.LogNinjaProjectileSpread"),
	0,
	TEXT("Logs Ninja multi-projectile target distribution when enabled."));

static TAutoConsoleVariable<int32> CVarHDDebugSamuraiTargeting(
	TEXT("hd.DebugSamuraiTargeting"),
	0,
	TEXT("Logs Samurai melee cluster target scoring when enabled."));

static TAutoConsoleVariable<int32> CVarHDLogSamuraiMomentum(
	TEXT("hd.LogSamuraiMomentum"),
	0,
	TEXT("Logs Samurai Momentum per-attack kill counts and cooldown reductions when enabled."));

static TAutoConsoleVariable<int32> CVarHDLogBladeCascade(
	TEXT("hd.LogBladeCascade"),
	0,
	TEXT("Logs Blade Cascade progress, ready, and consumption events when enabled."));

namespace AutoAttackMarkedForDeathUpgradeIds
{
	static const FName MarkedBlade(TEXT("MarkedBlade"));
	static const FName BleedingEdge(TEXT("BleedingEdge"));
}

static const UPlayerUpgradeComponent* GetPlayerUpgradesForAutoAttackMarkedForDeath(const UObject* WorldContextObject, const AActor* PlayerCharacter)
{
	const ASurvivorPlayerController* SurvivorController = Cast<ASurvivorPlayerController>(PlayerCharacter ? PlayerCharacter->GetOwner() : nullptr);
	if (!SurvivorController)
	{
		SurvivorController = Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(WorldContextObject, 0));
	}

	return SurvivorController ? SurvivorController->GetPlayerUpgrades() : nullptr;
}

UAutoAttackComponent::UAutoAttackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAutoAttackComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerCharacter = Cast<ACharacterBase>(GetOwner());
	if (!OwnerCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("AutoAttackComponent requires an ACharacterBase owner."));
		return;
	}

	OwnerCharacter->OnCharacterModeChanged.AddDynamic(this, &UAutoAttackComponent::HandleOwnerCharacterModeChanged);
	if (UCharacterStatsComponent* CharacterStats = OwnerCharacter->GetCharacterStats())
	{
		CharacterStats->OnStatsChanged.AddDynamic(this, &UAutoAttackComponent::HandleCharacterStatsChanged);
	}

	ApplyLegacyTargetingRangeDefaults();

	if (CanAutoAttack())
	{
		StartAutoAttack();
	}
}

void UAutoAttackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAutoAttack();

	if (OwnerCharacter)
	{
		OwnerCharacter->OnCharacterModeChanged.RemoveDynamic(this, &UAutoAttackComponent::HandleOwnerCharacterModeChanged);
		if (UCharacterStatsComponent* CharacterStats = OwnerCharacter->GetCharacterStats())
		{
			CharacterStats->OnStatsChanged.RemoveDynamic(this, &UAutoAttackComponent::HandleCharacterStatsChanged);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void UAutoAttackComponent::StartAutoAttack()
{
	if (!CanAutoAttack())
	{
		return;
	}

	ScheduleNextAttackTimerFromCooldown();
}

void UAutoAttackComponent::StopAutoAttack()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AttackTimerHandle);
	}

	CurrentAttackTarget.Reset();
	bIsAttacking = false;
	bAttackNotifyConsumed = false;
	bActiveAttackIsAssist = false;
	bActiveAttackTriggersFanOfBlades = false;
	bDoubleCutFollowUpActive = false;
	bDoubleCutFollowUpPending = false;
	ActiveAttackMontage = nullptr;
	ActiveAttackSequence = 0;
	if (OwnerCharacter)
	{
		OwnerCharacter->ClearFacingOverride();
	}
}

void UAutoAttackComponent::SetAttackInterval(float NewInterval)
{
	AttackInterval = FMath::Max(0.01f, NewInterval);

	if (GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(AttackTimerHandle))
	{
		ScheduleNextAttackTimer(GetEffectiveAttackInterval());
	}
}

void UAutoAttackComponent::SetAutoAttackEnabled(bool bEnabled)
{
	if (bAutoAttackEnabled == bEnabled)
	{
		return;
	}

	bAutoAttackEnabled = bEnabled;
	if (bAutoAttackEnabled)
	{
		StartAutoAttack();
	}
	else
	{
		StopAutoAttack();
	}
}

bool UAutoAttackComponent::IsAutoAttackEnabled() const
{
	return bAutoAttackEnabled;
}

bool UAutoAttackComponent::ReduceRemainingAttackCooldown(float Percent)
{
	if (!GetWorld() || Percent <= 0.0f)
	{
		return false;
	}

	const double CurrentTime = GetWorld()->GetTimeSeconds();
	const double OldRemainingCooldown = FMath::Max(0.0, NextAttackReadyTime - CurrentTime);
	if (OldRemainingCooldown <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float ClampedPercent = FMath::Clamp(Percent, 0.0f, 1.0f);
	const double NewRemainingCooldown = OldRemainingCooldown * static_cast<double>(1.0f - ClampedPercent);
	NextAttackReadyTime = CurrentTime + NewRemainingCooldown;

	if (LastAttackStartTime > -DBL_MAX / 2.0)
	{
		const double EffectiveElapsed = FMath::Max(0.0, static_cast<double>(AttackIntervalAtLastAttackStart) - NewRemainingCooldown);
		LastAttackStartTime = CurrentTime - EffectiveElapsed;
	}

	if (CanAutoAttack() && GetWorld()->GetTimerManager().IsTimerActive(AttackTimerHandle))
	{
		ScheduleNextAttackTimerFromCooldown();
	}

	if (CVarHDLogSamuraiMomentum.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogTemp, Log, TEXT("Momentum triggered: remaining cooldown %.3f -> %.3f"),
			OldRemainingCooldown,
			NewRemainingCooldown);
	}

	return true;
}

void UAutoAttackComponent::PerformAttackTrace()
{
	if (!TryConsumeAttackNotify())
	{
		return;
	}

	if (bDoubleCutFollowUpActive)
	{
		if (ExecuteMeleeAttackTrace())
		{
			OnAutoAttack.Broadcast(this, EAutoAttackSource::DoubleCut);
		}
		return;
	}

	const bool bCountsForDoubleCut = !ProjectileClass && HasDoubleCutUpgrade();
	if (ExecuteMeleeAttackTrace() && bCountsForDoubleCut)
	{
		RegisterDoubleCutPrimaryAttack();
	}
}

bool UAutoAttackComponent::ExecuteMeleeAttackTrace()
{
	if (!OwnerCharacter || !GetWorld())
	{
		UE_LOG(LogTemp, Warning, TEXT("AutoAttack trace skipped: owner/world invalid."));
		return false;
	}

	if (bActiveAttackIsAssist && !ProjectileClass)
	{
		AEnemyBase* AssistTarget = CurrentAttackTarget.Get();
		if (!AssistTarget || AssistTarget->IsDead() || !IsTargetInCurrentMeleeReach(AssistTarget))
		{
		AssistTarget = FindAssistTargetNearLocation(OwnerCharacter->GetActorLocation(), GetEffectiveTargetingRange());
			CurrentAttackTarget = AssistTarget;
		}

		if (!AssistTarget || AssistTarget->IsDead() || !IsTargetInCurrentMeleeReach(AssistTarget))
		{
			return false;
		}
	}

	FVector AttackForward = OwnerCharacter->GetVisualForwardVector();
	AttackForward.Z = 0.0f;
	if (!AttackForward.Normalize())
	{
		UE_LOG(LogTemp, Warning, TEXT("AutoAttack trace skipped: visual forward invalid."));
		return false;
	}

	const FVector AttackOrigin = OwnerCharacter->GetActorLocation();
	const FVector HitboxCenter = AttackOrigin + AttackForward * AttackForwardOffset;
	const float EffectiveAttackRadius = GetEffectiveAttackRadius();
	const float EffectiveAttackDamage = GetEffectiveAttackDamage();
	const UPlayerUpgradeComponent* PlayerUpgrades = GetPlayerUpgradesForAutoAttackMarkedForDeath(this, OwnerCharacter);
	const bool bCanApplyMarkedBlade = PlayerUpgrades && PlayerUpgrades->HasUpgradeId(AutoAttackMarkedForDeathUpgradeIds::MarkedBlade);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AutoAttackTrace), false, OwnerCharacter);
	QueryParams.AddIgnoredActor(OwnerCharacter);

	TArray<FHitResult> HitResults;
	const FCollisionShape TraceShape = FCollisionShape::MakeSphere(EffectiveAttackRadius);
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_GameTraceChannel1);
	GetWorld()->SweepMultiByObjectType(
		HitResults,
		HitboxCenter,
		HitboxCenter,
		FQuat::Identity,
		ObjectQueryParams,
		TraceShape,
		QueryParams);

	TSet<AEnemyBase*> UniqueEnemies;
	TArray<AEnemyBase*> HitEnemies;
	const AActor* OwnerActor = OwnerCharacter->GetOwner();
	const EPlayerAttackSource AttackSource = AEnemyBase::ResolvePlayerAttackSource(OwnerCharacter);
	const bool bCanApplyBleed = AttackSource == EPlayerAttackSource::Samurai && PlayerUpgrades && PlayerUpgrades->HasUpgradeId(AutoAttackMarkedForDeathUpgradeIds::BleedingEdge);
	for (const FHitResult& HitResult : HitResults)
	{
		AActor* HitActor = HitResult.GetActor();
		if (!HitActor || HitActor == OwnerCharacter || HitActor->GetOwner() == OwnerActor)
		{
			continue;
		}

		AEnemyBase* HitEnemy = Cast<AEnemyBase>(HitActor);
		if (!HitEnemy || HitEnemy->IsDead() || UniqueEnemies.Contains(HitEnemy))
		{
			continue;
		}

		UHealthComponent* EnemyHealth = HitEnemy->GetHealthComponent();
		if (!EnemyHealth || EnemyHealth->IsDead())
		{
			continue;
		}
		if (!HitEnemy->CanReceivePlayerDamage(AttackSource))
		{
			continue;
		}

		UniqueEnemies.Add(HitEnemy);
		HitEnemies.Add(HitEnemy);
	}

	AEnemyBase* PrimaryTarget = nullptr;
	float BestAlignment = -FLT_MAX;
	float BestDistanceSquared = FLT_MAX;
	constexpr float AlignmentTieTolerance = 0.0001f;
	constexpr float DistanceTieToleranceSquared = 1.0f;
	for (AEnemyBase* Candidate : HitEnemies)
	{
		FVector ToCandidate = Candidate->GetActorLocation() - AttackOrigin;
		ToCandidate.Z = 0.0f;
		const float DistanceSquared = ToCandidate.SizeSquared();
		const float Alignment = ToCandidate.Normalize() ? FVector::DotProduct(AttackForward, ToCandidate) : 1.0f;
		const bool bBetterAlignment = Alignment > BestAlignment + AlignmentTieTolerance;
		const bool bAlignmentTied = FMath::Abs(Alignment - BestAlignment) <= AlignmentTieTolerance;
		const bool bNearer = DistanceSquared < BestDistanceSquared - DistanceTieToleranceSquared;
		const bool bDistanceTied = FMath::Abs(DistanceSquared - BestDistanceSquared) <= DistanceTieToleranceSquared;
		const bool bStableNameWins = bDistanceTied && PrimaryTarget
			&& Candidate->GetPathName().Compare(PrimaryTarget->GetPathName(), ESearchCase::CaseSensitive) < 0;
		if (!PrimaryTarget || bBetterAlignment || (bAlignmentTied && (bNearer || bStableNameWins)))
		{
			PrimaryTarget = Candidate;
			BestAlignment = Alignment;
			BestDistanceSquared = DistanceSquared;
		}
	}

	int32 KilledEnemyCount = 0;
	const ESamuraiTechnique ActiveTechnique = GetActiveSamuraiTechnique();
	const float ResolvedPrimaryDamage = ActiveTechnique == ESamuraiTechnique::Duelist
		? ResolveDuelistPrimaryDamage(PrimaryTarget, EffectiveAttackDamage)
		: EffectiveAttackDamage;
	const float SecondaryDamage = EffectiveAttackDamage * FMath::Clamp(SecondaryTargetDamageMultiplier, 0.0f, 1.0f);
	float PrimaryHealthBeforeHit = 0.0f;
	FVector PrimaryDeathLocation = FVector::ZeroVector;
	bool bPrimaryKilled = false;

	if (PrimaryTarget)
	{
		if (UHealthComponent* PrimaryHealth = PrimaryTarget->GetHealthComponent(); PrimaryHealth && !PrimaryHealth->IsDead())
		{
			PrimaryHealthBeforeHit = PrimaryHealth->GetCurrentHealth();
			PrimaryDeathLocation = PrimaryTarget->GetActorLocation();
			const bool bDamageApplied = PrimaryTarget->ApplyPlayerDamage(ResolvedPrimaryDamage, AttackSource);
			bPrimaryKilled = PrimaryHealth->IsDead();
			if (bPrimaryKilled) ++KilledEnemyCount;
			if (bDamageApplied && !bPrimaryKilled && bCanApplyBleed) PrimaryTarget->ApplyStatus(EEnemyStatusEffect::Bleed, const_cast<UPlayerUpgradeComponent*>(PlayerUpgrades), AttackSource);
			if (bCanApplyMarkedBlade) PrimaryTarget->ApplyMark();
		}
	}

	if (bPrimaryKilled)
	{
		if (ActiveTechnique == ESamuraiTechnique::Cleaver)
		{
			ExecuteCleaverChain(PrimaryTarget, PrimaryDeathLocation, FMath::Max(0.0f, ResolvedPrimaryDamage - PrimaryHealthBeforeHit), AttackSource);
		}
		else if (ActiveTechnique == ESamuraiTechnique::Deathblow)
		{
			ExecuteDeathblow(PrimaryTarget, PrimaryDeathLocation, ResolvedPrimaryDamage, AttackSource, bCanApplyMarkedBlade);
		}
		else if (ActiveTechnique == ESamuraiTechnique::Duelist)
		{
			if (DuelistTarget.Get() == PrimaryTarget)
			{
				ResetDuelistState();
			}
		}
	}

	for (AEnemyBase* HitEnemy : HitEnemies)
	{
		if (!HitEnemy || HitEnemy == PrimaryTarget) continue;
		UHealthComponent* EnemyHealth = HitEnemy->GetHealthComponent();
		if (!EnemyHealth || EnemyHealth->IsDead()) continue;
		const bool bDamageApplied = HitEnemy->ApplyPlayerDamage(SecondaryDamage, AttackSource);
		if (EnemyHealth->IsDead()) ++KilledEnemyCount;
		if (bDamageApplied && !EnemyHealth->IsDead() && bCanApplyBleed) HitEnemy->ApplyStatus(EEnemyStatusEffect::Bleed, const_cast<UPlayerUpgradeComponent*>(PlayerUpgrades), AttackSource);
		if (bCanApplyMarkedBlade) HitEnemy->ApplyMark();
	}

	if (HitEnemies.Num() > 0 && ImpactSound)
	{
		UGameplayStatics::PlaySound2D(GetWorld(), ImpactSound);
	}

	if (bDebugAttackTrace)
	{
		const FColor DebugColor = HitEnemies.Num() > 0 ? FColor::Red : FColor::Cyan;
		constexpr float DebugDuration = 1.5f;
		DrawDebugLine(GetWorld(), AttackOrigin, HitboxCenter, DebugColor, false, DebugDuration, 0, 4.0f);
		DrawDebugSphere(GetWorld(), HitboxCenter, EffectiveAttackRadius, 24, DebugColor, false, DebugDuration, 0, 4.0f);
	}

	HandleSamuraiMomentum(KilledEnemyCount);

	return true;
}

void UAutoAttackComponent::SpawnAutoAttackProjectile()
{
	if (!TryConsumeAttackNotify())
	{
		return;
	}

	if (!CanExecuteAttackInCurrentMode())
	{
		UE_LOG(LogTemp, Warning, TEXT("Projectile spawn skipped: auto attack cannot run."));
		return;
	}

	if (!ProjectileClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Projectile spawn skipped: ProjectileClass invalid."));
		return;
	}

	TArray<AEnemyBase*> TargetCandidates;
	FindEnemyTargetsSorted(TargetCandidates);
	if (TargetCandidates.Num() == 0)
	{
		if (OwnerCharacter)
		{
			OwnerCharacter->ClearFacingOverride();
		}
		return;
	}

	const FVector SpawnLocation = GetProjectileSpawnLocation();
	const float EffectiveProjectileSpeed = GetEffectiveProjectileSpeed();
	const float EffectiveAttackDamage = GetEffectiveAttackDamage();
	const int32 NormalProjectileCount = GetEffectiveProjectileCount();
	const int32 EffectiveProjectileCount = ConsumeBladeCascadeBonusForNormalVolley(NormalProjectileCount);
	const int32 EffectiveProjectilePierceBonus = GetEffectiveProjectilePierceBonus();

	int32 SpawnedProjectileCount = 0;
	for (int32 ProjectileIndex = 0; ProjectileIndex < EffectiveProjectileCount; ++ProjectileIndex)
	{
		AEnemyBase* AssignedTarget = TargetCandidates[ProjectileIndex % TargetCandidates.Num()];
		if (!AssignedTarget || AssignedTarget->IsDead())
		{
			continue;
		}

		FVector SpawnDirection = GetEnemyAimLocation(AssignedTarget) - SpawnLocation;
		SpawnDirection.Z = 0.0f;
		if (!SpawnDirection.Normalize())
		{
			UE_LOG(LogTemp, Warning, TEXT("Projectile %d spawn skipped: invalid projectile direction."), ProjectileIndex + 1);
			continue;
		}

		SpawnProjectileInstance(SpawnLocation, SpawnDirection, EffectiveAttackDamage, EffectiveProjectileSpeed, EffectiveProjectilePierceBonus);
		++SpawnedProjectileCount;

		if (CVarHDLogNinjaProjectileSpread.GetValueOnGameThread() != 0)
		{
			UE_LOG(LogTemp, Log, TEXT("Ninja Attack Projectile %d/%d -> %s Direction=%s"),
				ProjectileIndex + 1,
				EffectiveProjectileCount,
				*GetNameSafe(AssignedTarget),
				*SpawnDirection.ToString());
		}

		if (bDebugTargeting)
		{
			constexpr float DebugDuration = 1.5f;
			DrawDebugLine(GetWorld(), SpawnLocation, GetEnemyAimLocation(AssignedTarget), FColor::Yellow, false, DebugDuration, 0, 3.0f);
		}
	}

	if (SpawnedProjectileCount <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Projectile spawn skipped: all assigned projectile directions were invalid."));
		OwnerCharacter->ClearFacingOverride();
		return;
	}

	if (bDebugTargeting)
	{
		constexpr float DebugDuration = 1.5f;
		DrawDebugSphere(GetWorld(), SpawnLocation, 24.0f, 12, FColor::Yellow, false, DebugDuration, 0, 3.0f);
	}

	RegisterNinjaAttackForFanOfBlades(SpawnLocation, EffectiveAttackDamage, EffectiveProjectileSpeed, EffectiveProjectilePierceBonus);

	CurrentAttackTarget.Reset();
	OwnerCharacter->ClearFacingOverride();
}

void UAutoAttackComponent::SpawnProjectileInstance(const FVector& SpawnLocation, const FVector& ProjectileDirection, float Damage, float Speed, int32 AdditionalPierceCount, bool bRegisterAttackCycle)
{
	if (!OwnerCharacter || !ProjectileClass || !GetWorld())
	{
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = OwnerCharacter;
	SpawnParameters.Instigator = OwnerCharacter;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AAttackProjectileBase* Projectile = GetWorld()->SpawnActor<AAttackProjectileBase>(
		ProjectileClass,
		SpawnLocation,
		ProjectileDirection.Rotation(),
		SpawnParameters);

	if (!Projectile)
	{
		UE_LOG(LogTemp, Warning, TEXT("Projectile spawn failed."));
		return;
	}

	Projectile->InitializeProjectile(
		OwnerCharacter,
		ProjectileDirection,
		Damage,
		Speed,
		EProjectileTargetType::Enemies,
		GetEffectiveTargetingRange(),
		true,
		nullptr,
		true,
		AdditionalPierceCount,
		GetEffectiveProjectileBounceBonus(),
		GetEffectiveProjectileSplitBonus());

	if (bRegisterAttackCycle)
	{
		RegisterKunaiFired();
	}
}

bool UAutoAttackComponent::SpawnShadowCloneVolley(const FVector& SpawnLocation, float SearchRange)
{
	if (!OwnerCharacter || !OwnerCharacter->IsA<ANinjaCharacter>() || !ProjectileClass || !GetWorld() || IsOwningPlayerDead())
	{
		return false;
	}

	TArray<AEnemyBase*> Targets;
	FindEnemyTargetsSortedFromLocation(SpawnLocation, FMath::Max(0.0f, SearchRange), Targets);
	if (Targets.Num() == 0) return false;

	const int32 ProjectileCount = FMath::Max(1, GetEffectiveProjectileCount());
	const float Damage = GetEffectiveAttackDamage();
	const float Speed = GetEffectiveProjectileSpeed();
	const int32 Pierce = GetEffectiveProjectilePierceBonus();
	int32 Spawned = 0;
	for (int32 Index = 0; Index < ProjectileCount; ++Index)
	{
		AEnemyBase* Target = Targets[Index % Targets.Num()];
		if (!Target || Target->IsDead()) continue;
		FVector Direction = GetEnemyAimLocation(Target) - SpawnLocation;
		Direction.Z = 0.0f;
		if (!Direction.Normalize()) continue;
		SpawnProjectileInstance(SpawnLocation, Direction, Damage, Speed, Pierce, false);
		++Spawned;
	}
	return Spawned > 0;
}

AEnemyBase* UAutoAttackComponent::FindAssistTarget() const
{
	return FindNearestEnemyTarget();
}

AEnemyBase* UAutoAttackComponent::FindAssistTargetNearLocation(const FVector& SearchLocation, float SearchRadius) const
{
	if (!ProjectileClass)
	{
		return FindBestMeleeTarget(SearchLocation, SearchRadius);
	}

	TArray<AEnemyBase*> SortedTargets;
	FindEnemyTargetsSortedFromLocation(SearchLocation, SearchRadius, SortedTargets);
	return SortedTargets.Num() > 0 ? SortedTargets[0] : nullptr;
}

bool UAutoAttackComponent::IsProjectileAttack() const
{
	return ProjectileClass != nullptr;
}

bool UAutoAttackComponent::IsTargetInCurrentMeleeReach(const AEnemyBase* TargetEnemy) const
{
	if (!OwnerCharacter || !TargetEnemy)
	{
		return false;
	}

	FVector AttackForward = OwnerCharacter->GetVisualForwardVector();
	AttackForward.Z = 0.0f;
	if (!AttackForward.Normalize())
	{
		return false;
	}

	const FVector HitboxCenter = OwnerCharacter->GetActorLocation() + AttackForward * AttackForwardOffset;
	const float EffectiveAttackRadius = GetEffectiveAttackRadius();
	const float TargetRadius = TargetEnemy->GetCapsuleComponent() ? TargetEnemy->GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.0f;
	return FVector::Dist2D(HitboxCenter, GetEnemyAimLocation(TargetEnemy)) <= EffectiveAttackRadius + TargetRadius;
}

void UAutoAttackComponent::HandleOwnerCharacterModeChanged(ECharacterMode OldMode, ECharacterMode NewMode)
{
	if (NewMode == ECharacterMode::Active)
	{
		if (CVarHDLogAutoAttackCooldown.GetValueOnGameThread() != 0 && GetWorld())
		{
			UE_LOG(LogTemp, Log, TEXT("[%s] Activated CurrentTime=%.2f NextReadyTime=%.2f RemainingCooldown=%.2f"),
				*GetNameSafe(GetOwner()),
				GetWorld()->GetTimeSeconds(),
				NextAttackReadyTime,
				FMath::Max(0.0, NextAttackReadyTime - GetWorld()->GetTimeSeconds()));
		}
		StartAutoAttack();
		return;
	}

	if (CVarHDLogAutoAttackCooldown.GetValueOnGameThread() != 0 && GetWorld())
	{
		UE_LOG(LogTemp, Log, TEXT("[%s] Deactivated CurrentTime=%.2f NextReadyTime=%.2f"),
			*GetNameSafe(GetOwner()),
			GetWorld()->GetTimeSeconds(),
			NextAttackReadyTime);
	}
	StopAutoAttack();
}

void UAutoAttackComponent::HandleCharacterStatsChanged()
{
	if (GetWorld() && LastAttackStartTime > -DBL_MAX / 2.0)
	{
		const float NewInterval = GetEffectiveAttackInterval();
		const float OldInterval = FMath::Max(0.01f, AttackIntervalAtLastAttackStart);
		const double CurrentTime = GetWorld()->GetTimeSeconds();
		const double Elapsed = FMath::Max(0.0, CurrentTime - LastAttackStartTime);
		const double CooldownProgress = FMath::Clamp(Elapsed / static_cast<double>(OldInterval), 0.0, 1.0);
		NextAttackReadyTime = CurrentTime + static_cast<double>(NewInterval) * (1.0 - CooldownProgress);
		AttackIntervalAtLastAttackStart = NewInterval;
		LastAttackStartTime = CurrentTime - CooldownProgress * static_cast<double>(NewInterval);

		if (CVarHDLogAutoAttackCooldown.GetValueOnGameThread() != 0)
		{
			UE_LOG(LogTemp, Log, TEXT("[%s] Cooldown recalculated after stat change OldInterval=%.3f NewInterval=%.3f Progress=%.2f CurrentTime=%.2f NextReadyTime=%.2f RemainingCooldown=%.2f"),
				*GetNameSafe(GetOwner()),
				OldInterval,
				NewInterval,
				CooldownProgress,
				CurrentTime,
				NextAttackReadyTime,
				FMath::Max(0.0, NextAttackReadyTime - CurrentTime));
		}
	}

	if (CanAutoAttack() && GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(AttackTimerHandle))
	{
		ScheduleNextAttackTimerFromCooldown();
	}
}

void UAutoAttackComponent::HandleAttackTimer()
{
	if (!CanAutoAttack())
	{
		StopAutoAttack();
		return;
	}

	if (bIsAttacking)
	{
		ScheduleNextAttackTimerFromCooldown();
		return;
	}

	if (OwnerCharacter && OwnerCharacter->IsDashing())
	{
		ScheduleNextAttackTimerFromCooldown();
		return;
	}

	if (!CanStartAttackNow())
	{
		const float Delay = FMath::Max(0.01f, static_cast<float>(NextAttackReadyTime - GetWorld()->GetTimeSeconds()));
		ScheduleNextAttackTimer(Delay);
		return;
	}

	if (StartTargetedAttack())
	{
		ScheduleNextAttackTimerFromCooldown();
	}
	else
	{
		ScheduleReadyTargetCheckTimer();
	}
}

void UAutoAttackComponent::ScheduleNextAttackTimer(float Delay)
{
	if (!CanAutoAttack())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float SafeDelay = FMath::Max(0.01f, Delay);
	World->GetTimerManager().ClearTimer(AttackTimerHandle);
	World->GetTimerManager().SetTimer(
		AttackTimerHandle,
		this,
		&UAutoAttackComponent::HandleAttackTimer,
		SafeDelay,
		false);

}

void UAutoAttackComponent::ScheduleNextAttackTimerFromCooldown()
{
	if (!CanAutoAttack() || !GetWorld())
	{
		return;
	}

	const double CurrentTime = GetWorld()->GetTimeSeconds();
	const float Delay = FMath::Max(0.01f, static_cast<float>(NextAttackReadyTime - CurrentTime));
	ScheduleNextAttackTimer(Delay);

	if (CVarHDLogAutoAttackCooldown.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[%s] Cooldown schedule CurrentTime=%.2f NextReadyTime=%.2f Delay=%.3f %s"),
			*GetNameSafe(GetOwner()),
			CurrentTime,
			NextAttackReadyTime,
			Delay,
			CurrentTime >= NextAttackReadyTime ? TEXT("READY IMMEDIATELY") : TEXT(""));
	}
}

void UAutoAttackComponent::ScheduleReadyTargetCheckTimer()
{
	if (!CanAutoAttack() || !GetWorld())
	{
		return;
	}

	ScheduleNextAttackTimer(ReadyTargetCheckInterval);
}

void UAutoAttackComponent::ApplyLegacyTargetingRangeDefaults()
{
	constexpr float PreviousDefaultTargetingRange = 1500.0f;
	if (!FMath::IsNearlyEqual(TargetingRange, PreviousDefaultTargetingRange, 0.01f))
	{
		return;
	}

	TargetingRange = ProjectileClass ? LegacyRangedDefaultTargetingRange : LegacyMeleeDefaultTargetingRange;
}

bool UAutoAttackComponent::TryStartAssistAttack(AEnemyBase*& OutTargetEnemy, float& OutExpectedDuration)
{
	OutTargetEnemy = nullptr;

	AEnemyBase* TargetEnemy = FindAssistTarget();
	if (!TargetEnemy)
	{
		OutExpectedDuration = 0.0f;
		return false;
	}

	OutTargetEnemy = TargetEnemy;
	return TryStartAssistAttackAtTarget(TargetEnemy, OutExpectedDuration);
}

bool UAutoAttackComponent::TryStartAssistAttackAtTarget(AEnemyBase* TargetEnemy, float& OutExpectedDuration)
{
	OutExpectedDuration = 0.0f;

	if (!OwnerCharacter || !GetWorld())
	{
		return false;
	}

	if (OwnerCharacter->GetCharacterMode() != ECharacterMode::Assisting)
	{
		UE_LOG(LogTemp, Warning, TEXT("Assist attack skipped: %s is not in Assisting mode."), *GetNameSafe(OwnerCharacter));
		return false;
	}

	if (bIsAttacking)
	{
		return false;
	}

	if (!TargetEnemy || TargetEnemy->IsDead())
	{
		CurrentAttackTarget.Reset();
		return false;
	}

	CurrentAttackTarget = TargetEnemy;
	const FVector AimLocation = GetEnemyAimLocation(TargetEnemy);
	FVector ToTarget = AimLocation - OwnerCharacter->GetActorLocation();
	ToTarget.Z = 0.0f;
	if (ToTarget.Normalize())
	{
		OwnerCharacter->SetVisualFacingRotation(FRotator(0.0f, ToTarget.Rotation().Yaw, 0.0f));
	}

	OutExpectedDuration = GetExpectedAttackMontageDuration();
	if (!ProjectileClass && WillNextSamuraiAttackTriggerDoubleCut())
	{
		OutExpectedDuration += GetExpectedDoubleCutFollowUpDuration();
	}

	const double PreviousLastAttackStartTime = LastAttackStartTime;
	const double PreviousNextAttackReadyTime = NextAttackReadyTime;
	const float PreviousAttackIntervalAtLastAttackStart = AttackIntervalAtLastAttackStart;

	const bool bStarted = PlayAttackMontage(false);

	LastAttackStartTime = PreviousLastAttackStartTime;
	NextAttackReadyTime = PreviousNextAttackReadyTime;
	AttackIntervalAtLastAttackStart = PreviousAttackIntervalAtLastAttackStart;

	if (bStarted)
	{
		OnAutoAttack.Broadcast(this, EAutoAttackSource::Assist);
	}
	else
	{
		CurrentAttackTarget.Reset();
		OutExpectedDuration = 0.0f;
	}

	return bStarted;
}

bool UAutoAttackComponent::PlayAttackMontage(bool bUpdateNormalCooldown)
{
	if (bIsAttacking)
	{
		return false;
	}

	UAnimMontage* MontageToPlay = GetMontageForNextAttack();
	if (!MontageToPlay)
	{
		UE_LOG(LogTemp, Warning, TEXT("AttackMontage invalid"));
		return false;
	}

	ACharacter* OwnerAsCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerAsCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("AutoAttackComponent owner is not an ACharacter."));
		return false;
	}

	USkeletalMeshComponent* MeshComponent = OwnerAsCharacter->GetMesh();
	if (!MeshComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("AutoAttackComponent owner mesh invalid."));
		return false;
	}

	UAnimInstance* AnimInstance = MeshComponent->GetAnimInstance();
	if (!AnimInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("AnimInstance invalid"));
		return false;
	}

	const float ActualPlayRate = CalculateAttackMontagePlayRate(MontageToPlay);
	const float PlayResult = AnimInstance->Montage_Play(MontageToPlay, ActualPlayRate);
	if (PlayResult <= 0.0f)
	{
		return false;
	}

	FOnMontageEnded MontageEndedDelegate;
	MontageEndedDelegate.BindUObject(this, &UAutoAttackComponent::HandleAttackMontageEnded);
	AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, MontageToPlay);

	FOnMontageBlendingOutStarted MontageBlendingOutDelegate;
	MontageBlendingOutDelegate.BindUObject(this, &UAutoAttackComponent::HandleAttackMontageBlendingOut);
	AnimInstance->Montage_SetBlendingOutDelegate(MontageBlendingOutDelegate, MontageToPlay);

	bIsAttacking = true;
	bAttackNotifyConsumed = false;
	bActiveAttackIsAssist = !bUpdateNormalCooldown;
	bActiveAttackTriggersFanOfBlades = WillNextNinjaAttackTriggerFanOfBlades();
	ActiveAttackMontage = MontageToPlay;
	if (bUpdateNormalCooldown)
	{
		LastAttackStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : LastAttackStartTime;
		AttackIntervalAtLastAttackStart = GetEffectiveAttackInterval();
		NextAttackReadyTime = LastAttackStartTime + AttackIntervalAtLastAttackStart;
	}
	ActiveAttackSequence = ++AttackSequence;
	if (CVarHDLogAutoAttackCooldown.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[%s] Attack started FinalInterval=%.3f NextReadyTime=%.2f"),
			*GetNameSafe(GetOwner()),
			AttackIntervalAtLastAttackStart,
			NextAttackReadyTime);
	}
	return true;
}

UAnimMontage* UAutoAttackComponent::GetMontageForNextAttack() const
{
	if (WillNextNinjaAttackTriggerFanOfBlades() && FanOfBladesAttackMontage)
	{
		return FanOfBladesAttackMontage;
	}

	return AttackMontage;
}

float UAutoAttackComponent::CalculateAttackMontagePlayRate(const UAnimMontage* Montage) const
{
	const float SafeMaxPlayRate = FMath::Max(1.0f, MaxAttackMontagePlayRate);

	if (!bScaleMontageWithAttackInterval || !Montage)
	{
		return 1.0f;
	}

	const float MontageLength = Montage->GetPlayLength();
	const float EffectiveAttackInterval = GetEffectiveAttackInterval();
	if (MontageLength <= KINDA_SMALL_NUMBER || EffectiveAttackInterval <= KINDA_SMALL_NUMBER)
	{
		return SafeMaxPlayRate;
	}

	const float CalculatedPlayRate = MontageLength / EffectiveAttackInterval;
	return FMath::Clamp(FMath::Max(1.0f, CalculatedPlayRate), 1.0f, SafeMaxPlayRate);
}

float UAutoAttackComponent::GetExpectedAttackMontageDuration() const
{
	const UAnimMontage* ExpectedMontage = GetMontageForNextAttack();
	if (!ExpectedMontage)
	{
		return 0.75f;
	}

	const float PlayRate = FMath::Max(0.01f, CalculateAttackMontagePlayRate(ExpectedMontage));
	return ExpectedMontage->GetPlayLength() / PlayRate;
}

float UAutoAttackComponent::GetExpectedDoubleCutFollowUpDuration() const
{
	return DoubleCutMontage ? DoubleCutMontage->GetPlayLength() : 0.0f;
}

void UAutoAttackComponent::HandleAttackMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != ActiveAttackMontage)
	{
		return;
	}

	ConsumePendingDoubleCutFollowUp();
}

void UAutoAttackComponent::HandleAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != ActiveAttackMontage)
	{
		return;
	}

	if (bDoubleCutFollowUpActive)
	{
		return;
	}

	bIsAttacking = false;
	bAttackNotifyConsumed = false;
	bActiveAttackIsAssist = false;
	bActiveAttackTriggersFanOfBlades = false;
	ActiveAttackMontage = nullptr;
	CurrentAttackTarget.Reset();
	if (OwnerCharacter)
	{
		OwnerCharacter->ClearFacingOverride();
	}
	ActiveAttackSequence = 0;
}

bool UAutoAttackComponent::StartTargetedAttack()
{
	if (OwnerCharacter && OwnerCharacter->IsDashing())
	{
		return false;
	}

	if (bIsAttacking)
	{
		return false;
	}

	if (!CanStartAttackNow())
	{
		return false;
	}

	AEnemyBase* TargetEnemy = FindNearestEnemyTarget();
	if (!TargetEnemy)
	{
		CurrentAttackTarget.Reset();
		if (OwnerCharacter)
		{
			OwnerCharacter->ClearFacingOverride();
		}
		return false;
	}

	CurrentAttackTarget = TargetEnemy;
	const FVector AimLocation = GetEnemyAimLocation(TargetEnemy);
	OwnerCharacter->SetFacingOverrideTarget(AimLocation);

	if (PlayAttackMontage())
	{
		OnAutoAttack.Broadcast(this, EAutoAttackSource::NormalAutoAttack);
		return true;
	}

	return false;
}

bool UAutoAttackComponent::CanStartAttackNow() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	return World->GetTimeSeconds() >= NextAttackReadyTime;
}

bool UAutoAttackComponent::CanExecuteAttackInCurrentMode() const
{
	return bAutoAttackEnabled
		&& OwnerCharacter
		&& !IsOwningPlayerDead()
		&& (OwnerCharacter->GetCharacterMode() == ECharacterMode::Active || OwnerCharacter->GetCharacterMode() == ECharacterMode::Assisting)
		&& GetWorld();
}

bool UAutoAttackComponent::IsOwningPlayerDead() const
{
	const ASurvivorPlayerController* SurvivorController = Cast<ASurvivorPlayerController>(OwnerCharacter ? OwnerCharacter->GetOwner() : nullptr);
	return SurvivorController && SurvivorController->IsPlayerDead();
}

bool UAutoAttackComponent::TryConsumeAttackNotify()
{
	if (IsOwningPlayerDead())
	{
		return false;
	}

	if (!bIsAttacking)
	{
		return false;
	}

	if (bAttackNotifyConsumed)
	{
		return false;
	}

	bAttackNotifyConsumed = true;
	return true;
}

void UAutoAttackComponent::RegisterDoubleCutPrimaryAttack()
{
	if (!GetWorld() || DoubleCutPrimaryAttackCount <= 0)
	{
		return;
	}

	++DoubleCutPrimaryAttackCounter;
	if (DoubleCutPrimaryAttackCounter < DoubleCutPrimaryAttackCount)
	{
		return;
	}

	DoubleCutPrimaryAttackCounter = 0;
	bDoubleCutFollowUpPending = true;
}

void UAutoAttackComponent::HandleSamuraiMomentum(int32 KilledEnemyCount)
{
	const UUpgradeDefinition* MomentumUpgrade = GetMomentumUpgrade();
	if (!MomentumUpgrade)
	{
		return;
	}

	const int32 RequiredKills = FMath::Max(1, MomentumUpgrade->MomentumRequiredKills);
	if (CVarHDLogSamuraiMomentum.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogTemp, Log, TEXT("Momentum: Attack killed %d enemies"), KilledEnemyCount);
	}

	if (KilledEnemyCount < RequiredKills)
	{
		return;
	}

	ReduceRemainingAttackCooldown(MomentumUpgrade->MomentumRemainingCooldownReduction);
}

bool UAutoAttackComponent::HasDoubleCutUpgrade() const
{
	if (!OwnerCharacter || !OwnerCharacter->IsA<ASamuraiCharacter>())
	{
		return false;
	}

	const UPlayerUpgradeComponent* PlayerUpgrades = GetPlayerUpgradesForAutoAttackMarkedForDeath(this, OwnerCharacter);
	return PlayerUpgrades && PlayerUpgrades->GetSpecialEffectLevel(EUpgradeSpecialEffect::DoubleCut) > 0;
}

const UUpgradeDefinition* UAutoAttackComponent::GetMomentumUpgrade() const
{
	if (!OwnerCharacter || !OwnerCharacter->IsA<ASamuraiCharacter>())
	{
		return nullptr;
	}

	const UPlayerUpgradeComponent* PlayerUpgrades = GetPlayerUpgradesForAutoAttackMarkedForDeath(this, OwnerCharacter);
	if (!PlayerUpgrades || PlayerUpgrades->GetSpecialEffectLevel(EUpgradeSpecialEffect::Momentum) <= 0)
	{
		return nullptr;
	}

	return PlayerUpgrades->GetAcquiredUpgradeWithSpecialEffect(EUpgradeSpecialEffect::Momentum);
}

bool UAutoAttackComponent::HasFanOfBladesUpgrade() const
{
	if (!OwnerCharacter || !OwnerCharacter->IsA<ANinjaCharacter>() || !ProjectileClass)
	{
		return false;
	}

	const UPlayerUpgradeComponent* PlayerUpgrades = GetPlayerUpgradesForAutoAttackMarkedForDeath(this, OwnerCharacter);
	return PlayerUpgrades && PlayerUpgrades->GetSpecialEffectLevel(EUpgradeSpecialEffect::FanOfBlades) > 0;
}

bool UAutoAttackComponent::WillNextNinjaAttackTriggerFanOfBlades() const
{
	return HasFanOfBladesUpgrade()
		&& FanOfBladesAttackInterval > 0
		&& FanOfBladesAttackCounter + 1 >= FanOfBladesAttackInterval;
}

void UAutoAttackComponent::RegisterNinjaAttackForFanOfBlades(const FVector& SpawnLocation, float Damage, float Speed, int32 AdditionalPierceCount)
{
	if (!HasFanOfBladesUpgrade())
	{
		return;
	}

	const int32 SafeTriggerInterval = FMath::Max(1, FanOfBladesAttackInterval);
	++FanOfBladesAttackCounter;
	if (!bActiveAttackTriggersFanOfBlades && FanOfBladesAttackCounter < SafeTriggerInterval)
	{
		return;
	}

	FanOfBladesAttackCounter = 0;
	SpawnFanOfBladesVolley(SpawnLocation, Damage, Speed, AdditionalPierceCount);
}

void UAutoAttackComponent::SpawnFanOfBladesVolley(const FVector& SpawnLocation, float Damage, float Speed, int32 AdditionalPierceCount)
{
	const int32 SafeProjectileCount = FMath::Max(1, FanOfBladesProjectileCount);
	for (int32 ProjectileIndex = 0; ProjectileIndex < SafeProjectileCount; ++ProjectileIndex)
	{
		const float AngleDegrees = (360.0f * static_cast<float>(ProjectileIndex)) / static_cast<float>(SafeProjectileCount);
		const FRotator DirectionRotation(0.0f, AngleDegrees, 0.0f);
		const FVector ProjectileDirection = DirectionRotation.Vector();
		SpawnProjectileInstance(SpawnLocation, ProjectileDirection, Damage, Speed, AdditionalPierceCount);
	}
}

const UUpgradeDefinition* UAutoAttackComponent::GetBladeCascadeUpgrade() const
{
	if (!OwnerCharacter || !OwnerCharacter->IsA<ANinjaCharacter>() || !ProjectileClass)
	{
		return nullptr;
	}

	const UPlayerUpgradeComponent* PlayerUpgrades = GetPlayerUpgradesForAutoAttackMarkedForDeath(this, OwnerCharacter);
	if (!PlayerUpgrades || PlayerUpgrades->GetSpecialEffectLevel(EUpgradeSpecialEffect::BladeCascade) <= 0)
	{
		return nullptr;
	}

	return PlayerUpgrades->GetAcquiredUpgradeWithSpecialEffect(EUpgradeSpecialEffect::BladeCascade);
}

int32 UAutoAttackComponent::ConsumeBladeCascadeBonusForNormalVolley(int32 NormalProjectileCount)
{
	const UUpgradeDefinition* BladeCascadeUpgrade = GetBladeCascadeUpgrade();
	if (!BladeCascadeUpgrade || !bBladeCascadeReady)
	{
		return NormalProjectileCount;
	}

	const int32 SafeBonusKunai = FMath::Max(1, BladeCascadeUpgrade->BladeCascadeBonusKunai);
	bBladeCascadeReady = false;
	const int32 FinalProjectileCount = NormalProjectileCount + SafeBonusKunai;

	if (CVarHDLogBladeCascade.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogTemp, Log, TEXT("Blade Cascade CONSUMED: normal=%d bonus=%d final=%d"),
			NormalProjectileCount,
			SafeBonusKunai,
			FinalProjectileCount);
	}

	return FinalProjectileCount;
}

void UAutoAttackComponent::RegisterKunaiFired()
{
	const UUpgradeDefinition* BladeCascadeUpgrade = GetBladeCascadeUpgrade();
	if (!BladeCascadeUpgrade)
	{
		return;
	}

	const int32 SafeThreshold = FMath::Max(1, BladeCascadeUpgrade->BladeCascadeKunaiThreshold);
	++BladeCascadeKunaiProgress;

	if (BladeCascadeKunaiProgress >= SafeThreshold)
	{
		if (!bBladeCascadeReady)
		{
			BladeCascadeKunaiProgress -= SafeThreshold;
			bBladeCascadeReady = true;
			if (CVarHDLogBladeCascade.GetValueOnGameThread() != 0)
			{
				UE_LOG(LogTemp, Log, TEXT("Blade Cascade READY"));
			}
			return;
		}

		BladeCascadeKunaiProgress = SafeThreshold - 1;
	}

	if (CVarHDLogBladeCascade.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogTemp, Log, TEXT("Blade Cascade: %d/%d"), BladeCascadeKunaiProgress, SafeThreshold);
	}
}

bool UAutoAttackComponent::WillNextSamuraiAttackTriggerDoubleCut() const
{
	return HasDoubleCutUpgrade()
		&& DoubleCutPrimaryAttackCount > 0
		&& DoubleCutPrimaryAttackCounter + 1 >= DoubleCutPrimaryAttackCount;
}

bool UAutoAttackComponent::AcquireDoubleCutFollowUpTarget()
{
	if (!OwnerCharacter || !OwnerCharacter->IsA<ASamuraiCharacter>())
	{
		CurrentAttackTarget.Reset();
		return false;
	}

	AEnemyBase* TargetEnemy = FindNearestEnemyTarget();
	if (!TargetEnemy || TargetEnemy->IsDead())
	{
		CurrentAttackTarget.Reset();
		if (OwnerCharacter)
		{
			OwnerCharacter->ClearFacingOverride();
		}
		return false;
	}

	CurrentAttackTarget = TargetEnemy;
	const FVector AimLocation = GetEnemyAimLocation(TargetEnemy);
	FVector ToTarget = AimLocation - OwnerCharacter->GetActorLocation();
	ToTarget.Z = 0.0f;
	if (!ToTarget.Normalize())
	{
		CurrentAttackTarget.Reset();
		return false;
	}

	OwnerCharacter->SetFacingOverrideTarget(AimLocation);
	OwnerCharacter->SetVisualFacingRotation(FRotator(0.0f, ToTarget.Rotation().Yaw, 0.0f));
	return true;
}

void UAutoAttackComponent::StartDoubleCutFollowUp()
{
	if (IsOwningPlayerDead() || !OwnerCharacter || !OwnerCharacter->IsA<ASamuraiCharacter>() || ProjectileClass)
	{
		return;
	}

	if (!AcquireDoubleCutFollowUpTarget())
	{
		return;
	}

	if (!DoubleCutMontage)
	{
		UE_LOG(LogTemp, Warning, TEXT("Double Cut triggered on %s, but DoubleCutMontage is not assigned. Falling back to immediate bonus slash."),
			*GetNameSafe(OwnerCharacter));
		ExecuteMeleeAttackTrace();
		return;
	}

	ACharacter* OwnerAsCharacter = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* MeshComponent = OwnerAsCharacter ? OwnerAsCharacter->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = MeshComponent ? MeshComponent->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("Double Cut skipped: AnimInstance invalid on %s."), *GetNameSafe(OwnerCharacter));
		return;
	}

	const float PlayResult = AnimInstance->Montage_Play(DoubleCutMontage, 1.0f);
	if (PlayResult <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("Double Cut montage failed to play on %s."), *GetNameSafe(OwnerCharacter));
		return;
	}

	FOnMontageEnded MontageEndedDelegate;
	MontageEndedDelegate.BindUObject(this, &UAutoAttackComponent::HandleDoubleCutMontageEnded);
	AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, DoubleCutMontage);

	bIsAttacking = true;
	bAttackNotifyConsumed = false;
	bActiveAttackIsAssist = false;
	bDoubleCutFollowUpActive = true;
}

void UAutoAttackComponent::ConsumePendingDoubleCutFollowUp()
{
	if (!bDoubleCutFollowUpPending)
	{
		return;
	}

	bDoubleCutFollowUpPending = false;
	bIsAttacking = false;
	bAttackNotifyConsumed = false;
	bActiveAttackIsAssist = false;
	ActiveAttackSequence = 0;
	StartDoubleCutFollowUp();
}

void UAutoAttackComponent::HandleDoubleCutMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != DoubleCutMontage)
	{
		return;
	}

	bIsAttacking = false;
	bAttackNotifyConsumed = false;
	bActiveAttackIsAssist = false;
	bDoubleCutFollowUpActive = false;
	CurrentAttackTarget.Reset();
	if (OwnerCharacter)
	{
		OwnerCharacter->ClearFacingOverride();
	}
}

float UAutoAttackComponent::GetEffectiveAttackInterval() const
{
	const UCharacterStatsComponent* CharacterStats = OwnerCharacter ? OwnerCharacter->GetCharacterStats() : nullptr;
	const float AttackSpeedMultiplier = CharacterStats ? CharacterStats->GetFinalAttackSpeedMultiplier() : 1.0f;
	const ASurvivorPlayerController* SurvivorController = Cast<ASurvivorPlayerController>(OwnerCharacter ? OwnerCharacter->GetOwner() : nullptr);
	const USharedPlayerStatsComponent* SharedStats = SurvivorController ? SurvivorController->GetSharedPlayerStats() : nullptr;
	const float GlobalAttackSpeedMultiplier = SharedStats ? SharedStats->GetFinalAttackSpeedMultiplier() : 1.0f;
	return FMath::Max(0.01f, AttackInterval / FMath::Max(0.01f, AttackSpeedMultiplier * GlobalAttackSpeedMultiplier));
}

float UAutoAttackComponent::GetEffectiveAttackDamage() const
{
	const UCharacterStatsComponent* CharacterStats = OwnerCharacter ? OwnerCharacter->GetCharacterStats() : nullptr;
	const float DamageMultiplier = CharacterStats ? CharacterStats->GetFinalDamageMultiplier() : 1.0f;
	const ASurvivorPlayerController* SurvivorController = Cast<ASurvivorPlayerController>(OwnerCharacter ? OwnerCharacter->GetOwner() : nullptr);
	const USharedPlayerStatsComponent* SharedStats = SurvivorController ? SurvivorController->GetSharedPlayerStats() : nullptr;
	const float GlobalDamageMultiplier = SharedStats ? SharedStats->GetFinalDamageMultiplier() : 1.0f;
	const UPlayerUpgradeComponent* PlayerUpgrades = SurvivorController ? SurvivorController->GetPlayerUpgrades() : nullptr;
	float PowerMultiplier = 1.0f;
	if (PlayerUpgrades)
	{
		if (OwnerCharacter && OwnerCharacter->IsA<ASamuraiCharacter>()) PowerMultiplier = PlayerUpgrades->GetSamuraiPowerMultiplier();
		else if (OwnerCharacter && OwnerCharacter->IsA<ANinjaCharacter>()) PowerMultiplier = PlayerUpgrades->GetNinjaPowerMultiplier();
	}
	return AttackDamage * DamageMultiplier * GlobalDamageMultiplier * PowerMultiplier;
}

float UAutoAttackComponent::GetEffectiveAttackRadius() const
{
	const UCharacterStatsComponent* CharacterStats = OwnerCharacter ? OwnerCharacter->GetCharacterStats() : nullptr;
	const float AttackAreaMultiplier = CharacterStats ? CharacterStats->GetFinalAttackAreaMultiplier() : 1.0f;
	return AttackRadius * AttackAreaMultiplier;
}

float UAutoAttackComponent::GetEffectiveProjectileSpeed() const
{
	const UCharacterStatsComponent* CharacterStats = OwnerCharacter ? OwnerCharacter->GetCharacterStats() : nullptr;
	const float ProjectileSpeedMultiplier = CharacterStats ? CharacterStats->GetFinalProjectileSpeedMultiplier() : 1.0f;
	return ProjectileSpeed * ProjectileSpeedMultiplier;
}

float UAutoAttackComponent::GetEffectiveTargetingRange() const
{
	if (ProjectileClass)
	{
		return TargetingRange;
	}

	const float EffectiveMeleeReach = FMath::Max(0.0f, AttackForwardOffset) + GetEffectiveAttackRadius();
	return FMath::Max(TargetingRange, EffectiveMeleeReach + 60.0f);
}

int32 UAutoAttackComponent::GetEffectiveProjectileCount() const
{
	const UCharacterStatsComponent* CharacterStats = OwnerCharacter ? OwnerCharacter->GetCharacterStats() : nullptr;
	return CharacterStats ? CharacterStats->GetFinalProjectileCount() : 1;
}

int32 UAutoAttackComponent::GetEffectiveProjectilePierceBonus() const
{
	const UCharacterStatsComponent* CharacterStats = OwnerCharacter ? OwnerCharacter->GetCharacterStats() : nullptr;
	return CharacterStats ? CharacterStats->GetFinalProjectilePierceBonus() : 0;
}

ESamuraiTechnique UAutoAttackComponent::GetActiveSamuraiTechnique() const
{
	const UPlayerUpgradeComponent* PlayerUpgrades = GetPlayerUpgradesForAutoAttackMarkedForDeath(this, OwnerCharacter);
	if (!PlayerUpgrades) return ESamuraiTechnique::None;
	if (PlayerUpgrades->GetSpecialEffectLevel(EUpgradeSpecialEffect::SamuraiCleaver) > 0) return ESamuraiTechnique::Cleaver;
	if (PlayerUpgrades->GetSpecialEffectLevel(EUpgradeSpecialEffect::SamuraiDuelist) > 0) return ESamuraiTechnique::Duelist;
	if (PlayerUpgrades->GetSpecialEffectLevel(EUpgradeSpecialEffect::SamuraiDeathblow) > 0) return ESamuraiTechnique::Deathblow;
	return ESamuraiTechnique::None;
}

float UAutoAttackComponent::ResolveDuelistPrimaryDamage(AEnemyBase* PrimaryTarget, float BasePrimaryDamage)
{
	if (!PrimaryTarget)
	{
		return BasePrimaryDamage;
	}

	if (bDoubleCutFollowUpActive)
	{
		const int32 AppliedStacks = DuelistTarget.Get() == PrimaryTarget ? DuelistStackCount : 0;
		return BasePrimaryDamage * (1.0f + FMath::Max(0.0f, DuelistDamagePerStack) * AppliedStacks);
	}

	AEnemyBase* PreviousTarget = DuelistTarget.Get();
	if (PreviousTarget != PrimaryTarget)
	{
		DuelistTarget = PrimaryTarget;
		DuelistStackCount = 0;
		OnDuelistTargetChanged.Broadcast(PreviousTarget, PrimaryTarget);
		OnDuelistStackChanged.Broadcast(DuelistStackCount);
	}
	else
	{
		if (DuelistStackCount < MAX_int32)
		{
			++DuelistStackCount;
		}
		OnDuelistStackChanged.Broadcast(DuelistStackCount);
	}

	return BasePrimaryDamage * (1.0f + FMath::Max(0.0f, DuelistDamagePerStack) * DuelistStackCount);
}

void UAutoAttackComponent::ResetDuelistState()
{
	AEnemyBase* PreviousTarget = DuelistTarget.Get();
	DuelistTarget.Reset();
	DuelistStackCount = 0;
	if (PreviousTarget) OnDuelistTargetChanged.Broadcast(PreviousTarget, nullptr);
	OnDuelistStackChanged.Broadcast(0);
}

void UAutoAttackComponent::ExecuteCleaverChain(AEnemyBase* OriginalPrimaryTarget, const FVector& OriginLocation, float RemainingDamage, EPlayerAttackSource AttackSource)
{
	if (!GetWorld() || RemainingDamage <= KINDA_SMALL_NUMBER) return;

	TSet<AEnemyBase*> VisitedTargets;
	if (OriginalPrimaryTarget) VisitedTargets.Add(OriginalPrimaryTarget);
	FVector FromLocation = OriginLocation;
	const int32 SafeTargetLimit = FMath::Max(1, MaxCleaverChainTargets);
	for (int32 ChainIndex = 0; ChainIndex < SafeTargetLimit && RemainingDamage > KINDA_SMALL_NUMBER; ++ChainIndex)
	{
		AEnemyBase* Target = FindCleaverTarget(FromLocation, VisitedTargets, AttackSource);
		if (!Target) break;
		UHealthComponent* Health = Target->GetHealthComponent();
		if (!Health || Health->IsDead()) break;

		VisitedTargets.Add(Target);
		const float HealthBeforeHit = Health->GetCurrentHealth();
		const FVector TargetLocation = Target->GetActorLocation();
		OnCleaverTransfer.Broadcast(FromLocation, TargetLocation, RemainingDamage);
		if (!Target->ApplyPlayerDamage(RemainingDamage, AttackSource)) break;
		if (!Health->IsDead()) break;

		RemainingDamage = FMath::Max(0.0f, RemainingDamage - HealthBeforeHit);
		FromLocation = TargetLocation;
	}
}

AEnemyBase* UAutoAttackComponent::FindCleaverTarget(const FVector& SearchLocation, const TSet<AEnemyBase*>& VisitedTargets, EPlayerAttackSource AttackSource) const
{
	if (!GetWorld() || CleaverChainRadius <= 0.0f) return nullptr;
	TArray<FOverlapResult> Results;
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_GameTraceChannel1);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CleaverChainTargeting), false, OwnerCharacter);
	if (OwnerCharacter) QueryParams.AddIgnoredActor(OwnerCharacter);
	GetWorld()->OverlapMultiByObjectType(Results, SearchLocation, FQuat::Identity, ObjectParams,
		FCollisionShape::MakeSphere(CleaverChainRadius), QueryParams);

	AEnemyBase* BestTarget = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	for (const FOverlapResult& Result : Results)
	{
		AEnemyBase* Candidate = Cast<AEnemyBase>(Result.GetActor());
		if (!Candidate || Candidate->IsDead() || VisitedTargets.Contains(Candidate)
			|| Candidate->GetRequiredPlayerAttackSource() != EPlayerAttackSource::Other
			|| !Candidate->CanReceivePlayerDamage(AttackSource)) continue;
		const UHealthComponent* Health = Candidate->GetHealthComponent();
		const float DistanceSquared = FVector::DistSquared(SearchLocation, Candidate->GetActorLocation());
		if (Health && !Health->IsDead() && DistanceSquared <= FMath::Square(CleaverChainRadius) && DistanceSquared < BestDistanceSquared)
		{
			BestTarget = Candidate;
			BestDistanceSquared = DistanceSquared;
		}
	}
	return BestTarget;
}

void UAutoAttackComponent::ExecuteDeathblow(AEnemyBase* DeadPrimaryTarget, const FVector& OriginLocation, float ResolvedPrimaryDamage, EPlayerAttackSource AttackSource, bool bApplyMarkedBlade)
{
	if (!GetWorld() || ResolvedPrimaryDamage <= 0.0f) return;
	const UCharacterStatsComponent* Stats = OwnerCharacter ? OwnerCharacter->GetCharacterStats() : nullptr;
	const float AreaMultiplier = Stats ? Stats->GetFinalAttackAreaMultiplier() : 1.0f;
	const float Radius = FMath::Max(0.0f, DeathblowBaseRadius) * FMath::Max(0.0f, AreaMultiplier);
	const float Damage = ResolvedPrimaryDamage * FMath::Max(0.0f, DeathblowDamageMultiplier);
	OnDeathblowTriggered.Broadcast(OriginLocation, Radius, Damage);
	if (Radius <= 0.0f || Damage <= 0.0f) return;

	TArray<FOverlapResult> Results;
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_GameTraceChannel1);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DeathblowTargets), false, OwnerCharacter);
	if (OwnerCharacter) QueryParams.AddIgnoredActor(OwnerCharacter);
	if (DeadPrimaryTarget) QueryParams.AddIgnoredActor(DeadPrimaryTarget);
	GetWorld()->OverlapMultiByObjectType(Results, OriginLocation, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(Radius), QueryParams);

	TSet<AEnemyBase*> DamagedTargets;
	for (const FOverlapResult& Result : Results)
	{
		AEnemyBase* Candidate = Cast<AEnemyBase>(Result.GetActor());
		if (!Candidate || Candidate == DeadPrimaryTarget || Candidate->IsDead() || DamagedTargets.Contains(Candidate)
			|| Candidate->GetRequiredPlayerAttackSource() != EPlayerAttackSource::Other
			|| !Candidate->CanReceivePlayerDamage(AttackSource)) continue;
		UHealthComponent* Health = Candidate->GetHealthComponent();
		if (!Health || Health->IsDead()) continue;
		DamagedTargets.Add(Candidate);
		if (Candidate->ApplyPlayerDamage(Damage, AttackSource) && bApplyMarkedBlade) Candidate->ApplyMark();
	}
}

int32 UAutoAttackComponent::GetEffectiveProjectileBounceBonus() const
{
	const UCharacterStatsComponent* CharacterStats = OwnerCharacter ? OwnerCharacter->GetCharacterStats() : nullptr;
	return CharacterStats ? CharacterStats->GetFinalProjectileBounceBonus() : 0;
}

int32 UAutoAttackComponent::GetEffectiveProjectileSplitBonus() const
{
	const UCharacterStatsComponent* CharacterStats = OwnerCharacter ? OwnerCharacter->GetCharacterStats() : nullptr;
	return CharacterStats ? CharacterStats->GetFinalProjectileSplitBonus() : 0;
}

float UAutoAttackComponent::GetBaseAttackInterval() const
{
	return AttackInterval;
}

float UAutoAttackComponent::GetBaseAttackDamage() const
{
	return AttackDamage;
}

float UAutoAttackComponent::GetBaseAttackRadius() const
{
	return AttackRadius;
}

float UAutoAttackComponent::GetBaseProjectileSpeed() const
{
	return ProjectileSpeed;
}

AEnemyBase* UAutoAttackComponent::FindNearestEnemyTarget() const
{
	if (!ProjectileClass && OwnerCharacter)
	{
		return FindBestMeleeTarget(OwnerCharacter->GetActorLocation(), GetEffectiveTargetingRange());
	}

	TArray<AEnemyBase*> SortedTargets;
	FindEnemyTargetsSorted(SortedTargets);
	AEnemyBase* BestEnemy = SortedTargets.Num() > 0 ? SortedTargets[0] : nullptr;

	if (bDebugTargeting)
	{
		constexpr float DebugDuration = 1.5f;
	DrawDebugSphere(GetWorld(), OwnerCharacter->GetActorLocation(), GetEffectiveTargetingRange(), 48, FColor::Green, false, DebugDuration, 0, 2.0f);
		if (BestEnemy)
		{
			DrawDebugLine(GetWorld(), OwnerCharacter->GetActorLocation(), BestEnemy->GetActorLocation(), FColor::Green, false, DebugDuration, 0, 3.0f);
		}
	}

	return BestEnemy;
}

AEnemyBase* UAutoAttackComponent::FindBestMeleeTarget(const FVector& SearchLocation, float SearchRadius) const
{
	TArray<AEnemyBase*> Candidates;
	FindEnemyTargetsSortedFromLocation(SearchLocation, SearchRadius, Candidates);
	if (Candidates.Num() == 0)
	{
		return nullptr;
	}

	if (Candidates.Num() > MaxMeleeClusterCandidates)
	{
		Candidates.SetNum(MaxMeleeClusterCandidates);
	}

	AEnemyBase* BestTarget = nullptr;
	float BestScore = -FLT_MAX;

	if (CVarHDDebugSamuraiTargeting.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogTemp, Log, TEXT("=== SAMURAI TARGETING ==="));
	}

	for (AEnemyBase* Candidate : Candidates)
	{
		int32 ClusterCount = 0;
		float DistancePenalty = 0.0f;
		float ImmediateThreatBonus = 0.0f;
		const float Score = ScoreMeleeTarget(Candidate, Candidates, SearchLocation, SearchRadius, ClusterCount, DistancePenalty, ImmediateThreatBonus);

		if (CVarHDDebugSamuraiTargeting.GetValueOnGameThread() != 0)
		{
			UE_LOG(LogTemp, Log, TEXT("Candidate=%s Distance=%.1f NearbyEnemies=%d ClusterScore=%.2f DistancePenalty=%.2f ImmediateBonus=%.2f FinalScore=%.2f"),
				*GetNameSafe(Candidate),
				Candidate ? FVector::Dist2D(SearchLocation, Candidate->GetActorLocation()) : 0.0f,
				ClusterCount,
				static_cast<float>(ClusterCount) * MeleeClusterTargetingWeight,
				DistancePenalty,
				ImmediateThreatBonus,
				Score);
		}

		if (Score > BestScore)
		{
			BestScore = Score;
			BestTarget = Candidate;
		}
	}

	if (CVarHDDebugSamuraiTargeting.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogTemp, Log, TEXT("SELECTED: %s Score=%.2f"), *GetNameSafe(BestTarget), BestScore);
		UE_LOG(LogTemp, Log, TEXT("=========================="));

		if (GetWorld() && BestTarget)
		{
			DrawDebugLine(GetWorld(), SearchLocation, BestTarget->GetActorLocation(), FColor::Orange, false, 1.0f, 0, 3.0f);
			DrawDebugSphere(GetWorld(), BestTarget->GetActorLocation(), GetEffectiveAttackRadius(), 24, FColor::Orange, false, 1.0f, 0, 2.0f);
		}
	}

	return BestTarget;
}

float UAutoAttackComponent::ScoreMeleeTarget(AEnemyBase* Candidate, const TArray<AEnemyBase*>& Candidates, const FVector& SearchLocation, float SearchRadius, int32& OutClusterCount, float& OutDistancePenalty, float& OutImmediateThreatBonus) const
{
	OutClusterCount = 0;
	OutDistancePenalty = 0.0f;
	OutImmediateThreatBonus = 0.0f;

	if (!Candidate)
	{
		return -FLT_MAX;
	}

	const float ClusterRadius = GetEffectiveAttackRadius();
	const FVector CandidateLocation = Candidate->GetActorLocation();
	for (const AEnemyBase* OtherCandidate : Candidates)
	{
		if (OtherCandidate && FVector::DistSquared2D(CandidateLocation, OtherCandidate->GetActorLocation()) <= FMath::Square(ClusterRadius))
		{
			++OutClusterCount;
		}
	}

	const float Distance = FVector::Dist2D(SearchLocation, CandidateLocation);
	const float NormalizedDistance = SearchRadius > KINDA_SMALL_NUMBER ? FMath::Clamp(Distance / SearchRadius, 0.0f, 1.0f) : 1.0f;
	OutDistancePenalty = NormalizedDistance * MeleeDistanceTargetingWeight;

	const float EffectiveMeleeReach = FMath::Max(0.0f, AttackForwardOffset) + GetEffectiveAttackRadius();
	if (Distance <= EffectiveMeleeReach * MeleeImmediateThreatRangeFraction)
	{
		OutImmediateThreatBonus = MeleeImmediateThreatBonus;
	}

	return static_cast<float>(OutClusterCount) * MeleeClusterTargetingWeight
		- OutDistancePenalty
		+ OutImmediateThreatBonus;
}

void UAutoAttackComponent::FindEnemyTargetsSorted(TArray<AEnemyBase*>& OutTargets) const
{
	FindEnemyTargetsSortedFromLocation(OwnerCharacter ? OwnerCharacter->GetActorLocation() : FVector::ZeroVector, GetEffectiveTargetingRange(), OutTargets);
}

void UAutoAttackComponent::FindEnemyTargetsSortedFromLocation(const FVector& SearchLocation, float SearchRadius, TArray<AEnemyBase*>& OutTargets) const
{
	OutTargets.Reset();

	if (!OwnerCharacter || !GetWorld())
	{
		return;
	}

	TArray<FOverlapResult> OverlapResults;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AutoAttackTargeting), false, OwnerCharacter);
	QueryParams.AddIgnoredActor(OwnerCharacter);

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_GameTraceChannel1);

	GetWorld()->OverlapMultiByObjectType(
		OverlapResults,
		SearchLocation,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(FMath::Max(0.0f, SearchRadius)),
		QueryParams);

	const AActor* OwnerActor = OwnerCharacter->GetOwner();
	TSet<AEnemyBase*> UniqueEnemies;

	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* OverlappedActor = OverlapResult.GetActor();
		if (!OverlappedActor || OverlappedActor == OwnerCharacter || OverlappedActor->GetOwner() == OwnerActor)
		{
			continue;
		}

		AEnemyBase* Enemy = Cast<AEnemyBase>(OverlappedActor);
		if (!Enemy || Enemy->IsDead())
		{
			continue;
		}

		if (UniqueEnemies.Contains(Enemy))
		{
			continue;
		}

		UHealthComponent* EnemyHealth = Enemy->GetHealthComponent();
		if (!EnemyHealth || EnemyHealth->IsDead())
		{
			continue;
		}

		UniqueEnemies.Add(Enemy);
		OutTargets.Add(Enemy);
	}

	OutTargets.Sort([SearchLocation](const AEnemyBase& Left, const AEnemyBase& Right)
	{
		return FVector::DistSquared2D(SearchLocation, Left.GetActorLocation())
			< FVector::DistSquared2D(SearchLocation, Right.GetActorLocation());
	});
}

FVector UAutoAttackComponent::GetProjectileSpawnLocation() const
{
	if (!OwnerCharacter)
	{
		return FVector::ZeroVector;
	}

	const USkeletalMeshComponent* MeshComponent = OwnerCharacter->GetMesh();
	if (MeshComponent && ProjectileSpawnSocket != NAME_None && MeshComponent->DoesSocketExist(ProjectileSpawnSocket))
	{
		return MeshComponent->GetSocketLocation(ProjectileSpawnSocket);
	}

	const FVector VisualForward = OwnerCharacter->GetVisualForwardVector();
	const FVector VisualRight = FRotationMatrix(OwnerCharacter->GetVisualFacingRotation()).GetScaledAxis(EAxis::Y);
	return OwnerCharacter->GetActorLocation()
		+ VisualForward * ProjectileSpawnOffset.X
		+ VisualRight * ProjectileSpawnOffset.Y
		+ FVector::UpVector * ProjectileSpawnOffset.Z;
}

FVector UAutoAttackComponent::GetEnemyAimLocation(const AEnemyBase* Enemy) const
{
	if (!Enemy)
	{
		return FVector::ZeroVector;
	}

	if (const UCapsuleComponent* CapsuleComponent = Enemy->GetCapsuleComponent())
	{
		return CapsuleComponent->GetComponentLocation();
	}

	return Enemy->GetActorLocation();
}

bool UAutoAttackComponent::CanAutoAttack() const
{
	return bAutoAttackEnabled
		&& OwnerCharacter
		&& !IsOwningPlayerDead()
		&& OwnerCharacter->GetCharacterMode() == ECharacterMode::Active
		&& GetWorld();
}
