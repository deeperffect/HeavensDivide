// Copyright Epic Games, Inc. All Rights Reserved.

#include "RangedEnemyBase.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AttackProjectileBase.h"
#include "CharacterBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "TimerManager.h"

void ARangedEnemyBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAttackTimer();

	Super::EndPlay(EndPlayReason);
}

void ARangedEnemyBase::UpdateEnemyBehavior(float DeltaSeconds)
{
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
		UE_LOG(LogTemp, Log, TEXT("Character Swapped During Ranged Attack"));
		UE_LOG(LogTemp, Log, TEXT("New CurrentTarget = %s"), *GetNameSafe(CurrentTarget));
		UE_LOG(LogTemp, Log, TEXT("Existing Ranged Attack Continues"));
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
		AttackInterval,
		true,
		0.0f);
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

void ARangedEnemyBase::StartAttack()
{
	if (bIsDead || IsPlayerTargetDead() || bIsAttacking || !CurrentTarget || !IsTargetInAttackRange())
	{
		StopAttackTimer();
		return;
	}

	FaceTarget();
	UE_LOG(LogTemp, Log, TEXT("Enemy Ranged Attack Started"));
	UE_LOG(LogTemp, Log, TEXT("Target at Start = %s"), *GetNameSafe(CurrentTarget));

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
	UE_LOG(LogTemp, Log, TEXT("Enemy ranged attack montage plays: %s Result=%.3f"), *GetNameSafe(AttackMontage), PlayResult);

	if (PlayResult <= 0.0f)
	{
		return;
	}

	bIsAttacking = true;
	UpdateAnimationBudgetSignificance();

	FOnMontageEnded MontageEndedDelegate;
	MontageEndedDelegate.BindUObject(this, &ARangedEnemyBase::HandleAttackMontageEnded);
	AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, AttackMontage);
}

void ARangedEnemyBase::SpawnAttackProjectile()
{
	UE_LOG(LogTemp, Log, TEXT("Enemy Ranged Projectile Release"));

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
	UE_LOG(LogTemp, Log, TEXT("Enemy projectile spawned: Target=%s SpawnLocation=%s Direction=%s"),
		*GetNameSafe(CurrentTarget),
		*SpawnLocation.ToString(),
		*Direction.ToString());
}

void ARangedEnemyBase::HandleAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != AttackMontage)
	{
		return;
	}

	bIsAttacking = false;
	UpdateAnimationBudgetSignificance();
	UE_LOG(LogTemp, Log, TEXT("Enemy Ranged Attack Finished"));
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
