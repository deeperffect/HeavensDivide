// Copyright Epic Games, Inc. All Rights Reserved.

#include "AttackProjectileBase.h"

#include "AutoAttackComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "EnemyBase.h"
#include "EnemyStatusEffectComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "HealthComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "PlayerUpgradeComponent.h"
#include "SurvivorPlayerController.h"
#include "Engine/OverlapResult.h"

namespace MarkedForDeathUpgradeIds
{
	static const FName MarkedBlade(TEXT("MarkedBlade"));
	static const FName ChainExecution(TEXT("ChainExecution"));
	static const FName ExecutionersKunai(TEXT("ExecutionersKunai"));
	static const FName VenomousKunai(TEXT("VenomousKunai"));
}

static const UPlayerUpgradeComponent* GetPlayerUpgradesForMarkedForDeath(const UObject* WorldContextObject, const AActor* GameplayOwner)
{
	const ASurvivorPlayerController* SurvivorController = Cast<ASurvivorPlayerController>(GameplayOwner ? GameplayOwner->GetOwner() : nullptr);
	if (!SurvivorController)
	{
		SurvivorController = Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(WorldContextObject, 0));
	}

	return SurvivorController ? SurvivorController->GetPlayerUpgrades() : nullptr;
}

AAttackProjectileBase::AAttackProjectileBase()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	CollisionComponent->SetupAttachment(Root);
	CollisionComponent->InitSphereRadius(16.0f);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CollisionComponent->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Overlap);

	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(CollisionComponent);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = Root;
	ProjectileMovement->InitialSpeed = ProjectileSpeed;
	ProjectileMovement->MaxSpeed = ProjectileSpeed;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->ProjectileGravityScale = 0.0f;
	ProjectileMovement->bSweepCollision = true;
}

void AAttackProjectileBase::BeginPlay()
{
	Super::BeginPlay();

	CollisionComponent->OnComponentBeginOverlap.AddDynamic(this, &AAttackProjectileBase::HandleProjectileOverlap);
	SetLifeSpan(ProjectileLifetime);
}

void AAttackProjectileBase::InitializeProjectile(
	AActor* InGameplayOwner,
	FVector Direction,
	float Damage,
	float Speed,
	EProjectileTargetType InTargetType,
	float InTargetingRange,
	bool bInCanTriggerExecutionersKunai,
	AActor* InIgnoredOverlapActor,
	bool bFlattenLaunchDirection,
	int32 InAdditionalPierceCount,
	int32 InAdditionalBounceCount,
	int32 InSplitUpgradeLevel)
{
	GameplayOwner = InGameplayOwner;
	SetOwner(InGameplayOwner);
	TargetType = InTargetType;
	SourceTargetingRange = FMath::Max(0.0f, InTargetingRange);
	bCanTriggerExecutionersKunai = bInCanTriggerExecutionersKunai;
	IgnoredOverlapActor = InIgnoredOverlapActor;
	AdditionalPierceCount = FMath::Max(0, InAdditionalPierceCount);
	RemainingEnemyHits = FMath::Max(1, 1 + AdditionalPierceCount);
	RemainingBounces = FMath::Max(0, InAdditionalBounceCount);
	bCanTriggerSplit = InSplitUpgradeLevel > 0;
	DamagedEnemies.Reset();

	if (APawn* OwnerPawn = Cast<APawn>(InGameplayOwner))
	{
		SetInstigator(OwnerPawn);
	}

	ProjectileDamage = Damage;
	ProjectileSpeed = Speed;

	if (bFlattenLaunchDirection)
	{
		Direction.Z = 0.0f;
	}
	if (!Direction.Normalize())
	{
		Destroy();
		return;
	}

	CollisionComponent->IgnoreActorWhenMoving(InGameplayOwner, true);
	if (IgnoredOverlapActor)
	{
		CollisionComponent->IgnoreActorWhenMoving(IgnoredOverlapActor, true);
	}
	ProjectileMovement->InitialSpeed = ProjectileSpeed;
	ProjectileMovement->MaxSpeed = ProjectileSpeed;
	ProjectileMovement->Velocity = Direction * ProjectileSpeed;
	ProjectileMovement->bIsHomingProjectile = false;
	ProjectileMovement->HomingTargetComponent = nullptr;
	SetActorRotation(Direction.Rotation());
	bIsProjectileInitialized = true;
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void AAttackProjectileBase::HandleProjectileOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bImpactResolved || !bIsProjectileInitialized)
	{
		LogProjectileFilterResult(OtherActor, false);
		return;
	}

	if (!OtherActor || OtherActor == this || OtherActor == GameplayOwner || OtherActor == IgnoredOverlapActor)
	{
		LogProjectileFilterResult(OtherActor, false);
		return;
	}

	const AActor* GameplayOwnerOwner = GameplayOwner ? GameplayOwner->GetOwner() : nullptr;
	if (GameplayOwnerOwner && OtherActor->GetOwner() == GameplayOwnerOwner)
	{
		LogProjectileFilterResult(OtherActor, false);
		return;
	}

	if (TargetType == EProjectileTargetType::Enemies)
	{
		AEnemyBase* HitEnemy = Cast<AEnemyBase>(OtherActor);
		if (!HitEnemy || HitEnemy->IsDead() || DamagedEnemies.Contains(HitEnemy))
		{
			LogProjectileFilterResult(OtherActor, false);
			return;
		}

		UHealthComponent* EnemyHealth = HitEnemy->GetHealthComponent();
		if (!EnemyHealth || EnemyHealth->IsDead())
		{
			LogProjectileFilterResult(OtherActor, false);
			return;
		}
		const EPlayerAttackSource AttackSource = AEnemyBase::ResolvePlayerAttackSource(GameplayOwner);
		if (!HitEnemy->CanReceivePlayerDamage(AttackSource))
		{
			return;
		}

		LogProjectileFilterResult(OtherActor, true);
		const UPlayerUpgradeComponent* PlayerUpgrades = GetPlayerUpgradesForMarkedForDeath(this, GameplayOwner);
		const bool bHasMarkedBlade = PlayerUpgrades && PlayerUpgrades->HasUpgradeId(MarkedForDeathUpgradeIds::MarkedBlade);
		const bool bHasChainExecution = PlayerUpgrades && PlayerUpgrades->HasUpgradeId(MarkedForDeathUpgradeIds::ChainExecution);
		const bool bHasExecutionersKunai = PlayerUpgrades && PlayerUpgrades->HasUpgradeId(MarkedForDeathUpgradeIds::ExecutionersKunai);
		const bool bHasVenomousKunai = AttackSource == EPlayerAttackSource::Ninja && PlayerUpgrades && PlayerUpgrades->HasUpgradeId(MarkedForDeathUpgradeIds::VenomousKunai);
		const float NormalDamage = ProjectileDamage;
		float FinalDamage = NormalDamage;
		const bool bWasMarked = bHasMarkedBlade && HitEnemy->IsMarked();
		const bool bConsumedMark = bWasMarked && HitEnemy->ConsumeMark();
		if (bConsumedMark)
		{
			FinalDamage *= FMath::Max(1.0f, MarkedTargetDamageMultiplier);
		}

		if (bDebugMarkedDamage)
		{
			UE_LOG(LogTemp, Log, TEXT("[MarkedForDeath] ProjectileHit Enemy=%s WasMarked=%s Consumed=%s NormalDamage=%.2f Multiplier=%.2f FinalDamage=%.2f"),
				*GetNameSafe(HitEnemy),
				bWasMarked ? TEXT("true") : TEXT("false"),
				bConsumedMark ? TEXT("true") : TEXT("false"),
				NormalDamage,
				MarkedTargetDamageMultiplier,
				FinalDamage);
		}

		const FVector ExecutionLocation = HitEnemy->GetActorLocation();
		const bool bDamageApplied = HitEnemy->ApplyPlayerDamage(FinalDamage, AttackSource);
		const bool bKilledEnemy = EnemyHealth->IsDead();
		if (bDamageApplied && !bKilledEnemy && bHasVenomousKunai)
		{
			HitEnemy->ApplyStatus(EEnemyStatusEffect::Poison, const_cast<UPlayerUpgradeComponent*>(PlayerUpgrades), AttackSource);
		}
		const bool bExecutionKill = bHasChainExecution && bConsumedMark && bKilledEnemy;

		if (bDebugChainExecution)
		{
			UE_LOG(LogTemp, Log, TEXT("[MarkedForDeath] ChainExecution Check Enemy=%s HasMarkedBlade=%s HasChainExecution=%s WasMarked=%s Consumed=%s Killed=%s WillTrigger=%s"),
				*GetNameSafe(HitEnemy),
				bHasMarkedBlade ? TEXT("true") : TEXT("false"),
				bHasChainExecution ? TEXT("true") : TEXT("false"),
				bWasMarked ? TEXT("true") : TEXT("false"),
				bConsumedMark ? TEXT("true") : TEXT("false"),
				bKilledEnemy ? TEXT("true") : TEXT("false"),
				bExecutionKill ? TEXT("true") : TEXT("false"));
		}

		if (bExecutionKill)
		{
			TryTriggerChainExecution(HitEnemy, ExecutionLocation);
		}

		if (bConsumedMark && bHasExecutionersKunai && bCanTriggerExecutionersKunai)
		{
			TryTriggerExecutionersKunai(HitEnemy, ExecutionLocation);
		}

		if (bDamageApplied && bCanTriggerSplit)
		{
			const FVector SplitImpactLocation = SweepResult.ImpactPoint.IsNearlyZero()
				? HitEnemy->GetActorLocation()
				: FVector(SweepResult.ImpactPoint);
			TrySpawnSplitProjectiles(HitEnemy, SplitImpactLocation);
			bCanTriggerSplit = false;
		}

		if (ConsumeEnemyHit(HitEnemy))
		{
			const FVector ImpactLocation = SweepResult.ImpactPoint.IsNearlyZero()
				? HitEnemy->GetActorLocation()
				: FVector(SweepResult.ImpactPoint);
			if (!bDamageApplied || !TryBounceFromImpact(ImpactLocation))
			{
				BeginImpactTrailFade();
			}
		}
		return;
	}

	if (TargetType == EProjectileTargetType::ActivePlayer)
	{
		if (Cast<AEnemyBase>(OtherActor))
		{
			LogProjectileFilterResult(OtherActor, false);
			return;
		}

		ACharacterBase* HitPlayerCharacter = Cast<ACharacterBase>(OtherActor);
		if (!HitPlayerCharacter)
		{
			LogProjectileFilterResult(OtherActor, false);
			return;
		}

		ASurvivorPlayerController* SurvivorController = Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
		UCharacterManagerComponent* CharacterManager = SurvivorController ? SurvivorController->GetCharacterManager() : nullptr;
		if (!CharacterManager || CharacterManager->GetActiveCharacter() != HitPlayerCharacter)
		{
			LogProjectileFilterResult(OtherActor, false);
			return;
		}

		UHealthComponent* PlayerHealth = SurvivorController->GetPlayerHealthComponent();
		if (!PlayerHealth || PlayerHealth->IsDead())
		{
			LogProjectileFilterResult(OtherActor, false);
			return;
		}

		LogProjectileFilterResult(OtherActor, true);
		SurvivorController->ApplyDamageToPlayer(ProjectileDamage);

		BeginImpactTrailFade();
	}
}

void AAttackProjectileBase::LogProjectileFilterResult(AActor* OtherActor, bool bValidDamageTarget) const
{
	if (!bDebugProjectileFiltering)
	{
		return;
	}

	const TCHAR* ProjectileSide = TargetType == EProjectileTargetType::ActivePlayer ? TEXT("Enemy Projectile") : TEXT("Player Projectile");
	UE_LOG(LogTemp, Log, TEXT("%s Owner=%s Hit=%s Valid Damage Target=%s"),
		ProjectileSide,
		*GetNameSafe(GameplayOwner),
		*GetNameSafe(OtherActor),
		bValidDamageTarget ? TEXT("true") : TEXT("false"));
}

void AAttackProjectileBase::BeginImpactTrailFade()
{
	if (bImpactResolved)
	{
		return;
	}

	bImpactResolved = true;
	bIsProjectileInitialized = false;

	if (CollisionComponent)
	{
		CollisionComponent->SetGenerateOverlapEvents(false);
		CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (ProjectileMovement)
	{
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Deactivate();
	}

	if (VisualMesh)
	{
		VisualMesh->SetVisibility(false, false);
		VisualMesh->SetHiddenInGame(true, false);
	}

	TArray<UNiagaraComponent*> NiagaraComponents;
	GetComponents(NiagaraComponents);
	for (UNiagaraComponent* NiagaraComponent : NiagaraComponents)
	{
		if (NiagaraComponent)
		{
			NiagaraComponent->Deactivate();
		}
	}

	const float SafeFadeDuration = FMath::Max(0.0f, ImpactTrailFadeDuration);
	if (SafeFadeDuration <= KINDA_SMALL_NUMBER)
	{
		Destroy();
		return;
	}

	SetLifeSpan(SafeFadeDuration);
}

bool AAttackProjectileBase::ConsumeEnemyHit(AEnemyBase* HitEnemy)
{
	if (HitEnemy)
	{
		DamagedEnemies.Add(HitEnemy);
	}

	--RemainingEnemyHits;
	return RemainingEnemyHits <= 0;
}

bool AAttackProjectileBase::TryBounceFromImpact(const FVector& ImpactLocation)
{
	if (RemainingBounces <= 0 || !ProjectileMovement)
	{
		return false;
	}

	AEnemyBase* BounceTarget = FindBounceTarget(ImpactLocation);
	if (!BounceTarget)
	{
		return false;
	}

	FVector BounceDirection = BounceTarget->GetActorLocation() - ImpactLocation;
	BounceDirection.Z = 0.0f;
	if (!BounceDirection.Normalize())
	{
		return false;
	}

	--RemainingBounces;
	RemainingEnemyHits = FMath::Max(1, 1 + AdditionalPierceCount);
	SetActorLocation(ImpactLocation + BounceDirection * 20.0f);
	SetActorRotation(BounceDirection.Rotation());
	ProjectileMovement->Velocity = BounceDirection * ProjectileSpeed;
	return true;
}

AEnemyBase* AAttackProjectileBase::FindBounceTarget(const FVector& SearchLocation) const
{
	if (!GetWorld() || BounceSearchRadius <= 0.0f)
	{
		return nullptr;
	}

	TArray<FOverlapResult> OverlapResults;
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_GameTraceChannel1);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ProjectileBounceTargeting), false, this);
	QueryParams.AddIgnoredActor(this);
	if (GameplayOwner) QueryParams.AddIgnoredActor(GameplayOwner);
	GetWorld()->OverlapMultiByObjectType(OverlapResults, SearchLocation, FQuat::Identity, ObjectQueryParams,
		FCollisionShape::MakeSphere(BounceSearchRadius), QueryParams);

	AEnemyBase* BestTarget = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	const EPlayerAttackSource AttackSource = AEnemyBase::ResolvePlayerAttackSource(GameplayOwner);
	for (const FOverlapResult& Result : OverlapResults)
	{
		AEnemyBase* Candidate = Cast<AEnemyBase>(Result.GetActor());
		if (!Candidate || Candidate->IsDead() || DamagedEnemies.Contains(Candidate) || !Candidate->CanReceivePlayerDamage(AttackSource))
		{
			continue;
		}
		const UHealthComponent* Health = Candidate->GetHealthComponent();
		const float DistanceSquared = FVector::DistSquared(SearchLocation, Candidate->GetActorLocation());
		if (Health && !Health->IsDead() && DistanceSquared <= FMath::Square(BounceSearchRadius) && DistanceSquared < BestDistanceSquared)
		{
			BestTarget = Candidate;
			BestDistanceSquared = DistanceSquared;
		}
	}
	return BestTarget;
}

void AAttackProjectileBase::TrySpawnSplitProjectiles(AEnemyBase* HitEnemy, const FVector& ImpactLocation)
{
	if (!GetWorld() || SplitProjectileCount <= 0 || !ProjectileMovement)
	{
		return;
	}

	FVector IncomingDirection = ProjectileMovement->Velocity.GetSafeNormal2D();
	if (IncomingDirection.IsNearlyZero())
	{
		IncomingDirection = GetActorForwardVector().GetSafeNormal2D();
	}
	const int32 ChildCount = FMath::Max(1, SplitProjectileCount);
	for (int32 Index = 0; Index < ChildCount; ++Index)
	{
		const float Alpha = ChildCount == 1 ? 0.5f : static_cast<float>(Index) / static_cast<float>(ChildCount - 1);
		const float Angle = FMath::Lerp(-FMath::Abs(SplitAngleDegrees), FMath::Abs(SplitAngleDegrees), Alpha);
		SpawnSplitProjectile(ImpactLocation, IncomingDirection.RotateAngleAxis(Angle, FVector::UpVector), HitEnemy);
	}
}

void AAttackProjectileBase::SpawnSplitProjectile(const FVector& SpawnLocation, const FVector& Direction, AEnemyBase* HitEnemy)
{
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = GameplayOwner;
	SpawnParameters.Instigator = GetInstigator();
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AAttackProjectileBase* Child = GetWorld()->SpawnActor<AAttackProjectileBase>(GetClass(), SpawnLocation + Direction * 20.0f, Direction.Rotation(), SpawnParameters);
	if (!Child) return;
	Child->InitializeProjectile(GameplayOwner, Direction, ProjectileDamage, ProjectileSpeed, TargetType, SourceTargetingRange,
		bCanTriggerExecutionersKunai, HitEnemy, true, AdditionalPierceCount, RemainingBounces, 0);
	Child->DamagedEnemies = DamagedEnemies;
	if (HitEnemy) Child->DamagedEnemies.Add(HitEnemy);
}

void AAttackProjectileBase::TryTriggerChainExecution(AEnemyBase* ExecutedEnemy, const FVector& ExecutionLocation)
{
	const int32 SafeChainExecutionTargetCount = GetSafeChainExecutionTargetCount();
	const float SafeChainExecutionRadius = GetSafeChainExecutionRadius();
	if (!GetWorld() || !ExecutedEnemy)
	{
		return;
	}

	TArray<FOverlapResult> OverlapResults;
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_GameTraceChannel1);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ChainExecution), false, this);
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(ExecutedEnemy);
	if (GameplayOwner)
	{
		QueryParams.AddIgnoredActor(GameplayOwner);
	}

	GetWorld()->OverlapMultiByObjectType(
		OverlapResults,
		ExecutionLocation,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(SafeChainExecutionRadius),
		QueryParams);

	struct FChainExecutionCandidate
	{
		TObjectPtr<AEnemyBase> Enemy;
		float DistanceSquared = 0.0f;
	};

	TArray<FChainExecutionCandidate> Candidates;
	Candidates.Reserve(OverlapResults.Num());

	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AEnemyBase* CandidateEnemy = Cast<AEnemyBase>(OverlapResult.GetActor());
		if (!CandidateEnemy || CandidateEnemy == ExecutedEnemy || CandidateEnemy->IsDead() || CandidateEnemy->IsMarked())
		{
			continue;
		}

		UHealthComponent* CandidateHealth = CandidateEnemy->GetHealthComponent();
		if (!CandidateHealth || CandidateHealth->IsDead())
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared2D(ExecutionLocation, CandidateEnemy->GetActorLocation());
		if (DistanceSquared > FMath::Square(SafeChainExecutionRadius))
		{
			continue;
		}

		Candidates.Add({ CandidateEnemy, DistanceSquared });
	}

	Candidates.Sort([](const FChainExecutionCandidate& Left, const FChainExecutionCandidate& Right)
	{
		return Left.DistanceSquared < Right.DistanceSquared;
	});

	const int32 MarksToTransfer = FMath::Min(SafeChainExecutionTargetCount, Candidates.Num());

	if (bDebugChainExecution)
	{
		UE_LOG(LogTemp, Log, TEXT("[MarkedForDeath] ChainExecution ExecutedEnemy=%s Radius=%.1f RawRadius=%.1f TargetCount=%d RawTargetCount=%d Candidates=%d MarksToTransfer=%d"),
			*GetNameSafe(ExecutedEnemy),
			SafeChainExecutionRadius,
			ChainExecutionRadius,
			SafeChainExecutionTargetCount,
			ChainExecutionTargetCount,
			Candidates.Num(),
			MarksToTransfer);
	}

	for (int32 CandidateIndex = 0; CandidateIndex < MarksToTransfer; ++CandidateIndex)
	{
		AEnemyBase* CandidateEnemy = Candidates[CandidateIndex].Enemy.Get();
		if (!CandidateEnemy)
		{
			continue;
		}

		const bool bAppliedMark = CandidateEnemy->ApplyMark();
		if (bDebugChainExecution)
		{
			UE_LOG(LogTemp, Log, TEXT("[MarkedForDeath] ChainExecution Transfer Target=%s Distance=%.1f Applied=%s"),
				*GetNameSafe(CandidateEnemy),
				FMath::Sqrt(Candidates[CandidateIndex].DistanceSquared),
				bAppliedMark ? TEXT("true") : TEXT("false"));
		}
	}
}

void AAttackProjectileBase::TryTriggerExecutionersKunai(AEnemyBase* ConsumedMarkEnemy, const FVector& MarkConsumedLocation)
{
	if (!GetWorld() || SourceTargetingRange <= 0.0f)
	{
		if (bDebugExecutionersKunai)
		{
			UE_LOG(LogTemp, Log, TEXT("[MarkedForDeath] ExecutionersKunai skipped: World=%s TargetingRange=%.1f"),
				GetWorld() ? TEXT("valid") : TEXT("invalid"),
				SourceTargetingRange);
		}
		return;
	}

	AEnemyBase* BonusTarget = FindExecutionersKunaiTarget(ConsumedMarkEnemy, MarkConsumedLocation);
	if (!BonusTarget)
	{
		if (bDebugExecutionersKunai)
		{
			UE_LOG(LogTemp, Log, TEXT("[MarkedForDeath] ExecutionersKunai skipped: no valid target within %.1f of %s"),
				SourceTargetingRange,
				*MarkConsumedLocation.ToString());
		}
		return;
	}

	SpawnExecutionersKunai(BonusTarget, ConsumedMarkEnemy, MarkConsumedLocation);
}

AEnemyBase* AAttackProjectileBase::FindExecutionersKunaiTarget(AEnemyBase* ConsumedMarkEnemy, const FVector& SearchLocation) const
{
	if (!GetWorld() || SourceTargetingRange <= 0.0f)
	{
		return nullptr;
	}

	TArray<FOverlapResult> OverlapResults;
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_GameTraceChannel1);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ExecutionersKunaiTargeting), false, this);
	QueryParams.AddIgnoredActor(this);
	if (GameplayOwner)
	{
		QueryParams.AddIgnoredActor(GameplayOwner);
	}

	GetWorld()->OverlapMultiByObjectType(
		OverlapResults,
		SearchLocation,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(SourceTargetingRange),
		QueryParams);

	AEnemyBase* BestTarget = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	AEnemyBase* ConsumedEnemyFallback = nullptr;
	float ConsumedEnemyFallbackDistanceSquared = TNumericLimits<float>::Max();
	TSet<AEnemyBase*> UniqueEnemies;

	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AEnemyBase* CandidateEnemy = Cast<AEnemyBase>(OverlapResult.GetActor());
		if (!CandidateEnemy || CandidateEnemy->IsDead() || UniqueEnemies.Contains(CandidateEnemy))
		{
			continue;
		}

		UHealthComponent* CandidateHealth = CandidateEnemy->GetHealthComponent();
		if (!CandidateHealth || CandidateHealth->IsDead())
		{
			continue;
		}

		const bool bIsConsumedMarkEnemy = CandidateEnemy == ConsumedMarkEnemy;
		UniqueEnemies.Add(CandidateEnemy);

		const float DistanceSquared = FVector::DistSquared2D(SearchLocation, CandidateEnemy->GetActorLocation());
		if (DistanceSquared > FMath::Square(SourceTargetingRange))
		{
			continue;
		}

		if (bIsConsumedMarkEnemy)
		{
			ConsumedEnemyFallback = CandidateEnemy;
			ConsumedEnemyFallbackDistanceSquared = DistanceSquared;
			continue;
		}

		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestTarget = CandidateEnemy;
		}
	}

	if (!BestTarget)
	{
		BestTarget = ConsumedEnemyFallback;
		BestDistanceSquared = ConsumedEnemyFallbackDistanceSquared;
	}

	if (bDebugExecutionersKunai)
	{
		UE_LOG(LogTemp, Log, TEXT("[MarkedForDeath] ExecutionersKunai target search Candidates=%d ConsumedEnemy=%s Selected=%s UsedConsumedEnemyFallback=%s Distance=%.1f Range=%.1f"),
			UniqueEnemies.Num(),
			*GetNameSafe(ConsumedMarkEnemy),
			*GetNameSafe(BestTarget),
			BestTarget && BestTarget == ConsumedMarkEnemy ? TEXT("true") : TEXT("false"),
			BestTarget ? FMath::Sqrt(BestDistanceSquared) : -1.0f,
			SourceTargetingRange);
	}

	return BestTarget;
}

void AAttackProjectileBase::SpawnExecutionersKunai(AEnemyBase* TargetEnemy, AEnemyBase* ConsumedMarkEnemy, const FVector& SpawnOrigin)
{
	if (!GetWorld() || !TargetEnemy || TargetEnemy->IsDead())
	{
		return;
	}

	const UCapsuleComponent* TargetCapsule = TargetEnemy->GetCapsuleComponent();
	const FVector TargetLocation = TargetCapsule ? TargetCapsule->GetComponentLocation() : TargetEnemy->GetActorLocation();
	FVector HorizontalDirection = TargetLocation - SpawnOrigin;
	HorizontalDirection.Z = 0.0f;
	const bool bTargetAtSpawnOrigin = HorizontalDirection.SizeSquared2D() <= FMath::Square(FMath::Max(16.0f, TargetCapsule ? TargetCapsule->GetScaledCapsuleRadius() : 16.0f));
	if (!HorizontalDirection.Normalize())
	{
		HorizontalDirection = ProjectileMovement ? ProjectileMovement->Velocity : FVector::ZeroVector;
		HorizontalDirection.Z = 0.0f;
		if (!HorizontalDirection.Normalize() && GameplayOwner)
		{
			HorizontalDirection = SpawnOrigin - GameplayOwner->GetActorLocation();
			HorizontalDirection.Z = 0.0f;
			HorizontalDirection.Normalize();
		}

		if (HorizontalDirection.IsNearlyZero())
		{
			if (bDebugExecutionersKunai)
			{
				UE_LOG(LogTemp, Log, TEXT("[MarkedForDeath] ExecutionersKunai skipped: invalid direction to %s"), *GetNameSafe(TargetEnemy));
			}
			return;
		}
	}

	const float SpawnOffset = FMath::Max(0.0f, ExecutionersKunaiSpawnForwardOffset);
	const FVector GroundSpawnLocation = bTargetAtSpawnOrigin
		? SpawnOrigin - HorizontalDirection * SpawnOffset
		: SpawnOrigin + HorizontalDirection * SpawnOffset;
	const FVector SpawnLocation = GroundSpawnLocation + ExecutionersKunaiSpawnOffset;
	FVector LaunchDirection = TargetLocation - SpawnLocation;
	if (!LaunchDirection.Normalize())
	{
		if (bDebugExecutionersKunai)
		{
			UE_LOG(LogTemp, Log, TEXT("[MarkedForDeath] ExecutionersKunai skipped: invalid elevated direction to %s"), *GetNameSafe(TargetEnemy));
		}
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = GameplayOwner;
	SpawnParameters.Instigator = GetInstigator();
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AAttackProjectileBase* BonusProjectile = GetWorld()->SpawnActor<AAttackProjectileBase>(
		GetClass(),
		SpawnLocation,
		LaunchDirection.Rotation(),
		SpawnParameters);

	if (!BonusProjectile)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MarkedForDeath] ExecutionersKunai spawn failed."));
		return;
	}

	BonusProjectile->InitializeProjectile(
		GameplayOwner,
		LaunchDirection,
		ProjectileDamage,
		ProjectileSpeed * FMath::Max(0.01f, ExecutionersKunaiSpeedMultiplier),
		TargetType,
		SourceTargetingRange,
		false,
		TargetEnemy != ConsumedMarkEnemy ? ConsumedMarkEnemy : nullptr,
		false,
		AdditionalPierceCount,
		RemainingBounces,
		bCanTriggerSplit ? 1 : 0);

	if (AActor* GameplayOwnerActor = GameplayOwner.Get())
	{
		if (UAutoAttackComponent* AutoAttack = GameplayOwnerActor->FindComponentByClass<UAutoAttackComponent>())
		{
			AutoAttack->RegisterKunaiFired();
		}
	}

	if (bDebugExecutionersKunai)
	{
		UE_LOG(LogTemp, Log, TEXT("[MarkedForDeath] ExecutionersKunai spawned Target=%s Spawn=%s Direction=%s SameTargetFallback=%s Damage=%.2f Speed=%.1f SpeedMultiplier=%.2f"),
			*GetNameSafe(TargetEnemy),
			*SpawnLocation.ToString(),
			*LaunchDirection.ToString(),
			bTargetAtSpawnOrigin ? TEXT("true") : TEXT("false"),
			ProjectileDamage,
			ProjectileSpeed * FMath::Max(0.01f, ExecutionersKunaiSpeedMultiplier),
			ExecutionersKunaiSpeedMultiplier);
	}
}

int32 AAttackProjectileBase::GetSafeChainExecutionTargetCount() const
{
	return ChainExecutionTargetCount > 0 ? ChainExecutionTargetCount : 2;
}

float AAttackProjectileBase::GetSafeChainExecutionRadius() const
{
	return ChainExecutionRadius > 0.0f ? ChainExecutionRadius : 500.0f;
}
