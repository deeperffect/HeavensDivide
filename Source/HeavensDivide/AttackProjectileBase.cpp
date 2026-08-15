// Copyright Epic Games, Inc. All Rights Reserved.

#include "AttackProjectileBase.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "EnemyBase.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "HealthComponent.h"
#include "Kismet/GameplayStatics.h"
#include "PlayerUpgradeComponent.h"
#include "SurvivorPlayerController.h"
#include "Engine/OverlapResult.h"

namespace MarkedForDeathUpgradeIds
{
	static const FName MarkedBlade(TEXT("MarkedBlade"));
	static const FName ChainExecution(TEXT("ChainExecution"));
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

void AAttackProjectileBase::InitializeProjectile(AActor* InGameplayOwner, FVector Direction, float Damage, float Speed, EProjectileTargetType InTargetType)
{
	GameplayOwner = InGameplayOwner;
	SetOwner(InGameplayOwner);
	TargetType = InTargetType;

	if (APawn* OwnerPawn = Cast<APawn>(InGameplayOwner))
	{
		SetInstigator(OwnerPawn);
	}

	ProjectileDamage = Damage;
	ProjectileSpeed = Speed;

	Direction.Z = 0.0f;
	if (!Direction.Normalize())
	{
		Destroy();
		return;
	}

	CollisionComponent->IgnoreActorWhenMoving(InGameplayOwner, true);
	ProjectileMovement->InitialSpeed = ProjectileSpeed;
	ProjectileMovement->MaxSpeed = ProjectileSpeed;
	ProjectileMovement->Velocity = Direction * ProjectileSpeed;
	ProjectileMovement->bIsHomingProjectile = false;
	ProjectileMovement->HomingTargetComponent = nullptr;
	SetActorRotation(Direction.Rotation());
	bIsProjectileInitialized = true;
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	UE_LOG(LogTemp, Log, TEXT("Projectile initialized: Owner=%s Direction=%s Damage=%.2f Speed=%.2f"),
		*GetNameSafe(InGameplayOwner),
		*Direction.ToString(),
		ProjectileDamage,
		ProjectileSpeed);
}

void AAttackProjectileBase::HandleProjectileOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!bIsProjectileInitialized)
	{
		LogProjectileFilterResult(OtherActor, false);
		return;
	}

	if (!OtherActor || OtherActor == this || OtherActor == GameplayOwner)
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
		if (!HitEnemy || HitEnemy->IsDead())
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

		LogProjectileFilterResult(OtherActor, true);
		const UPlayerUpgradeComponent* PlayerUpgrades = GetPlayerUpgradesForMarkedForDeath(this, GameplayOwner);
		const bool bHasMarkedBlade = PlayerUpgrades && PlayerUpgrades->HasUpgradeId(MarkedForDeathUpgradeIds::MarkedBlade);
		const bool bHasChainExecution = PlayerUpgrades && PlayerUpgrades->HasUpgradeId(MarkedForDeathUpgradeIds::ChainExecution);
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
		EnemyHealth->ApplyDamage(FinalDamage);
		const bool bKilledEnemy = EnemyHealth->IsDead();
		const bool bExecutionKill = bHasChainExecution && bConsumedMark && bKilledEnemy;
		UE_LOG(LogTemp, Log, TEXT("Projectile hit enemy: %s Damage=%.2f RemainingHealth=%.2f"),
			*GetNameSafe(HitEnemy),
			FinalDamage,
			EnemyHealth->GetCurrentHealth());

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

		Destroy();
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
		PlayerHealth->ApplyDamage(ProjectileDamage);
		UE_LOG(LogTemp, Log, TEXT("Projectile hit active player: %s Damage=%.2f RemainingHealth=%.2f"),
			*GetNameSafe(HitPlayerCharacter),
			ProjectileDamage,
			PlayerHealth->GetCurrentHealth());

		Destroy();
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

int32 AAttackProjectileBase::GetSafeChainExecutionTargetCount() const
{
	return ChainExecutionTargetCount > 0 ? ChainExecutionTargetCount : 2;
}

float AAttackProjectileBase::GetSafeChainExecutionRadius() const
{
	return ChainExecutionRadius > 0.0f ? ChainExecutionRadius : 500.0f;
}
