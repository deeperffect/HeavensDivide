// Copyright Epic Games, Inc. All Rights Reserved.

#include "AutoAttackComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AttackProjectileBase.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "EnemyBase.h"
#include "GameFramework/Character.h"
#include "HealthComponent.h"
#include "TimerManager.h"
#include "Engine/OverlapResult.h"

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

	ScheduleNextAttackTimer(AttackInterval);
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
			AttackInterval);
		ScheduleNextAttackTimer(AttackInterval);
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

	UE_LOG(LogTemp, Log, TEXT("AutoAttack trace origin: %s"), *AttackOrigin.ToString());
	UE_LOG(LogTemp, Log, TEXT("AutoAttack trace direction: %s"), *AttackForward.ToString());
	UE_LOG(LogTemp, Log, TEXT("AutoAttack trace sphere center: %s"), *HitboxCenter.ToString());
	UE_LOG(LogTemp, Log, TEXT("AutoAttack trace AttackRadius: %.2f AttackRange: %.2f AttackForwardOffset: %.2f"),
		AttackRadius,
		AttackRange,
		AttackForwardOffset);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AutoAttackTrace), false, OwnerCharacter);
	QueryParams.AddIgnoredActor(OwnerCharacter);

	TArray<FHitResult> HitResults;
	const FCollisionShape TraceShape = FCollisionShape::MakeSphere(AttackRadius);
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
		EnemyHealth->ApplyDamage(AttackDamage);
		UE_LOG(LogTemp, Log, TEXT("AutoAttack damaged enemy: %s Damage=%.2f RemainingHealth=%.2f"),
			*GetNameSafe(HitEnemy),
			AttackDamage,
			EnemyHealth->GetCurrentHealth());
	}

	if (bDebugAttackTrace)
	{
		const FColor DebugColor = DamagedEnemies.Num() > 0 ? FColor::Red : FColor::Cyan;
		constexpr float DebugDuration = 1.5f;
		DrawDebugLine(GetWorld(), AttackOrigin, HitboxCenter, DebugColor, false, DebugDuration, 0, 4.0f);
		DrawDebugSphere(GetWorld(), HitboxCenter, AttackRadius, 24, DebugColor, false, DebugDuration, 0, 4.0f);
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
	FVector ProjectileDirection = AimLocation - SpawnLocation;
	ProjectileDirection.Z = 0.0f;

	if (!ProjectileDirection.Normalize())
	{
		UE_LOG(LogTemp, Warning, TEXT("Projectile spawn skipped: invalid projectile direction."));
		OwnerCharacter->ClearFacingOverride();
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
		OwnerCharacter->ClearFacingOverride();
		return;
	}

	Projectile->InitializeProjectile(OwnerCharacter, ProjectileDirection, AttackDamage, ProjectileSpeed, EProjectileTargetType::Enemies, TargetEnemy);

	UE_LOG(LogTemp, Log, TEXT("[%.2f] Attack #%d Projectile Spawned: Target=%s SpawnLocation=%s Direction=%s"),
		GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0,
		ActiveAttackSequence,
		*GetNameSafe(TargetEnemy),
		*SpawnLocation.ToString(),
		*ProjectileDirection.ToString());

	if (bDebugTargeting)
	{
		constexpr float DebugDuration = 1.5f;
		DrawDebugLine(GetWorld(), SpawnLocation, AimLocation, FColor::Yellow, false, DebugDuration, 0, 3.0f);
		DrawDebugSphere(GetWorld(), SpawnLocation, 24.0f, 12, FColor::Yellow, false, DebugDuration, 0, 3.0f);
	}

	CurrentAttackTarget.Reset();
	OwnerCharacter->ClearFacingOverride();
}

void UAutoAttackComponent::HandleOwnerCharacterModeChanged(ECharacterMode OldMode, ECharacterMode NewMode)
{
	if (NewMode == ECharacterMode::Active)
	{
		StartAutoAttack();
		return;
	}

	StopAutoAttack();
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
		ScheduleNextAttackTimer(AttackInterval);
		return;
	}

	if (!CanStartAttackNow())
	{
		UE_LOG(LogTemp, Log, TEXT("Attack Attempt Skipped: Cooldown"));
		const double NextReadyTime = LastAttackStartTime + AttackInterval;
		const float Delay = FMath::Max(0.01f, static_cast<float>(NextReadyTime - GetWorld()->GetTimeSeconds()));
		ScheduleNextAttackTimer(Delay);
		return;
	}

	StartTargetedAttack();
	ScheduleNextAttackTimer(AttackInterval);
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
	const float CalculatedPlayRate = AttackInterval > KINDA_SMALL_NUMBER ? MontageLength / AttackInterval : MaxAttackMontagePlayRate;
	UE_LOG(LogTemp, Log, TEXT("Attack Montage Rate: AttackInterval=%.3f MontageLength=%.3f CalculatedPlayRate=%.3f ActualPlayRate=%.3f"),
		AttackInterval,
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
	ActiveAttackSequence = ++AttackSequence;
	UE_LOG(LogTemp, Log, TEXT("[%.2f] Attack #%d Started"), GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0, ActiveAttackSequence);
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
	if (MontageLength <= KINDA_SMALL_NUMBER || AttackInterval <= KINDA_SMALL_NUMBER)
	{
		return SafeMaxPlayRate;
	}

	const float CalculatedPlayRate = MontageLength / AttackInterval;
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

	return World->GetTimeSeconds() >= LastAttackStartTime + AttackInterval;
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

AEnemyBase* UAutoAttackComponent::FindNearestEnemyTarget() const
{
	if (!OwnerCharacter || !GetWorld())
	{
		return nullptr;
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

	AEnemyBase* BestEnemy = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	const AActor* OwnerActor = OwnerCharacter->GetOwner();

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

		UHealthComponent* EnemyHealth = Enemy->GetHealthComponent();
		if (!EnemyHealth || EnemyHealth->IsDead())
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared2D(OwnerCharacter->GetActorLocation(), Enemy->GetActorLocation());
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestEnemy = Enemy;
		}
	}

	if (bDebugTargeting)
	{
		constexpr float DebugDuration = 1.5f;
		DrawDebugSphere(GetWorld(), OwnerCharacter->GetActorLocation(), TargetingRange, 48, FColor::Green, false, DebugDuration, 0, 2.0f);
		if (BestEnemy)
		{
			DrawDebugLine(GetWorld(), OwnerCharacter->GetActorLocation(), BestEnemy->GetActorLocation(), FColor::Green, false, DebugDuration, 0, 3.0f);
			UE_LOG(LogTemp, Log, TEXT("Targeting selected enemy: %s Distance=%.2f"),
				*GetNameSafe(BestEnemy),
				FMath::Sqrt(BestDistanceSquared));
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("Targeting found no living enemy in range %.2f."), TargetingRange);
		}
	}

	return BestEnemy;
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
