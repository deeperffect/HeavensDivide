// Copyright Epic Games, Inc. All Rights Reserved.

#include "MeleeEnemyBase.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "DrawDebugHelpers.h"
#include "HealthComponent.h"
#include "Kismet/GameplayStatics.h"
#include "SurvivorPlayerController.h"

void AMeleeEnemyBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAttackTimer();

	Super::EndPlay(EndPlayReason);
}

void AMeleeEnemyBase::UpdateEnemyBehavior(float DeltaSeconds)
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

bool AMeleeEnemyBase::ShouldSkipMovement() const
{
	return bIsAttacking;
}

void AMeleeEnemyBase::StopEnemyBehavior()
{
	bIsAttacking = false;
	StopEnemyMovement();
	StopAttackTimer();
}

void AMeleeEnemyBase::HandlePlayerCharacterSwapped(ACharacterBase* OldCharacter, ACharacterBase* NewCharacter)
{
	Super::HandlePlayerCharacterSwapped(OldCharacter, NewCharacter);

	if (bIsAttacking)
	{
		UE_LOG(LogTemp, Log, TEXT("Character Swapped During Attack"));
		UE_LOG(LogTemp, Log, TEXT("New CurrentTarget = %s"), *GetNameSafe(CurrentTarget));
		UE_LOG(LogTemp, Log, TEXT("Existing Attack Continues"));
	}
}

void AMeleeEnemyBase::HandleDeath()
{
	bIsAttacking = false;
	StopAttackTimer();

	Super::HandleDeath();
}

bool AMeleeEnemyBase::ShouldForceHighAnimationBudgetSignificance() const
{
	return bIsAttacking;
}

bool AMeleeEnemyBase::IsTargetInAttackRange() const
{
	if (!CurrentTarget)
	{
		return false;
	}

	const FVector ToTarget = CurrentTarget->GetActorLocation() - GetActorLocation();
	return ToTarget.SizeSquared2D() <= FMath::Square(AttackRange);
}

void AMeleeEnemyBase::StartAttackTimer()
{
	if (bIsDead || IsPlayerTargetDead() || !GetWorld() || GetWorld()->GetTimerManager().IsTimerActive(AttackTimerHandle))
	{
		return;
	}

	GetWorld()->GetTimerManager().SetTimer(
		AttackTimerHandle,
		this,
		&AMeleeEnemyBase::HandleAttackTimer,
		AttackInterval,
		true,
		0.0f);
}

void AMeleeEnemyBase::StopAttackTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AttackTimerHandle);
	}
}

void AMeleeEnemyBase::HandleAttackTimer()
{
	if (bIsAttacking)
	{
		return;
	}

	StartAttack();
}

void AMeleeEnemyBase::StartAttack()
{
	if (bIsDead || IsPlayerTargetDead() || bIsAttacking || !CurrentTarget || !IsTargetInAttackRange())
	{
		StopAttackTimer();
		return;
	}

	FaceTarget();
	UE_LOG(LogTemp, Log, TEXT("Enemy Attack Started"));
	UE_LOG(LogTemp, Log, TEXT("Target at Start = %s"), *GetNameSafe(CurrentTarget));

	if (!AttackMontage)
	{
		UE_LOG(LogTemp, Warning, TEXT("Enemy attack montage invalid: %s"), *GetNameSafe(this));
		return;
	}

	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("Enemy attack AnimInstance invalid: %s"), *GetNameSafe(this));
		return;
	}

	const float PlayResult = AnimInstance->Montage_Play(AttackMontage);
	UE_LOG(LogTemp, Log, TEXT("Enemy attack montage plays: %s Result=%.3f"), *GetNameSafe(AttackMontage), PlayResult);

	if (PlayResult <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("Enemy attack montage failed to play: %s"), *GetNameSafe(this));
		return;
	}

	bIsAttacking = true;
	UpdateAnimationBudgetSignificance();

	FOnMontageEnded MontageEndedDelegate;
	MontageEndedDelegate.BindUObject(this, &AMeleeEnemyBase::HandleAttackMontageEnded);
	AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, AttackMontage);
}

void AMeleeEnemyBase::PerformAttackHit()
{
	UE_LOG(LogTemp, Log, TEXT("Enemy Attack Impact"));

	if (bIsDead || IsPlayerTargetDead() || !ObservedCharacterManager)
	{
		return;
	}

	ACharacterBase* ActivePlayerCharacter = ObservedCharacterManager->GetActiveCharacter();
	if (!ActivePlayerCharacter)
	{
		return;
	}

	FVector AttackForward = GetActorForwardVector();
	AttackForward.Z = 0.0f;
	if (!AttackForward.Normalize())
	{
		return;
	}

	const FVector HitCenter = GetActorLocation() + AttackForward * AttackHitForwardOffset;
	const float DistanceSquaredToActivePlayer = FVector::DistSquared2D(HitCenter, ActivePlayerCharacter->GetActorLocation());
	const bool bPlayerInHitArea = DistanceSquaredToActivePlayer <= FMath::Square(AttackHitRadius);

	if (bDebugAttackHit && GetWorld())
	{
		const FColor DebugColor = bPlayerInHitArea ? FColor::Red : FColor::Silver;
		constexpr float DebugDuration = 1.5f;
		DrawDebugLine(GetWorld(), GetActorLocation(), HitCenter, DebugColor, false, DebugDuration, 0, 3.0f);
		DrawDebugSphere(GetWorld(), HitCenter, AttackHitRadius, 24, DebugColor, false, DebugDuration, 0, 3.0f);
	}

	if (!bPlayerInHitArea)
	{
		UE_LOG(LogTemp, Log, TEXT("Enemy attack missed: ActiveCharacter=%s"), *GetNameSafe(ActivePlayerCharacter));
		return;
	}

	ASurvivorPlayerController* SurvivorController = Cast<ASurvivorPlayerController>(ActivePlayerCharacter->GetController());
	if (!SurvivorController)
	{
		SurvivorController = Cast<ASurvivorPlayerController>(ActivePlayerCharacter->GetOwner());
	}

	UHealthComponent* TargetHealth = SurvivorController ? SurvivorController->GetPlayerHealthComponent() : nullptr;
	if (!TargetHealth || TargetHealth->IsDead())
	{
		return;
	}

	TargetHealth->ApplyDamage(AttackDamage);
	UE_LOG(LogTemp, Log, TEXT("Player receives damage: Target=%s Damage=%.2f RemainingHealth=%.2f"),
		*GetNameSafe(ActivePlayerCharacter),
		AttackDamage,
		TargetHealth->GetCurrentHealth());
}

void AMeleeEnemyBase::HandleAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != AttackMontage)
	{
		return;
	}

	bIsAttacking = false;
	UpdateAnimationBudgetSignificance();
	UE_LOG(LogTemp, Log, TEXT("Enemy Attack Finished"));
}
