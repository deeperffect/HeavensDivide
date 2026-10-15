// Copyright Epic Games, Inc. All Rights Reserved.

#include "MeleeEnemyBase.h"
#include "EnemyAttackCue.h"
#include "TankMeleeEnemyBase.h"
#include "CombatAudio.h"

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

	if (IsTargetInAttackRange() && HasClearChaseRoute())
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
	// Elite slams already own their exact circle/rectangle warning.
	if (IsA<ATankMeleeEnemyBase>()) return;
	if (AttackCue.IsValid()) AttackCue->Destroy();
    AttackCue.Reset(); // Ordinary strikes communicate through their animation, not an AoE circle.
}

void AMeleeEnemyBase::HandleAttackFinished()
{
}

void AMeleeEnemyBase::ExecuteAttackHit()
{
	if (AttackCue.IsValid()) AttackCue->Impact();
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

	UCombatAudioLibrary::PlayEvent(this, TEXT("EnemySwing"), GetActorLocation());
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
	StartAttackLunge();
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
	// Present the peak of the wobble on the same frame as the hit, even at low frame rates.
	LungeStartTime = GetWorld()->GetTimeSeconds();
	UpdateAttackLunge();
	ExecuteAttackHit();
	if (!CanContinueBasicAttack() || BasicAttackPhase != EBasicAttackPhase::Recovery)
	{
		CancelBasicAttack();
		return;
	}
	GetWorldTimerManager().SetTimer(RecoveryTimer, this, &AMeleeEnemyBase::FinishAttackRecovery,
		FMath::Max(0.001f, AttackRecovery), false);
}

void AMeleeEnemyBase::StartAttackLunge()
{
	if (!bUseProceduralAttackMotion || !GetMesh() || GetNetMode() == NM_DedicatedServer
		|| (AttackLungeDistance <= 0.0f && AttackWobbleAngle <= 0.0f)) return;
	PreAttackMeshLocation = GetMesh()->GetRelativeLocation();
	PreAttackMeshRotation = GetMesh()->GetRelativeRotation().Quaternion();
	const USceneComponent* Parent = GetMesh()->GetAttachParent();
	LungeRelativeDirection = Parent ? Parent->GetComponentTransform().InverseTransformVector(GetActorForwardVector()) : GetActorForwardVector();
	WobbleRelativeAxis = Parent ? Parent->GetComponentTransform().InverseTransformVectorNoScale(GetActorRightVector()) : GetActorRightVector();
	LungeStartTime = GetWorld()->GetTimeSeconds();
	bLungeActive = true;
	GetWorldTimerManager().SetTimer(LungeTimer, this, &AMeleeEnemyBase::UpdateAttackLunge, 1.0f / 60.0f, true);
}

void AMeleeEnemyBase::UpdateAttackLunge()
{
	if (!bLungeActive) return;
	if (!CanContinueBasicAttack()) { CancelBasicAttack(); return; }
	const float Elapsed = static_cast<float>(GetWorld()->GetTimeSeconds() - LungeStartTime);
	float Lean = 0.0f;
	float Rock = 0.0f;
	if (BasicAttackPhase == EBasicAttackPhase::Windup)
	{
		const float Windup = FMath::Max(0.001f, AttackWindup);
		const float Alpha = FMath::Clamp(Elapsed / Windup, 0.0f, 1.0f);
		// Anticipate backwards, then swing forward over the final part of the windup.
		const float ReleaseDuration = FMath::Clamp(AttackLungeOutDuration, 0.001f, Windup);
		const float Release = FMath::SmoothStep(Windup - ReleaseDuration, Windup, Elapsed);
		Lean = FMath::Lerp(-0.35f * FMath::SmoothStep(0.0f, 0.5f, Alpha), 1.0f, Release);
		Rock = FMath::Sin(Alpha * 2.0f * PI) * 0.25f * (1.0f - Release);
	}
	else
	{
		// One small rebound, fitted inside recovery. No per-frame actor tick is needed.
		const float Duration = FMath::Min(FMath::Max(0.001f, AttackRecovery),
			FMath::Max(0.001f, AttackLungeOutDuration + AttackLungeReturnDuration));
		if (Elapsed >= Duration) { ResetAttackLunge(); return; }
		const float Alpha = FMath::Clamp(Elapsed / Duration, 0.0f, 1.0f);
		Lean = FMath::Cos(Alpha * 1.5f * PI) * FMath::Square(1.0f - Alpha);
	}
	if (GetMesh())
	{
		const float Angle = FMath::DegreesToRadians(FMath::Clamp(AttackWobbleAngle, 0.0f, 30.0f));
		const FQuat Rotation = FQuat(WobbleRelativeAxis.GetSafeNormal(), Angle * Lean)
			* FQuat(LungeRelativeDirection.GetSafeNormal(), Angle * Rock) * PreAttackMeshRotation;
		GetMesh()->SetRelativeLocationAndRotation(
			PreAttackMeshLocation + LungeRelativeDirection * FMath::Max(0.0f, AttackLungeDistance) * Lean, Rotation);
	}
}

void AMeleeEnemyBase::ResetAttackLunge()
{
	GetWorldTimerManager().ClearTimer(LungeTimer);
	if (bLungeActive && GetMesh()) GetMesh()->SetRelativeLocationAndRotation(PreAttackMeshLocation, PreAttackMeshRotation);
	bLungeActive = false;
}

void AMeleeEnemyBase::CancelBasicAttack()
{
	if (BasicAttackPhase == EBasicAttackPhase::Windup && AttackCue.IsValid()) AttackCue->Destroy();
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
