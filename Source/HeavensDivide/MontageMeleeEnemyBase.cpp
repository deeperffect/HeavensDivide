#include "MontageMeleeEnemyBase.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"

void AMontageMeleeEnemyBase::StartAttack()
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

	if (PlayResult <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("Enemy attack montage failed to play: %s"), *GetNameSafe(this));
		return;
	}

	bIsAttacking = true;
	MarkAttackStarted();
	UpdateAnimationBudgetSignificance();
	HandleAttackCommitted();

	FOnMontageEnded MontageEndedDelegate;
	MontageEndedDelegate.BindUObject(this, &AMontageMeleeEnemyBase::HandleAttackMontageEnded);
	AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, AttackMontage);
}

void AMontageMeleeEnemyBase::PerformAttackHit()
{
	if (IsStressTestCombatDisabled())
	{
		return;
	}

	ExecuteAttackHit();
}

void AMontageMeleeEnemyBase::HandleAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != AttackMontage)
	{
		return;
	}

	bIsAttacking = false;
	UpdateAnimationBudgetSignificance();
	HandleAttackFinished();
	if (!bInterrupted && !bIsDead && !IsPlayerTargetDead() && CurrentTarget && IsTargetInAttackRange())
	{
		StartAttackTimer();
	}
}
