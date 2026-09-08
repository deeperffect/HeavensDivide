// Copyright Epic Games, Inc. All Rights Reserved.

#include "MeleeEnemyBase.h"

#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "DrawDebugHelpers.h"
#include "HealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "TimerManager.h"
#include "SurvivorPlayerController.h"

AMeleeEnemyBase::AMeleeEnemyBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void AMeleeEnemyBase::ApplySpawnDifficultyScaling(float HealthMultiplier, float DamageMultiplier)
{
	Super::ApplySpawnDifficultyScaling(HealthMultiplier, DamageMultiplier);
	AttackDamage *= FMath::Max(0.0f, DamageMultiplier);
}

void AMeleeEnemyBase::ApplySpawnInstanceModifiers(float HealthMultiplier, float DamageMultiplier, float MovementSpeedMultiplier)
{
	Super::ApplySpawnInstanceModifiers(HealthMultiplier, DamageMultiplier, MovementSpeedMultiplier);
	AttackDamage *= FMath::Max(0.0f, DamageMultiplier);
}

void AMeleeEnemyBase::CapturePreBloodboundState()
{
	Super::CapturePreBloodboundState();
	PreBloodboundAttackDamage = AttackDamage;
}

void AMeleeEnemyBase::RestorePreBloodboundState()
{
	Super::RestorePreBloodboundState();
	AttackDamage = PreBloodboundAttackDamage;
}

void AMeleeEnemyBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelBasicAttack();
	StopAttackTimer();

	Super::EndPlay(EndPlayReason);
}

void AMeleeEnemyBase::UpdateEnemyBehavior(float DeltaSeconds)
{
	if (IsStressTestCombatDisabled())
	{
		CancelBasicAttack();
		AEnemyBase::UpdateEnemyBehavior(DeltaSeconds);
		return;
	}

	if (bIsDead || bGameplaySuspended || IsPlayerTargetDead())
	{
		CancelBasicAttack();
		StopAttackTimer();
		StopEnemyMovement();
		return;
	}

	if (!EnsureTargetFromCharacterManager())
	{
		CancelBasicAttack();
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
	CancelBasicAttack();
	bIsAttacking = false;
	StopEnemyMovement();
	StopAttackTimer();
}

void AMeleeEnemyBase::HandlePlayerCharacterSwapped(ACharacterBase* OldCharacter, ACharacterBase* NewCharacter)
{
	Super::HandlePlayerCharacterSwapped(OldCharacter, NewCharacter);

	if (BasicAttackPhase != EBasicAttackPhase::None) CancelBasicAttack();
}

void AMeleeEnemyBase::HandleDeath()
{
	CancelBasicAttack();
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
	if (!IsValid(CurrentTarget))
	{
		return false;
	}

	const FVector ToTarget = CurrentTarget->GetActorLocation() - GetActorLocation();
	return ToTarget.SizeSquared2D() <= FMath::Square(AttackRange);
}

void AMeleeEnemyBase::StartAttackTimer()
{
	if (bIsDead || bGameplaySuspended || IsPlayerTargetDead() || !GetWorld() || GetWorld()->GetTimerManager().IsTimerActive(AttackTimerHandle))
	{
		return;
	}

	GetWorld()->GetTimerManager().SetTimer(
		AttackTimerHandle,
		this,
		&AMeleeEnemyBase::HandleAttackTimer,
		GetAttackTimerDelay(),
		false);
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

bool AMeleeEnemyBase::CanStartAttackNow() const
{
	const UWorld* World = GetWorld();
	return !World || World->GetTimeSeconds() >= NextAttackStartTime;
}

float AMeleeEnemyBase::GetAttackTimerDelay() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.01f;
	}

	const float RemainingTime = static_cast<float>(NextAttackStartTime - World->GetTimeSeconds());
	return FMath::Max(0.01f, RemainingTime);
}

void AMeleeEnemyBase::MarkAttackStarted()
{
	if (const UWorld* World = GetWorld())
	{
		NextAttackStartTime = World->GetTimeSeconds() + FMath::Max(0.01f, AttackInterval);
	}
}

void AMeleeEnemyBase::HandleAttackCommitted()
{
}

void AMeleeEnemyBase::HandleAttackFinished()
{
}

void AMeleeEnemyBase::ExecuteAttackHit()
{
	if (IsStressTestCombatDisabled())
	{
		return;
	}

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
	// Exact sphere/capsule overlap against only the active player: no crowd-wide
	// physics query or dependency on overlap-event flags. Reject targets behind us.
	const UCapsuleComponent* Capsule = ActivePlayerCharacter->GetCapsuleComponent();
	if (!Capsule || AttackHitRadius <= 0.0f) return;
	const float SegmentHalfLength = FMath::Max(0.0f, Capsule->GetScaledCapsuleHalfHeight() - Capsule->GetScaledCapsuleRadius());
	const FVector CapsuleCenter = Capsule->GetComponentLocation();
	const FVector CapsuleAxis = Capsule->GetUpVector() * SegmentHalfLength;
	const bool bPlayerInHitArea = FVector::DotProduct(ActivePlayerCharacter->GetActorLocation() - GetActorLocation(), AttackForward) >= 0.0f
		&& FMath::PointDistToSegmentSquared(HitCenter, CapsuleCenter - CapsuleAxis, CapsuleCenter + CapsuleAxis)
		<= FMath::Square(AttackHitRadius + Capsule->GetScaledCapsuleRadius());

#if ENABLE_DRAW_DEBUG
	if (bDebugAttackHit && GetWorld())
	{
		const FColor DebugColor = bPlayerInHitArea ? FColor::Red : FColor::Silver;
		constexpr float DebugDuration = 1.5f;
		DrawDebugLine(GetWorld(), GetActorLocation(), HitCenter, DebugColor, false, DebugDuration, 0, 3.0f);
		DrawDebugSphere(GetWorld(), HitCenter, AttackHitRadius, 24, DebugColor, false, DebugDuration, 0, 3.0f);
	}

#endif
	if (!bPlayerInHitArea)
	{
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

	SurvivorController->ApplyDamageToPlayer(AttackDamage);
}


void AMeleeEnemyBase::SetTarget(AActor* NewTarget)
{
	if (BasicAttackPhase != EBasicAttackPhase::None && NewTarget != CurrentTarget) CancelBasicAttack();
	Super::SetTarget(NewTarget);
}

bool AMeleeEnemyBase::CanContinueBasicAttack() const
{
	return bIsAttacking && !bIsDead && !bGameplaySuspended && !IsActorBeingDestroyed()
		&& !IsStressTestCombatDisabled() && !IsPlayerTargetDead()
		&& BasicAttackTarget.IsValid() && BasicAttackTarget.Get() == CurrentTarget;
}

void AMeleeEnemyBase::StartAttack()
{
	if (!GetWorld() || bIsDead || bGameplaySuspended || bIsAttacking || IsActorBeingDestroyed()
		|| IsStressTestCombatDisabled() || IsPlayerTargetDead() || !IsTargetInAttackRange()) return;
	if (!CanStartAttackNow()) { StartAttackTimer(); return; }
	StopAttackTimer();
	StopEnemyMovement();
	FaceTarget();
	bIsAttacking = true;
	BasicAttackPhase = EBasicAttackPhase::Windup;
	BasicAttackTarget = CurrentTarget;
	MarkAttackStarted();
	UpdateAnimationBudgetSignificance();
	HandleAttackCommitted();
	GetWorldTimerManager().SetTimer(WindupTimer, this, &AMeleeEnemyBase::FinishAttackWindup,
		FMath::Max(0.001f, AttackWindup), false);
}

void AMeleeEnemyBase::FinishAttackWindup()
{
	if (BasicAttackPhase != EBasicAttackPhase::Windup) return;
	if (!CanContinueBasicAttack()) { CancelBasicAttack(); return; }
	// Advance before applying damage so reentrant callbacks cannot hit twice.
	BasicAttackPhase = EBasicAttackPhase::Recovery;
	StopEnemyMovement();
	ExecuteAttackHit();
	if (!CanContinueBasicAttack() || BasicAttackPhase != EBasicAttackPhase::Recovery)
	{
		CancelBasicAttack();
		return;
	}
	if (bUseProceduralAttackMotion && GetMesh() && AttackLungeDistance > 0.0f && GetNetMode() != NM_DedicatedServer)
	{
		PreAttackMeshLocation = GetMesh()->GetRelativeLocation();
		const USceneComponent* Parent = GetMesh()->GetAttachParent();
		LungeRelativeDirection = Parent ? Parent->GetComponentTransform().InverseTransformVector(GetActorForwardVector()) : GetActorForwardVector();
		LungeStartTime = GetWorld()->GetTimeSeconds();
		bLungeActive = true;
		GetWorldTimerManager().SetTimer(LungeTimer, this, &AMeleeEnemyBase::UpdateAttackLunge, 1.0f / 60.0f, true);
	}
	GetWorldTimerManager().SetTimer(RecoveryTimer, this, &AMeleeEnemyBase::FinishAttackRecovery,
		FMath::Max(0.001f, AttackRecovery), false);
}

void AMeleeEnemyBase::UpdateAttackLunge()
{
	if (!bLungeActive) return;
	if (!CanContinueBasicAttack()) { CancelBasicAttack(); return; }
	const float Elapsed = static_cast<float>(GetWorld()->GetTimeSeconds() - LungeStartTime);
	// Fit the presentation inside recovery, even when designers shorten recovery.
	const float Out = FMath::Max(0.001f, AttackLungeOutDuration);
	const float Back = FMath::Max(0.001f, AttackLungeReturnDuration);
	const float Fit = FMath::Min(1.0f, FMath::Max(0.001f, AttackRecovery) / (Out + Back));
	if (Elapsed >= (Out + Back) * Fit) { ResetAttackLunge(); return; }
	const float Alpha = Elapsed < Out * Fit ? Elapsed / (Out * Fit) : 1.0f - (Elapsed - Out * Fit) / (Back * Fit);
	const float Smooth = FMath::SmoothStep(0.0f, 1.0f, FMath::Clamp(Alpha, 0.0f, 1.0f));
	if (GetMesh()) GetMesh()->SetRelativeLocation(PreAttackMeshLocation + LungeRelativeDirection * AttackLungeDistance * Smooth);
}

void AMeleeEnemyBase::ResetAttackLunge()
{
	GetWorldTimerManager().ClearTimer(LungeTimer);
	if (bLungeActive && GetMesh()) GetMesh()->SetRelativeLocation(PreAttackMeshLocation);
	bLungeActive = false;
}

void AMeleeEnemyBase::CancelBasicAttack()
{
	GetWorldTimerManager().ClearTimer(WindupTimer);
	GetWorldTimerManager().ClearTimer(RecoveryTimer);
	ResetAttackLunge();
	if (BasicAttackPhase == EBasicAttackPhase::None) return;
	BasicAttackPhase = EBasicAttackPhase::None;
	BasicAttackTarget.Reset();
	bIsAttacking = false;
	StopAttackTimer();
	UpdateAnimationBudgetSignificance();
}

void AMeleeEnemyBase::FinishAttackRecovery()
{
	if (BasicAttackPhase != EBasicAttackPhase::Recovery) return;
	const bool bResume = CanContinueBasicAttack();
	CancelBasicAttack();
	HandleAttackFinished();
	if (bResume) UpdateEnemyBehavior(0.0f);
}
