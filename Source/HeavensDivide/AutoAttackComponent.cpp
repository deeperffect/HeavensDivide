// Copyright Epic Games, Inc. All Rights Reserved.

#include "AutoAttackComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AttackProjectileBase.h"
#include "CharacterStatsComponent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "EnemyBase.h"
#include "GameFramework/Character.h"
#include "HealthComponent.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
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

	UE_LOG(LogTemp, Log, TEXT("[%.2f] StartAutoAttack called Owner=%s Component=%p TimerActive=%s"),
		GetWorld()->GetTimeSeconds(),
		*GetNameSafe(GetOwner()),
		this,
		GetWorld()->GetTimerManager().IsTimerActive(AttackTimerHandle) ? TEXT("true") : TEXT("false"));

	ScheduleNextAttackTimerFromCooldown();
}

void UAutoAttackComponent::StopAutoAttack()
{
	if (UWorld* World = GetWorld())
	{
		UE_LOG(LogTemp, Log, TEXT("[%.2f] StopAutoAttack called Owner=%s Component=%p TimerActive=%s"),
			World->GetTimeSeconds(),
			*GetNameSafe(GetOwner()),
			this,
			World->GetTimerManager().IsTimerActive(AttackTimerHandle) ? TEXT("true") : TEXT("false"));
		World->GetTimerManager().ClearTimer(AttackTimerHandle);
	}

	CurrentAttackTarget.Reset();
	bIsAttacking = false;
	bAttackNotifyConsumed = false;
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
		UE_LOG(LogTemp, Log, TEXT("[%.2f] SetAttackInterval restarting timer Owner=%s Component=%p AttackInterval=%.3f"),
			GetWorld()->GetTimeSeconds(),
			*GetNameSafe(GetOwner()),
			this,
			GetEffectiveAttackInterval());
		ScheduleNextAttackTimer(GetEffectiveAttackInterval());
	}
}

void UAutoAttackComponent::PerformAttackTrace()
{
	UE_LOG(LogTemp, Log, TEXT("[%.2f] Attack #%d Notify Fired"), GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0, ActiveAttackSequence);
	if (!TryConsumeAttackNotify())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("PerformAttackTrace Called"));

	if (!OwnerCharacter || !GetWorld())
	{
		UE_LOG(LogTemp, Warning, TEXT("AutoAttack trace skipped: owner/world invalid."));
		return;
	}

	FVector AttackForward = OwnerCharacter->GetVisualForwardVector();
	AttackForward.Z = 0.0f;
	if (!AttackForward.Normalize())
	{
		UE_LOG(LogTemp, Warning, TEXT("AutoAttack trace skipped: visual forward invalid."));
		return;
	}

	const FVector AttackOrigin = OwnerCharacter->GetActorLocation();
	const FVector HitboxCenter = AttackOrigin + AttackForward * AttackForwardOffset;
	const float EffectiveAttackRadius = GetEffectiveAttackRadius();
	const float EffectiveAttackDamage = GetEffectiveAttackDamage();

	UE_LOG(LogTemp, Log, TEXT("AutoAttack trace origin: %s"), *AttackOrigin.ToString());
	UE_LOG(LogTemp, Log, TEXT("AutoAttack trace direction: %s"), *AttackForward.ToString());
	UE_LOG(LogTemp, Log, TEXT("AutoAttack trace sphere center: %s"), *HitboxCenter.ToString());
	UE_LOG(LogTemp, Log, TEXT("AutoAttack trace AttackRadius: %.2f AttackRange: %.2f AttackForwardOffset: %.2f"),
		EffectiveAttackRadius,
		AttackRange,
		AttackForwardOffset);

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

	TSet<AEnemyBase*> DamagedEnemies;
	const AActor* OwnerActor = OwnerCharacter->GetOwner();
	for (const FHitResult& HitResult : HitResults)
	{
		AActor* HitActor = HitResult.GetActor();
		if (!HitActor || HitActor == OwnerCharacter || HitActor->GetOwner() == OwnerActor)
		{
			continue;
		}

		AEnemyBase* HitEnemy = Cast<AEnemyBase>(HitActor);
		if (!HitEnemy || HitEnemy->IsDead() || DamagedEnemies.Contains(HitEnemy))
		{
			continue;
		}

		UHealthComponent* EnemyHealth = HitEnemy->GetHealthComponent();
		if (!EnemyHealth || EnemyHealth->IsDead())
		{
			continue;
		}

		DamagedEnemies.Add(HitEnemy);
		EnemyHealth->ApplyDamage(EffectiveAttackDamage);
		UE_LOG(LogTemp, Log, TEXT("AutoAttack damaged enemy: %s Damage=%.2f RemainingHealth=%.2f"),
			*GetNameSafe(HitEnemy),
			EffectiveAttackDamage,
			EnemyHealth->GetCurrentHealth());
	}

	if (bDebugAttackTrace)
	{
		const FColor DebugColor = DamagedEnemies.Num() > 0 ? FColor::Red : FColor::Cyan;
		constexpr float DebugDuration = 1.5f;
		DrawDebugLine(GetWorld(), AttackOrigin, HitboxCenter, DebugColor, false, DebugDuration, 0, 4.0f);
		DrawDebugSphere(GetWorld(), HitboxCenter, EffectiveAttackRadius, 24, DebugColor, false, DebugDuration, 0, 4.0f);
		UE_LOG(LogTemp, Log, TEXT("AutoAttack debug trace drawn."));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("AutoAttack debug trace disabled."));
	}
}

void UAutoAttackComponent::SpawnAutoAttackProjectile()
{
	UE_LOG(LogTemp, Log, TEXT("[%.2f] Attack #%d Projectile Notify Fired"), GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0, ActiveAttackSequence);
	if (!TryConsumeAttackNotify())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("SpawnAutoAttackProjectile Called"));

	if (!CanAutoAttack())
	{
		UE_LOG(LogTemp, Warning, TEXT("Projectile spawn skipped: auto attack cannot run."));
		return;
	}

	if (!ProjectileClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Projectile spawn skipped: ProjectileClass invalid."));
		return;
	}

	AEnemyBase* TargetEnemy = CurrentAttackTarget.Get();
	if (!TargetEnemy || TargetEnemy->IsDead())
	{
		TargetEnemy = FindNearestEnemyTarget();
		CurrentAttackTarget = TargetEnemy;
	}

	if (!TargetEnemy || TargetEnemy->IsDead())
	{
		UE_LOG(LogTemp, Log, TEXT("Projectile spawn skipped: no valid target."));
		if (OwnerCharacter)
		{
			OwnerCharacter->ClearFacingOverride();
		}
		return;
	}

	const FVector SpawnLocation = GetProjectileSpawnLocation();
	const FVector AimLocation = GetEnemyAimLocation(TargetEnemy);
	const float EffectiveProjectileSpeed = GetEffectiveProjectileSpeed();
	const float EffectiveAttackDamage = GetEffectiveAttackDamage();
	const float EffectiveHomingStrengthMultiplier = GetEffectiveHomingStrengthMultiplier();
	const int32 EffectiveProjectileCount = GetEffectiveProjectileCount();
	TArray<AEnemyBase*> TargetCandidates;
	FindEnemyTargetsSorted(TargetCandidates);
	if (TargetCandidates.Num() == 0)
	{
		TargetCandidates.Add(TargetEnemy);
	}

	FVector ProjectileDirection = AimLocation - SpawnLocation;
	ProjectileDirection.Z = 0.0f;

	if (!ProjectileDirection.Normalize())
	{
		UE_LOG(LogTemp, Warning, TEXT("Projectile spawn skipped: invalid projectile direction."));
		OwnerCharacter->ClearFacingOverride();
		return;
	}

	const float SpreadAngleDegrees = 8.0f;
	const float StartAngle = -0.5f * SpreadAngleDegrees * static_cast<float>(EffectiveProjectileCount - 1);
	TArray<AEnemyBase*> AssignedTargets;
	AssignedTargets.Reserve(EffectiveProjectileCount);
	TMap<AEnemyBase*, int32> TargetUseCounts;
	for (int32 ProjectileIndex = 0; ProjectileIndex < EffectiveProjectileCount; ++ProjectileIndex)
	{
		AEnemyBase* AssignedTarget = TargetCandidates[ProjectileIndex % TargetCandidates.Num()];
		AssignedTargets.Add(AssignedTarget);
		TargetUseCounts.FindOrAdd(AssignedTarget)++;
	}

	TMap<AEnemyBase*, int32> TargetUseIndices;
	for (int32 ProjectileIndex = 0; ProjectileIndex < EffectiveProjectileCount; ++ProjectileIndex)
	{
		const float Angle = StartAngle + SpreadAngleDegrees * static_cast<float>(ProjectileIndex);
		const FVector SpawnDirection = ProjectileDirection.RotateAngleAxis(Angle, FVector::UpVector).GetSafeNormal();
		AEnemyBase* AssignedTarget = AssignedTargets[ProjectileIndex];
		const int32 SharedTargetCount = TargetUseCounts.FindRef(AssignedTarget);
		const int32 SharedTargetIndex = TargetUseIndices.FindOrAdd(AssignedTarget)++;

		FVector HomingTargetOffset = FVector::ZeroVector;
		if (SharedTargetCount > 1 && SharedTargetHomingOffsetStep > 0.0f)
		{
			const float CenteredIndex = static_cast<float>(SharedTargetIndex) - 0.5f * static_cast<float>(SharedTargetCount - 1);
			float AllowedOffset = MaxSharedTargetHomingOffset;
			if (AssignedTarget)
			{
				if (const UCapsuleComponent* TargetCapsule = AssignedTarget->GetCapsuleComponent())
				{
					AllowedOffset = FMath::Min(AllowedOffset, TargetCapsule->GetScaledCapsuleRadius() * 0.65f);
				}
			}
			const float OffsetMagnitude = FMath::Clamp(CenteredIndex * SharedTargetHomingOffsetStep, -AllowedOffset, AllowedOffset);
			const FVector RightVector = FVector::CrossProduct(FVector::UpVector, SpawnDirection).GetSafeNormal();
			HomingTargetOffset = RightVector * OffsetMagnitude;
		}

		const float FanCenterDistance = EffectiveProjectileCount > 1
			? FMath::Abs(static_cast<float>(ProjectileIndex) - 0.5f * static_cast<float>(EffectiveProjectileCount - 1)) / (0.5f * static_cast<float>(EffectiveProjectileCount - 1))
			: 0.0f;
		const float ProjectileHomingStrengthMultiplier = EffectiveHomingStrengthMultiplier * (1.0f - OuterProjectileHomingStrengthReduction * FanCenterDistance);
		SpawnProjectileInstance(SpawnLocation, SpawnDirection, AssignedTarget, EffectiveAttackDamage, EffectiveProjectileSpeed, ProjectileHomingStrengthMultiplier, HomingTargetOffset);

		if (CVarHDLogNinjaProjectileSpread.GetValueOnGameThread() != 0)
		{
			UE_LOG(LogTemp, Log, TEXT("Ninja Attack Projectile %d/%d -> %s Offset=%s HomingStrengthMultiplier=%.2f"),
				ProjectileIndex + 1,
				EffectiveProjectileCount,
				*GetNameSafe(AssignedTarget),
				*HomingTargetOffset.ToString(),
				ProjectileHomingStrengthMultiplier);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[%.2f] Attack #%d Projectile Spawned: Target=%s SpawnLocation=%s Direction=%s ProjectileCount=%d"),
		GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0,
		ActiveAttackSequence,
		*GetNameSafe(TargetEnemy),
		*SpawnLocation.ToString(),
		*ProjectileDirection.ToString(),
		EffectiveProjectileCount);

	if (bDebugTargeting)
	{
		constexpr float DebugDuration = 1.5f;
		DrawDebugLine(GetWorld(), SpawnLocation, AimLocation, FColor::Yellow, false, DebugDuration, 0, 3.0f);
		DrawDebugSphere(GetWorld(), SpawnLocation, 24.0f, 12, FColor::Yellow, false, DebugDuration, 0, 3.0f);
	}

	CurrentAttackTarget.Reset();
	OwnerCharacter->ClearFacingOverride();
}

void UAutoAttackComponent::SpawnProjectileInstance(const FVector& SpawnLocation, const FVector& ProjectileDirection, AEnemyBase* TargetEnemy, float Damage, float Speed, float HomingStrengthMultiplier, const FVector& HomingTargetOffset)
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

	Projectile->InitializeProjectile(OwnerCharacter, ProjectileDirection, Damage, Speed, EProjectileTargetType::Enemies, TargetEnemy, HomingStrengthMultiplier, HomingTargetOffset);
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

	UE_LOG(LogTemp, Log, TEXT("[%.2f] Attack Timer Fired Owner=%s Component=%p"),
		GetWorld()->GetTimeSeconds(),
		*GetNameSafe(GetOwner()),
		this);
	UE_LOG(LogTemp, Log, TEXT("Attack Ready"));

	if (bIsAttacking)
	{
		UE_LOG(LogTemp, Log, TEXT("Attack Attempt Skipped: Already Attacking"));
		ScheduleNextAttackTimerFromCooldown();
		return;
	}

	if (!CanStartAttackNow())
	{
		UE_LOG(LogTemp, Log, TEXT("Attack Attempt Skipped: Cooldown"));
		const float Delay = FMath::Max(0.01f, static_cast<float>(NextAttackReadyTime - GetWorld()->GetTimeSeconds()));
		ScheduleNextAttackTimer(Delay);
		return;
	}

	StartTargetedAttack();
	ScheduleNextAttackTimerFromCooldown();
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

	UE_LOG(LogTemp, Log, TEXT("[%.2f] AutoAttack timer scheduled Owner=%s Component=%p Delay=%.3f TimerActive=%s"),
		World->GetTimeSeconds(),
		*GetNameSafe(GetOwner()),
		this,
		SafeDelay,
		World->GetTimerManager().IsTimerActive(AttackTimerHandle) ? TEXT("true") : TEXT("false"));
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

bool UAutoAttackComponent::PlayAttackMontage()
{
	if (!AttackMontage)
	{
		UE_LOG(LogTemp, Warning, TEXT("AttackMontage invalid"));
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("AttackMontage valid: %s"), *GetNameSafe(AttackMontage));

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

	UE_LOG(LogTemp, Log, TEXT("AnimInstance valid: %s"), *GetNameSafe(AnimInstance));

	const float ActualPlayRate = CalculateAttackMontagePlayRate(AttackMontage);
	const float MontageLength = AttackMontage->GetPlayLength();
	const float EffectiveAttackInterval = GetEffectiveAttackInterval();
	const float CalculatedPlayRate = EffectiveAttackInterval > KINDA_SMALL_NUMBER ? MontageLength / EffectiveAttackInterval : MaxAttackMontagePlayRate;
	UE_LOG(LogTemp, Log, TEXT("Attack Montage Rate: AttackInterval=%.3f MontageLength=%.3f CalculatedPlayRate=%.3f ActualPlayRate=%.3f"),
		EffectiveAttackInterval,
		MontageLength,
		CalculatedPlayRate,
		ActualPlayRate);

	const float PlayResult = AnimInstance->Montage_Play(AttackMontage, ActualPlayRate);
	UE_LOG(LogTemp, Log, TEXT("Attack montage play result: %.3f"), PlayResult);
	if (PlayResult <= 0.0f)
	{
		return false;
	}

	FOnMontageEnded MontageEndedDelegate;
	MontageEndedDelegate.BindUObject(this, &UAutoAttackComponent::HandleAttackMontageEnded);
	AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, AttackMontage);

	bIsAttacking = true;
	bAttackNotifyConsumed = false;
	LastAttackStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : LastAttackStartTime;
	AttackIntervalAtLastAttackStart = GetEffectiveAttackInterval();
	NextAttackReadyTime = LastAttackStartTime + AttackIntervalAtLastAttackStart;
	ActiveAttackSequence = ++AttackSequence;
	UE_LOG(LogTemp, Log, TEXT("[%.2f] Attack #%d Started"), GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0, ActiveAttackSequence);
	if (CVarHDLogAutoAttackCooldown.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[%s] Attack started FinalInterval=%.3f NextReadyTime=%.2f"),
			*GetNameSafe(GetOwner()),
			AttackIntervalAtLastAttackStart,
			NextAttackReadyTime);
	}
	return true;
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

void UAutoAttackComponent::HandleAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != AttackMontage)
	{
		return;
	}

	bIsAttacking = false;
	bAttackNotifyConsumed = false;
	CurrentAttackTarget.Reset();
	if (OwnerCharacter)
	{
		OwnerCharacter->ClearFacingOverride();
	}
	UE_LOG(LogTemp, Log, TEXT("[%.2f] Attack #%d Finished"), GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0, ActiveAttackSequence);
	ActiveAttackSequence = 0;
}

void UAutoAttackComponent::StartTargetedAttack()
{
	if (bIsAttacking)
	{
		UE_LOG(LogTemp, Log, TEXT("Attack Attempt Skipped: Already Attacking"));
		return;
	}

	if (!CanStartAttackNow())
	{
		UE_LOG(LogTemp, Log, TEXT("Attack Attempt Skipped: Cooldown"));
		return;
	}

	AEnemyBase* TargetEnemy = FindNearestEnemyTarget();
	if (!TargetEnemy)
	{
		UE_LOG(LogTemp, Log, TEXT("Auto attack skipped: no living enemy in range."));
		CurrentAttackTarget.Reset();
		if (OwnerCharacter)
		{
			OwnerCharacter->ClearFacingOverride();
		}
		return;
	}

	CurrentAttackTarget = TargetEnemy;
	const FVector AimLocation = GetEnemyAimLocation(TargetEnemy);
	OwnerCharacter->SetFacingOverrideTarget(AimLocation);

	UE_LOG(LogTemp, Log, TEXT("Auto attack target selected: %s Distance=%.2f"),
		*GetNameSafe(TargetEnemy),
		FVector::Dist2D(OwnerCharacter->GetActorLocation(), TargetEnemy->GetActorLocation()));

	if (!ProjectileClass)
	{
		UE_LOG(LogTemp, Log, TEXT("Samurai Auto Attack"));
	}

	if (PlayAttackMontage())
	{
		OnAutoAttack.Broadcast();
	}
}

void UAutoAttackComponent::StartProjectileAttack()
{
	StartTargetedAttack();
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

bool UAutoAttackComponent::TryConsumeAttackNotify()
{
	if (!bIsAttacking)
	{
		UE_LOG(LogTemp, Log, TEXT("Attack Notify Skipped: No Active Attack"));
		return false;
	}

	if (bAttackNotifyConsumed)
	{
		UE_LOG(LogTemp, Log, TEXT("Attack Notify Skipped: Already Consumed"));
		return false;
	}

	bAttackNotifyConsumed = true;
	return true;
}

float UAutoAttackComponent::GetEffectiveAttackInterval() const
{
	const UCharacterStatsComponent* CharacterStats = OwnerCharacter ? OwnerCharacter->GetCharacterStats() : nullptr;
	const float AttackSpeedMultiplier = CharacterStats ? CharacterStats->GetFinalAttackSpeedMultiplier() : 1.0f;
	return FMath::Max(0.01f, AttackInterval / FMath::Max(0.01f, AttackSpeedMultiplier));
}

float UAutoAttackComponent::GetEffectiveAttackDamage() const
{
	const UCharacterStatsComponent* CharacterStats = OwnerCharacter ? OwnerCharacter->GetCharacterStats() : nullptr;
	const float DamageMultiplier = CharacterStats ? CharacterStats->GetFinalDamageMultiplier() : 1.0f;
	const ASurvivorPlayerController* SurvivorController = Cast<ASurvivorPlayerController>(OwnerCharacter ? OwnerCharacter->GetOwner() : nullptr);
	const USharedPlayerStatsComponent* SharedStats = SurvivorController ? SurvivorController->GetSharedPlayerStats() : nullptr;
	const float GlobalDamageMultiplier = SharedStats ? SharedStats->GetFinalDamageMultiplier() : 1.0f;
	return AttackDamage * DamageMultiplier * GlobalDamageMultiplier;
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

float UAutoAttackComponent::GetEffectiveHomingStrengthMultiplier() const
{
	const UCharacterStatsComponent* CharacterStats = OwnerCharacter ? OwnerCharacter->GetCharacterStats() : nullptr;
	return CharacterStats ? CharacterStats->GetFinalHomingStrengthMultiplier() : 1.0f;
}

int32 UAutoAttackComponent::GetEffectiveProjectileCount() const
{
	const UCharacterStatsComponent* CharacterStats = OwnerCharacter ? OwnerCharacter->GetCharacterStats() : nullptr;
	return CharacterStats ? CharacterStats->GetFinalProjectileCount() : 1;
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
	TArray<AEnemyBase*> SortedTargets;
	FindEnemyTargetsSorted(SortedTargets);
	AEnemyBase* BestEnemy = SortedTargets.Num() > 0 ? SortedTargets[0] : nullptr;

	if (bDebugTargeting)
	{
		constexpr float DebugDuration = 1.5f;
		DrawDebugSphere(GetWorld(), OwnerCharacter->GetActorLocation(), TargetingRange, 48, FColor::Green, false, DebugDuration, 0, 2.0f);
		if (BestEnemy)
		{
			DrawDebugLine(GetWorld(), OwnerCharacter->GetActorLocation(), BestEnemy->GetActorLocation(), FColor::Green, false, DebugDuration, 0, 3.0f);
			UE_LOG(LogTemp, Log, TEXT("Targeting selected enemy: %s Distance=%.2f"),
				*GetNameSafe(BestEnemy),
				FVector::Dist2D(OwnerCharacter->GetActorLocation(), BestEnemy->GetActorLocation()));
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("Targeting found no living enemy in range %.2f."), TargetingRange);
		}
	}

	return BestEnemy;
}

void UAutoAttackComponent::FindEnemyTargetsSorted(TArray<AEnemyBase*>& OutTargets) const
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
		OwnerCharacter->GetActorLocation(),
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(TargetingRange),
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

	OutTargets.Sort([this](const AEnemyBase& Left, const AEnemyBase& Right)
	{
		return FVector::DistSquared2D(OwnerCharacter->GetActorLocation(), Left.GetActorLocation())
			< FVector::DistSquared2D(OwnerCharacter->GetActorLocation(), Right.GetActorLocation());
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
		&& OwnerCharacter->GetCharacterMode() == ECharacterMode::Active
		&& GetWorld();
}
