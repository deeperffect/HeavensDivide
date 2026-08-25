// Copyright Epic Games, Inc. All Rights Reserved.

#include "RangedEnemyBase.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AttackProjectileBase.h"
#include "CharacterBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "TimerManager.h"

void ARangedEnemyBase::ApplySpawnDifficultyScaling(float HealthMultiplier, float DamageMultiplier)
{
	Super::ApplySpawnDifficultyScaling(HealthMultiplier, DamageMultiplier);
	AttackDamage *= FMath::Max(0.0f, DamageMultiplier);
}

void ARangedEnemyBase::ApplySpawnInstanceModifiers(float HealthMultiplier, float DamageMultiplier, float MovementSpeedMultiplier)
{
	Super::ApplySpawnInstanceModifiers(HealthMultiplier, DamageMultiplier, MovementSpeedMultiplier);
	AttackDamage *= FMath::Max(0.0f, DamageMultiplier);
}

void ARangedEnemyBase::CapturePreBloodboundState()
{
	Super::CapturePreBloodboundState();
	PreBloodboundAttackDamage = AttackDamage;
}

void ARangedEnemyBase::RestorePreBloodboundState()
{
	Super::RestorePreBloodboundState();
	AttackDamage = PreBloodboundAttackDamage;
}

void ARangedEnemyBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAttackTimer();

	Super::EndPlay(EndPlayReason);
}

void ARangedEnemyBase::UpdateEnemyBehavior(float DeltaSeconds)
{
	if (IsStressTestCombatDisabled())
	{
		AEnemyBase::UpdateEnemyBehavior(DeltaSeconds);
		return;
	}

	if (bIsDead || IsPlayerTargetDead())
	{
		StopAttackTimer();
		StopEnemyMovement();
		return;
	}

	if (!EnsureTargetFromCharacterManager())
	{
		StopAttackTimer();
		StopEnemyMovement();
		return;
	}

	if (bIsAttacking)
	{
		StopEnemyMovement();
		return;
	}

	if (IsTargetInAttackRange())
	{
		StopEnemyMovement();
		FaceTarget();
		StartAttackTimer();
		return;
	}

	StopAttackTimer();
	MoveTowardCurrentTarget();
}

bool ARangedEnemyBase::ShouldSkipMovement() const
{
	return bIsAttacking;
}

void ARangedEnemyBase::StopEnemyBehavior()
{
	bIsAttacking = false;
	StopEnemyMovement();
	StopAttackTimer();
}

void ARangedEnemyBase::HandlePlayerCharacterSwapped(ACharacterBase* OldCharacter, ACharacterBase* NewCharacter)
{
	Super::HandlePlayerCharacterSwapped(OldCharacter, NewCharacter);

	if (bIsAttacking)
	{
	}
}

void ARangedEnemyBase::HandleDeath()
{
	bIsAttacking = false;
	StopAttackTimer();

	Super::HandleDeath();
}

bool ARangedEnemyBase::ShouldForceHighAnimationBudgetSignificance() const
{
	return bIsAttacking;
}

bool ARangedEnemyBase::IsTargetInAttackRange() const
{
	if (!CurrentTarget)
	{
		return false;
	}

	const FVector ToTarget = CurrentTarget->GetActorLocation() - GetActorLocation();
	return ToTarget.SizeSquared2D() <= FMath::Square(AttackRange);
}

void ARangedEnemyBase::StartAttackTimer()
{
	if (bIsDead || IsPlayerTargetDead() || !GetWorld() || GetWorld()->GetTimerManager().IsTimerActive(AttackTimerHandle))
	{
		return;
	}

	GetWorld()->GetTimerManager().SetTimer(
		AttackTimerHandle,
		this,
		&ARangedEnemyBase::HandleAttackTimer,
		GetAttackTimerDelay(),
		false);
}

void ARangedEnemyBase::StopAttackTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AttackTimerHandle);
	}
}

void ARangedEnemyBase::HandleAttackTimer()
{
	if (bIsAttacking)
	{
		return;
	}

	StartAttack();
}

bool ARangedEnemyBase::CanStartAttackNow() const
{
	const UWorld* World = GetWorld();
	return !World || World->GetTimeSeconds() >= NextAttackStartTime;
}

float ARangedEnemyBase::GetAttackTimerDelay() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.01f;
	}

	const float RemainingTime = static_cast<float>(NextAttackStartTime - World->GetTimeSeconds());
	return FMath::Max(0.01f, RemainingTime);
}

void ARangedEnemyBase::MarkAttackStarted()
{
	if (const UWorld* World = GetWorld())
	{
		NextAttackStartTime = World->GetTimeSeconds() + FMath::Max(0.01f, AttackInterval);
	}
}

void ARangedEnemyBase::StartAttack()
{
	if (IsStressTestCombatDisabled())
	{
		StopAttackTimer();
		return;
	}

	if (bIsDead || IsPlayerTargetDead() || bIsAttacking || !CurrentTarget || !IsTargetInAttackRange())
	{
		StopAttackTimer();
		return;
	}

	if (!CanStartAttackNow())
	{
		StartAttackTimer();
		return;
	}

	FaceTarget();

	if (!AttackMontage)
	{
		UE_LOG(LogTemp, Warning, TEXT("Enemy ranged attack montage invalid: %s"), *GetNameSafe(this));
		return;
	}

	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("Enemy ranged attack AnimInstance invalid: %s"), *GetNameSafe(this));
		return;
	}

	const float PlayResult = AnimInstance->Montage_Play(AttackMontage);

	if (PlayResult <= 0.0f)
	{
		return;
	}

	bIsAttacking = true;
	MarkAttackStarted();
	UpdateAnimationBudgetSignificance();

	FOnMontageEnded MontageEndedDelegate;
	MontageEndedDelegate.BindUObject(this, &ARangedEnemyBase::HandleAttackMontageEnded);
	AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, AttackMontage);
}

void ARangedEnemyBase::SpawnAttackProjectile()
{
	if (IsStressTestCombatDisabled())
	{
		return;
	}

	if (bIsDead || IsPlayerTargetDead() || !CurrentTarget || !ProjectileClass || !GetWorld())
	{
		return;
	}

	const FVector SpawnLocation = GetProjectileSpawnLocation();
	FVector Direction = CurrentTarget->GetActorLocation() - SpawnLocation;
	Direction.Z = 0.0f;
	if (!Direction.Normalize())
	{
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.Instigator = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AAttackProjectileBase* Projectile = GetWorld()->SpawnActor<AAttackProjectileBase>(
		ProjectileClass,
		SpawnLocation,
		Direction.Rotation(),
		SpawnParameters);

	if (!Projectile)
	{
		return;
	}

	Projectile->InitializeProjectile(this, Direction, AttackDamage, ProjectileSpeed, EProjectileTargetType::ActivePlayer);
}

void ARangedEnemyBase::HandleAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != AttackMontage)
	{
		return;
	}

	bIsAttacking = false;
	UpdateAnimationBudgetSignificance();
	if (!bInterrupted && !bIsDead && !IsPlayerTargetDead() && CurrentTarget && IsTargetInAttackRange())
	{
		StartAttackTimer();
	}
}

FVector ARangedEnemyBase::GetProjectileSpawnLocation() const
{
	const USkeletalMeshComponent* MeshComponent = GetMesh();
	if (MeshComponent && ProjectileSpawnSocket != NAME_None && MeshComponent->DoesSocketExist(ProjectileSpawnSocket))
	{
		return MeshComponent->GetSocketLocation(ProjectileSpawnSocket);
	}

	return GetActorLocation()
		+ GetActorForwardVector() * ProjectileSpawnOffset.X
		+ GetActorRightVector() * ProjectileSpawnOffset.Y
		+ FVector::UpVector * ProjectileSpawnOffset.Z;
}
