// Copyright Epic Games, Inc. All Rights Reserved.

#include "AutoAttackComponent.h"
#include "SwapPresentationComponent.h"
#include "NinjaBuildComponent.h"
#include "SurvivorAbilityComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AttackProjectileBase.h"
#include "CharacterStatsComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "EnemyBase.h"
#include "EnemyStatusEffectComponent.h"
#include "GameFramework/Character.h"
#include "HealthComponent.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "NinjaCharacter.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "PlayerUpgradeComponent.h"
#include "SamuraiCharacter.h"
#include "SamuraiBladeWave.h"
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



static UPlayerUpgradeComponent* GetPlayerUpgradesForAutoAttackMarkedForDeath(const UObject* WorldContextObject, const AActor* PlayerCharacter)
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
	BladeWaveClass = ASamuraiBladeWave::StaticClass();
	ImpactFeedback.bEnableCameraShake = true;
	ImpactFeedback.CameraShakeClass = USamuraiImpactCameraShake::StaticClass();
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
	bDoubleCutFollowUpActive = false;
	bDoubleCutFollowUpPending = false;
	ActiveAttackMontage = nullptr;
	ActiveAttackSequence = 0;
	if (OwnerCharacter)
	{
		OwnerCharacter->ClearFacingOverride();
	}
	RestoreAttackWeaponVisualScale();
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

void UAutoAttackComponent::PerformAttackTrace()
{
	if (!CanExecuteAttackInCurrentMode())
	{
		return;
	}
	if (!TryConsumeAttackNotify())
	{
		return;
	}

	// Reaching this point means the attack montage's hit notify was consumed for
	// one legitimate Samurai swing. This shared commit path includes both the
	// primary swing and Double Cut's actual follow-up swing.
	LastResolvedPrimaryAttackDamage = GetEffectiveAttackDamage();
	const bool bMeleeTraceResolved = ExecuteMeleeAttackTrace();
	if (bMeleeTraceResolved)
	{
		SpawnBladeWavesForAttack(LastResolvedPrimaryAttackDamage);
	}

	if (bDoubleCutFollowUpActive)
	{
		if (bMeleeTraceResolved)
		{
			// The earned proc is consumed only here: the follow-up montage has
			// reached its authoritative committed-damage notify.
			bDoubleCutReady = false;
			OnAutoAttack.Broadcast(this, EAutoAttackSource::DoubleCut);
		}
		return;
	}

	const bool bCountsForDoubleCut = !ProjectileClass && !bActiveAttackIsAssist && HasDoubleCutUpgrade();
	if (bMeleeTraceResolved && bCountsForDoubleCut)
	{
		RegisterDoubleCutPrimaryAttack();
		bDoubleCutFollowUpPending = bDoubleCutReady;
	}
}

void UAutoAttackComponent::CaptureRunState(FAutoAttackRunState& OutState) const
{
	OutState.DoubleCutCounter = DoubleCutPrimaryAttackCounter;
	OutState.bDoubleCutReady = bDoubleCutReady;
	OutState.CrossingBladesCounter = CrossingBladesAttackCounter;
	OutState.bGrandEntranceReady = bGrandEntranceReady;
	OutState.bExtraProjectileOnRight = bNormalVolleyExtraProjectileOnRight;
}

void UAutoAttackComponent::RestoreRunState(const FAutoAttackRunState& State)
{
	const int32 DoubleCutThreshold = FMath::Max(1, DoubleCutPrimaryAttackCount);
	bDoubleCutReady = State.bDoubleCutReady || State.DoubleCutCounter == DoubleCutThreshold;
	DoubleCutPrimaryAttackCounter = State.DoubleCutCounter > DoubleCutThreshold
		? DoubleCutThreshold - 1
		: FMath::Clamp(State.DoubleCutCounter, 0, DoubleCutThreshold - 1);
	CrossingBladesAttackCounter = FMath::Max(0, State.CrossingBladesCounter);
	bGrandEntranceReady = State.bGrandEntranceReady;
	bNormalVolleyExtraProjectileOnRight = State.bExtraProjectileOnRight;
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

 if(auto* B=GetOwner()->FindComponentByClass<UNinjaBuildComponent>();B&&B->ReplaceVolley(bActiveAttackIsAssist && CurrentAttackTarget.IsValid() ? GetEnemyAimLocation(CurrentAttackTarget.Get())-OwnerCharacter->GetActorLocation() : ActiveAttackDirection,bActiveAttackIsAssist))return;
	if (!ProjectileClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Projectile spawn skipped: ProjectileClass invalid."));
		return;
	}

	const FVector SpawnLocation = GetProjectileSpawnLocation();
	const float EffectiveProjectileSpeed = GetEffectiveProjectileSpeed();
	const float EffectiveAttackDamage = GetEffectiveAttackDamage();
	const int32 NormalProjectileCount = GetEffectiveProjectileCount();
	int32 EffectiveProjectileCount = NormalProjectileCount;
	const int32 EffectiveProjectilePierceBonus = GetEffectiveProjectilePierceBonus();

	AEnemyBase* PrimaryTarget = nullptr;
	FVector BaseDirection = FVector::ZeroVector;
	if (bActiveAttackIsAssist && CurrentAttackTarget.IsValid())
	{
		BaseDirection=(GetEnemyAimLocation(CurrentAttackTarget.Get())-SpawnLocation).GetSafeNormal2D();
	}
	else if (IsCursorTargetingEnabledForNormalAttack())
	{
		BaseDirection = ActiveAttackDirection;
		BaseDirection.Z = 0.0f;
		if (!BaseDirection.Normalize() && !ResolveCursorAttackDirection(BaseDirection)) return;
	}
	else
	{
		TArray<AEnemyBase*> TargetCandidates;
		FindEnemyTargetsSorted(TargetCandidates);
		if (TargetCandidates.IsEmpty())
		{
			if (OwnerCharacter) OwnerCharacter->ClearFacingOverride();
			return;
		}

		PrimaryTarget = TargetCandidates[0];
		if (!PrimaryTarget || PrimaryTarget->IsDead()) return;
		BaseDirection = GetEnemyAimLocation(PrimaryTarget) - SpawnLocation;
		BaseDirection.Z = 0.0f;
		if (!BaseDirection.Normalize()) return;
	}
	TArray<FVector> VolleyDirections;
	const UUpgradeDefinition* GrandEntrance = GetReadyGrandEntranceUpgrade();
	float VolleySpacing = KunaiSpreadAngle;
	if (GrandEntrance)
	{
		EffectiveProjectileCount = FMath::Clamp(EffectiveProjectileCount + FMath::Clamp(FMath::RoundToInt(GrandEntrance->GetBalanceValue(TEXT("NinjaBonusProjectiles"),8)),1,64),1,128);
		VolleySpacing = FMath::Clamp(GrandEntrance->GetBalanceValue(TEXT("NinjaFanAngle"),100),0.f,180.f) / FMath::Max(1,EffectiveProjectileCount-1);
		bGrandEntranceReady = false;
	}
 if(auto* B=GetOwner()->FindComponentByClass<UNinjaBuildComponent>())
 {
  if(bActiveAttackIsAssist)
  {
   int32 Volley=B->VolleyCount, Consecutive=B->ConsecutiveVolleys;
   B->ModifyVolleyWithCounters(BaseDirection,EffectiveProjectileCount,VolleySpacing,Volley,Consecutive);
  }
  else B->ModifyVolley(BaseDirection,EffectiveProjectileCount,VolleySpacing);
 }
	BuildCenteredProjectileSpreadDirections(BaseDirection, EffectiveProjectileCount, VolleySpacing, bNormalVolleyExtraProjectileOnRight, VolleyDirections);
	if (EffectiveProjectileCount % 2 == 0) bNormalVolleyExtraProjectileOnRight = !bNormalVolleyExtraProjectileOnRight;

	int32 SpawnedProjectileCount = 0;
	for (int32 ProjectileIndex = 0; ProjectileIndex < VolleyDirections.Num(); ++ProjectileIndex)
	{
		const FVector& SpawnDirection = VolleyDirections[ProjectileIndex];
		SpawnProjectileInstance(SpawnLocation, SpawnDirection, EffectiveAttackDamage, EffectiveProjectileSpeed, EffectiveProjectilePierceBonus);
		++SpawnedProjectileCount;

		if (CVarHDLogNinjaProjectileSpread.GetValueOnGameThread() != 0)
		{
			UE_LOG(LogTemp, Log, TEXT("Ninja Attack Projectile %d/%d -> %s Direction=%s"),
				ProjectileIndex + 1,
				EffectiveProjectileCount,
				PrimaryTarget ? *GetNameSafe(PrimaryTarget) : TEXT("Cursor"),
				*SpawnDirection.ToString());
		}

		if (bDebugTargeting)
		{
			constexpr float DebugDuration = 1.5f;
			DrawDebugLine(GetWorld(), SpawnLocation, SpawnLocation + SpawnDirection * GetEffectiveTargetingRange(), FColor::Yellow, false, DebugDuration, 0, 3.0f);
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


	CurrentAttackTarget.Reset();
	OwnerCharacter->ClearFacingOverride();
}

void UAutoAttackComponent::SpawnProjectileInstance(const FVector& SpawnLocation, const FVector& ProjectileDirection, float Damage, float Speed, int32 AdditionalPierceCount)
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

	Projectile->bAssistProjectile=bActiveAttackIsAssist;
	Projectile->InitializeProjectile(
		OwnerCharacter,
		ProjectileDirection,
		Damage,
		Speed,
		EProjectileTargetType::Enemies,
		GetEffectiveTargetingRange(),
		nullptr,
		true,
		AdditionalPierceCount,
		GetEffectiveProjectileBounceBonus(),
		GetEffectiveProjectileSplitBonus());

}

bool UAutoAttackComponent::SpawnShadowCloneVolley(const FVector& SpawnLocation, float SearchRange, bool& bExtraProjectileOnRight, int32* CloneVolley, int32* CloneConsecutive)
{
    // Clone notifies may overlap a Tag Team animation on their source Ninja.
    TGuardValue<bool> CloneSourceGuard(bActiveAttackIsAssist,false);
	if (!OwnerCharacter || !OwnerCharacter->IsA<ANinjaCharacter>() || !ProjectileClass || !GetWorld() || IsOwningPlayerDead())
	{
		return false;
	}

	TArray<AEnemyBase*> Targets;
	FindEnemyTargetsSortedFromLocation(SpawnLocation, FMath::Max(0.0f, SearchRange), Targets);
	if (Targets.Num() == 0) return false;

	AEnemyBase* PrimaryTarget = Targets[0];
	if (!PrimaryTarget || PrimaryTarget->IsDead()) return false;
	FVector BaseDirection = GetEnemyAimLocation(PrimaryTarget) - SpawnLocation;
	BaseDirection.Z = 0.0f;
	if (!BaseDirection.Normalize()) return false;
	int32 ProjectileCount = FMath::Max(1, GetEffectiveProjectileCount());
	const float Damage = GetEffectiveAttackDamage();
	const float Speed = GetEffectiveProjectileSpeed();
	const int32 Pierce = GetEffectiveProjectilePierceBonus();
	TArray<FVector> VolleyDirections;
	float Spacing=KunaiSpreadAngle;
 auto* Build=OwnerCharacter->FindComponentByClass<UNinjaBuildComponent>();
 if(Build&&CloneVolley&&CloneConsecutive)Build->ModifyVolleyWithCounters(BaseDirection,ProjectileCount,Spacing,*CloneVolley,*CloneConsecutive);
 BuildCenteredProjectileSpreadDirections(BaseDirection, ProjectileCount, Spacing, bExtraProjectileOnRight, VolleyDirections);
	if (ProjectileCount % 2 == 0) bExtraProjectileOnRight = !bExtraProjectileOnRight;
	int32 Spawned = 0;
	for (const FVector& Direction : VolleyDirections)
	{
		SpawnProjectileInstance(SpawnLocation, Direction, Damage, Speed, Pierce);
		++Spawned;
	}
 return Spawned > 0;
}

void UAutoAttackComponent::BuildCenteredProjectileSpreadDirections(const FVector& BaseDirection, int32 ProjectileCount, float SpreadAngleDegrees, bool bExtraProjectileOnRight, TArray<FVector>& OutDirections)
{
	OutDirections.Reset();
	const int32 Count = FMath::Max(0, ProjectileCount);
	if (Count == 0) return;
	const float Spacing = FMath::Max(0.0f, SpreadAngleDegrees);
	const int32 Half = Count / 2;
	const int32 MinimumStep = Count % 2 == 1 ? -Half : (bExtraProjectileOnRight ? -(Half - 1) : -Half);
	const int32 MaximumStep = Count % 2 == 1 ? Half : (bExtraProjectileOnRight ? Half : Half - 1);
	OutDirections.Reserve(Count);
	for (int32 Step = MinimumStep; Step <= MaximumStep; ++Step)
	{
		FVector Direction = BaseDirection.RotateAngleAxis(static_cast<float>(Step) * Spacing, FVector::UpVector);
		Direction.Z = 0.0f;
		if (Direction.Normalize()) OutDirections.Add(Direction);
	}
}

AEnemyBase* UAutoAttackComponent::FindAssistTarget() const
{
	return OwnerCharacter ? FindAssistTargetNearLocation(OwnerCharacter->GetActorLocation(), GetEffectiveTargetingRange()) : nullptr;
}

AEnemyBase* UAutoAttackComponent::FindAssistTargetNearLocation(const FVector& SearchLocation, float SearchRadius) const
{
	TArray<AEnemyBase*> SortedTargets;
	FindEnemyTargetsSortedFromLocation(SearchLocation, SearchRadius, SortedTargets);
	if (OwnerCharacter && OwnerCharacter->GetOwner())
	{
		if (const auto* Abilities = OwnerCharacter->GetOwner()->FindComponentByClass<USurvivorAbilityComponent>())
		{
			const auto Source = AEnemyBase::ResolvePlayerAttackSource(OwnerCharacter);
			Abilities->PrioritizePreparedTargets(Source, SortedTargets);
			if (!SortedTargets.IsEmpty() && Abilities->HasTriggerablePreparation(Source, SortedTargets[0])) return SortedTargets[0];
		}
	}
	if (!ProjectileClass)
	{
		return FindBestMeleeTarget(SearchLocation, SearchRadius);
	}

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
	if (OldMode == ECharacterMode::Active && NewMode != ECharacterMode::Active) bGrandEntranceReady = false;
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
	if(OwnerCharacter && OwnerCharacter->SwapPresentation && OwnerCharacter->SwapPresentation->IsBlockingAttacks()) return false;
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

	const float NormalCalculatedPlayRate = CalculateAttackMontagePlayRate(MontageToPlay);
	const bool bProspectiveDoubleCutPrimary = bUpdateNormalCooldown
		&& !ProjectileClass
		&& OwnerCharacter
		&& OwnerCharacter->IsA<ASamuraiCharacter>()
		&& WillNextSamuraiAttackTriggerDoubleCut();
	const float ActualPlayRate = NormalCalculatedPlayRate
		* (bProspectiveDoubleCutPrimary ? FMath::Max(0.01f, DoubleCutPrimarySpeedMultiplier) : 1.0f);
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
	ActiveAttackMontage = MontageToPlay;
	ApplyAttackWeaponVisualScale();
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
	const UAnimMontage* FollowUpMontage = DoubleCutMontage ? DoubleCutMontage.Get() : AttackMontage.Get();
	if (!FollowUpMontage) return 0.0f;
	return FollowUpMontage->GetPlayLength() / FMath::Max(0.01f, CalculateAttackMontagePlayRate(FollowUpMontage));
}

void UAutoAttackComponent::HandleAttackMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != ActiveAttackMontage)
	{
		return;
	}

	if (bInterrupted)
	{
		RestoreAttackWeaponVisualScale();
	}
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
	// A Double Cut is a chained attack after the normal primary has fully ended,
	// never an alternate montage selected during the primary's blend-out.
	if (ConsumePendingDoubleCutFollowUp())
	{
		return;
	}

	bIsAttacking = false;
	bAttackNotifyConsumed = false;
	bActiveAttackIsAssist = false;
	ActiveAttackMontage = nullptr;
	CurrentAttackTarget.Reset();
	if (OwnerCharacter)
	{
		OwnerCharacter->ClearFacingOverride();
	}
	ActiveAttackSequence = 0;
	RestoreAttackWeaponVisualScale();
}

bool UAutoAttackComponent::StartTargetedAttack()
{
 if(auto* B=GetOwner()->FindComponentByClass<UNinjaBuildComponent>();B&&B->Has(TEXT("ReturningFang")))return false;
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

	if (IsCursorTargetingEnabledForNormalAttack())
	{
		FVector CursorDirection;
		if (!ResolveCursorAttackDirection(CursorDirection))
		{
			return false;
		}

		CurrentAttackTarget.Reset();
		ActiveAttackDirection = CursorDirection;
		OwnerCharacter->SetFacingOverrideTarget(OwnerCharacter->GetActorLocation() + CursorDirection * FMath::Max(100.0f, GetEffectiveTargetingRange()));
		if (PlayAttackMontage())
		{
			OnAutoAttack.Broadcast(this, EAutoAttackSource::NormalAutoAttack);
			return true;
		}
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
	ActiveAttackDirection = AimLocation - OwnerCharacter->GetActorLocation();
	ActiveAttackDirection.Z = 0.0f;
	ActiveAttackDirection.Normalize();
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
	if(OwnerCharacter && OwnerCharacter->SwapPresentation && OwnerCharacter->SwapPresentation->IsBlockingAttacks()) return false;
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

bool UAutoAttackComponent::IsCursorTargetingEnabledForNormalAttack() const
{
	if (bActiveAttackIsAssist || !OwnerCharacter || OwnerCharacter->GetCharacterMode() != ECharacterMode::Active)
	{
		return false;
	}

	const ASurvivorPlayerController* Controller = Cast<ASurvivorPlayerController>(OwnerCharacter->GetOwner());
	if (!Controller)
	{
		Controller = Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	}
	return Controller && !Controller->IsAutoTargetingEnabled();
}

bool UAutoAttackComponent::ResolveCursorAttackDirection(FVector& OutDirection) const
{
	const ASurvivorPlayerController* Controller = Cast<ASurvivorPlayerController>(OwnerCharacter ? OwnerCharacter->GetOwner() : nullptr);
	if (!Controller)
	{
		Controller = Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	}
	return Controller && OwnerCharacter && Controller->GetCursorAttackDirection(OwnerCharacter->GetActorLocation(), OutDirection);
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

void UAutoAttackComponent::ArmGrandEntranceAfterSwap()
{
	const auto* Upgrades = GetPlayerUpgradesForAutoAttackMarkedForDeath(this, OwnerCharacter);
	bGrandEntranceReady = OwnerCharacter && OwnerCharacter->GetCharacterMode() == ECharacterMode::Active
		&& Upgrades && Upgrades->HasUpgradeId(TEXT("GrandEntrance"));
}

const UUpgradeDefinition* UAutoAttackComponent::GetReadyGrandEntranceUpgrade() const
{
	if (!bGrandEntranceReady || bActiveAttackIsAssist || bDoubleCutFollowUpActive || !OwnerCharacter
		|| OwnerCharacter->GetCharacterMode() != ECharacterMode::Active || IsOwningPlayerDead()) return nullptr;
	const auto* Controller = Cast<ASurvivorPlayerController>(OwnerCharacter->GetOwner());
	if (!Controller || !Controller->IsRunInProgress()) return nullptr;
	const auto* Upgrades = Controller->GetPlayerUpgrades();
	return Upgrades && Upgrades->HasUpgradeId(TEXT("GrandEntrance")) ? Upgrades->FindUpgradeDefinition(TEXT("GrandEntrance")) : nullptr;
}

float UAutoAttackComponent::GetGrandEntranceRadius(const UUpgradeDefinition* Upgrade) const
{
	const auto* Stats = OwnerCharacter ? OwnerCharacter->GetCharacterStats() : nullptr;
	return FMath::Max(GetEffectiveAttackRadius(), FMath::Max(0.f,Upgrade->GetBalanceValue(TEXT("SamuraiRadius"),600)) * (Stats ? Stats->GetFinalAttackAreaMultiplier() : 1.f));
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
	const auto* GrandEntrance = GetReadyGrandEntranceUpgrade();
	return FMath::Max(TargetingRange, FMath::Max(EffectiveMeleeReach, GrandEntrance ? GetGrandEntranceRadius(GrandEntrance) : 0.f) + 60.0f);
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

int32 UAutoAttackComponent::GetEffectiveProjectileBounceBonus() const
{
	const UCharacterStatsComponent* CharacterStats = OwnerCharacter ? OwnerCharacter->GetCharacterStats() : nullptr;
	return CharacterStats ? CharacterStats->GetFinalProjectileBounceBonus() : 0;
}

int32 UAutoAttackComponent::GetEffectiveProjectileSplitBonus() const
{
 const auto* Build=OwnerCharacter?OwnerCharacter->FindComponentByClass<UNinjaBuildComponent>():nullptr;
 return Build&&Build->Has(TEXT("ForkingProjectiles"))?1:0;
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
