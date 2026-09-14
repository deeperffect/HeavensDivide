// Copyright Epic Games, Inc. All Rights Reserved.

#include "AttackProjectileBase.h"
#include "NinjaBuildComponent.h"
#include "SurvivorAbilityComponent.h"

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
	IgnoredOverlapActor = InIgnoredOverlapActor;
	AdditionalPierceCount = FMath::Max(0, InAdditionalPierceCount);
	RemainingEnemyHits = FMath::Max(1, 1 + AdditionalPierceCount);
	RemainingBounces = FMath::Max(0, InAdditionalBounceCount);
	bCanTriggerSplit = InSplitUpgradeLevel > 0;
 if(bCanTriggerSplit)SplitProjectileCount=2;
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
		const bool bHasVenomousKunai = AttackSource == EPlayerAttackSource::Ninja && PlayerUpgrades && PlayerUpgrades->HasUpgradeId(MarkedForDeathUpgradeIds::VenomousKunai);
		const float NormalDamage = ProjectileDamage;
		float FinalDamage = NormalDamage;
		// Intrinsic marks from War Banner / Spectral Brand also support the Ninja payoff.
		const bool bWasMarked = AttackSource == EPlayerAttackSource::Ninja && HitEnemy->IsMarked();
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

		FVector FeedbackLocation, ImpactNormal;
		HitEnemy->GetImpactContact(GetActorLocation(), FeedbackLocation, ImpactNormal);
		if (bFromSweep && !SweepResult.bStartPenetrating && !SweepResult.ImpactNormal.IsNearlyZero())
		{
			FeedbackLocation = SweepResult.ImpactPoint;
			ImpactNormal = SweepResult.ImpactNormal;
		}
		const uint8 StatusBefore=(HitEnemy->HasStatus(EEnemyStatusEffect::Bleed)?1:0)|(HitEnemy->HasStatus(EEnemyStatusEffect::Poison)?2:0);
		const bool bDamageApplied = AttackSource==EPlayerAttackSource::Ninja ? UNinjaBuildComponent::ApplyEmbeddedHit(HitEnemy,FinalDamage,const_cast<UPlayerUpgradeComponent*>(PlayerUpgrades)) : HitEnemy->ApplyPlayerDamage(FinalDamage, AttackSource);
		if (bDamageApplied) UImpactFeedbackLibrary::PlayImpactFeedback(this, ImpactFeedback, FeedbackLocation, ImpactNormal);
		if (bDamageApplied && GameplayOwnerOwner)
			if (auto* Abilities = GameplayOwnerOwner->FindComponentByClass<USurvivorAbilityComponent>()) Abilities->NotifyPartnerHit(AttackSource, HitEnemy,bAssistProjectile,StatusBefore);
		const bool bKilledEnemy = EnemyHealth->IsDead();
		if (bDamageApplied && !bKilledEnemy && bHasVenomousKunai)
		{
			HitEnemy->ApplyStatus(EEnemyStatusEffect::Poison, const_cast<UPlayerUpgradeComponent*>(PlayerUpgrades), AttackSource);
		}
		if (bDamageApplied && bCanTriggerSplit)
		{
			const FVector SplitImpactLocation = SweepResult.ImpactPoint.IsNearlyZero()
				? HitEnemy->GetActorLocation()
				: FVector(SweepResult.ImpactPoint);
			bCanTriggerSplit = false;
			TrySpawnSplitProjectiles(HitEnemy, SplitImpactLocation);
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
	Child->bAssistProjectile=bAssistProjectile;
	Child->InitializeProjectile(GameplayOwner, Direction, ProjectileDamage * 0.5f, ProjectileSpeed, TargetType, SourceTargetingRange,
		HitEnemy, true, AdditionalPierceCount, RemainingBounces, 0);
	Child->DamagedEnemies = DamagedEnemies;
	if (HitEnemy) Child->DamagedEnemies.Add(HitEnemy);
}
