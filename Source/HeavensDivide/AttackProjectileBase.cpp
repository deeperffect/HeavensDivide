// Copyright Epic Games, Inc. All Rights Reserved.

#include "AttackProjectileBase.h"

#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "EnemyBase.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "HealthComponent.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "SurvivorPlayerController.h"

static TAutoConsoleVariable<int32> CVarHDLogNinjaHoming(
	TEXT("hd.LogNinjaHoming"),
	0,
	TEXT("Logs meaningful Ninja projectile homing assist events when enabled."));

AAttackProjectileBase::AAttackProjectileBase()
{
	PrimaryActorTick.bCanEverTick = true;
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

void AAttackProjectileBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateHomingAssist();
}

void AAttackProjectileBase::BeginPlay()
{
	Super::BeginPlay();

	CollisionComponent->OnComponentBeginOverlap.AddDynamic(this, &AAttackProjectileBase::HandleProjectileOverlap);
	SetLifeSpan(ProjectileLifetime);
}

void AAttackProjectileBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DisableHoming();

	Super::EndPlay(EndPlayReason);
}

void AAttackProjectileBase::InitializeProjectile(AActor* InGameplayOwner, FVector Direction, float Damage, float Speed, EProjectileTargetType InTargetType, AActor* InHomingTarget, float InHomingStrengthMultiplier, FVector InHomingTargetOffset)
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
	SetActorRotation(Direction.Rotation());
	ConfigureHoming(InHomingTarget, InHomingStrengthMultiplier, InHomingTargetOffset);
	bIsProjectileInitialized = true;
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	UE_LOG(LogTemp, Log, TEXT("Projectile initialized: Owner=%s Direction=%s Damage=%.2f Speed=%.2f Homing=%s HomingTarget=%s HomingOffset=%s HomingAcceleration=%.2f"),
		*GetNameSafe(InGameplayOwner),
		*Direction.ToString(),
		ProjectileDamage,
		ProjectileSpeed,
		ProjectileMovement->bIsHomingProjectile ? TEXT("true") : TEXT("false"),
		*GetNameSafe(HomingTargetActor),
		*InHomingTargetOffset.ToString(),
		ProjectileMovement->HomingAccelerationMagnitude);
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
		EnemyHealth->ApplyDamage(ProjectileDamage);
		UE_LOG(LogTemp, Log, TEXT("Projectile hit enemy: %s Damage=%.2f RemainingHealth=%.2f"),
			*GetNameSafe(HitEnemy),
			ProjectileDamage,
			EnemyHealth->GetCurrentHealth());

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

void AAttackProjectileBase::HandleHomingTargetDestroyed(AActor* DestroyedActor)
{
	if (DestroyedActor == HomingTargetActor)
	{
		DisableHoming();
	}
}

void AAttackProjectileBase::HandleHomingTargetDeath()
{
	DisableHoming();
}

void AAttackProjectileBase::ConfigureHoming(AActor* InHomingTarget, float InHomingStrengthMultiplier, const FVector& InHomingTargetOffset)
{
	if (!ProjectileMovement)
	{
		return;
	}

	DisableHoming();

	if (!bIsHoming || !InHomingTarget)
	{
		return;
	}

	HomingTargetActor = InHomingTarget;
	ConfiguredHomingTargetOffset = InHomingTargetOffset;
	USceneComponent* TargetComponent = InHomingTarget->GetRootComponent();
	if (!TargetComponent)
	{
		DisableHoming();
		return;
	}

	if (!InHomingTargetOffset.IsNearlyZero())
	{
		HomingOffsetTargetComponent = NewObject<USceneComponent>(this, TEXT("HomingOffsetTargetComponent"));
		if (HomingOffsetTargetComponent)
		{
			HomingOffsetTargetComponent->RegisterComponent();
			HomingOffsetTargetComponent->SetWorldLocation(TargetComponent->GetComponentLocation() + InHomingTargetOffset);
			HomingOffsetTargetComponent->AttachToComponent(TargetComponent, FAttachmentTransformRules::KeepWorldTransform);
			TargetComponent = HomingOffsetTargetComponent;
		}
	}

	ProjectileMovement->bIsHomingProjectile = true;
	ProjectileMovement->HomingTargetComponent = TargetComponent;
	ConfiguredHomingStrengthMultiplier = FMath::Max(0.0f, InHomingStrengthMultiplier);
	const float SpeedScaledMinimum = FMath::Square(FMath::Max(0.0f, ProjectileSpeed)) / FMath::Max(1.0f, NearTargetDistance) * MinimumHomingAccelerationSpeedScale;
	BaseConfiguredHomingAcceleration = FMath::Max(HomingAccelerationMagnitude * ConfiguredHomingStrengthMultiplier, SpeedScaledMinimum);
	ProjectileMovement->HomingAccelerationMagnitude = BaseConfiguredHomingAcceleration;
	SetActorTickEnabled(true);

	InHomingTarget->OnDestroyed.AddDynamic(this, &AAttackProjectileBase::HandleHomingTargetDestroyed);
	if (AEnemyBase* EnemyTarget = Cast<AEnemyBase>(InHomingTarget))
	{
		if (UHealthComponent* TargetHealth = EnemyTarget->GetHealthComponent())
		{
			TargetHealth->OnDeath.AddDynamic(this, &AAttackProjectileBase::HandleHomingTargetDeath);
		}
	}
}

void AAttackProjectileBase::DisableHoming()
{
	SetActorTickEnabled(false);

	if (ProjectileMovement)
	{
		ProjectileMovement->bIsHomingProjectile = false;
		ProjectileMovement->HomingTargetComponent = nullptr;
	}

	if (HomingOffsetTargetComponent)
	{
		HomingOffsetTargetComponent->DestroyComponent();
		HomingOffsetTargetComponent = nullptr;
	}

	if (HomingTargetActor)
	{
		HomingTargetActor->OnDestroyed.RemoveDynamic(this, &AAttackProjectileBase::HandleHomingTargetDestroyed);
		if (AEnemyBase* EnemyTarget = Cast<AEnemyBase>(HomingTargetActor))
		{
			if (UHealthComponent* TargetHealth = EnemyTarget->GetHealthComponent())
			{
				TargetHealth->OnDeath.RemoveDynamic(this, &AAttackProjectileBase::HandleHomingTargetDeath);
			}
		}
	}

	HomingTargetActor = nullptr;
	BaseConfiguredHomingAcceleration = 0.0f;
	ConfiguredHomingStrengthMultiplier = 1.0f;
	bWasApproachingHomingTarget = false;
	bLoggedNearTargetSteering = false;
	bLoggedOvershootRecovery = false;
}

void AAttackProjectileBase::UpdateHomingAssist()
{
	if (!ProjectileMovement || !ProjectileMovement->bIsHomingProjectile || !HomingTargetActor)
	{
		return;
	}

	const FVector TargetCenter = GetHomingTargetCenter();
	const FVector ToTarget = TargetCenter - GetActorLocation();
	const float DistanceToTarget = ToTarget.Size();
	if (DistanceToTarget <= KINDA_SMALL_NUMBER)
	{
		TryApplyAssignedTargetHit(0.0f);
		return;
	}

	if (TryApplyAssignedTargetHit(DistanceToTarget))
	{
		return;
	}

	const FVector DirectionToTarget = ToTarget / DistanceToTarget;
	const FVector VelocityDirection = ProjectileMovement->Velocity.GetSafeNormal();
	const float DotToTarget = FVector::DotProduct(VelocityDirection, DirectionToTarget);
	const bool bIsApproaching = DotToTarget > 0.05f;
	const bool bOvershot = bWasApproachingHomingTarget && DotToTarget < -0.15f;
	bWasApproachingHomingTarget = bWasApproachingHomingTarget || bIsApproaching;

	const float NearAlpha = NearTargetDistance > KINDA_SMALL_NUMBER
		? FMath::Clamp(1.0f - (DistanceToTarget / NearTargetDistance), 0.0f, 1.0f)
		: 0.0f;
	const float SmoothNearAlpha = FMath::InterpEaseInOut(0.0f, 1.0f, NearAlpha, 2.0f);
	float DesiredMultiplier = FMath::Lerp(1.0f, NearTargetHomingMultiplier, SmoothNearAlpha);
	if (bOvershot)
	{
		DesiredMultiplier = FMath::Max(DesiredMultiplier, OvershootHomingMultiplier);
		if (!bLoggedOvershootRecovery && CVarHDLogNinjaHoming.GetValueOnGameThread() != 0)
		{
			bLoggedOvershootRecovery = true;
			UE_LOG(LogTemp, Log, TEXT("[NinjaProjectile] Overshoot detected Dot=%.2f Distance=%.1f Using recovery homing"), DotToTarget, DistanceToTarget);
		}
	}
	else if (NearAlpha > 0.0f && !bLoggedNearTargetSteering && CVarHDLogNinjaHoming.GetValueOnGameThread() != 0)
	{
		bLoggedNearTargetSteering = true;
		UE_LOG(LogTemp, Log, TEXT("[NinjaProjectile] Entered near-target steering Distance=%.1f HomingMultiplier=%.2f"), DistanceToTarget, DesiredMultiplier);
	}

	ProjectileMovement->HomingAccelerationMagnitude = BaseConfiguredHomingAcceleration * DesiredMultiplier;

	if (HomingOffsetTargetComponent && NearAlpha > 0.0f)
	{
		const FVector TargetRootLocation = HomingTargetActor->GetRootComponent()
			? HomingTargetActor->GetRootComponent()->GetComponentLocation()
			: HomingTargetActor->GetActorLocation();
		HomingOffsetTargetComponent->SetWorldLocation(TargetRootLocation + ConfiguredHomingTargetOffset * (1.0f - SmoothNearAlpha));
	}
}

bool AAttackProjectileBase::TryApplyAssignedTargetHit(float DistanceToTarget)
{
	if (TargetType != EProjectileTargetType::Enemies || !HomingTargetActor || HomingHitForgivenessRadius <= 0.0f)
	{
		return false;
	}

	AEnemyBase* TargetEnemy = Cast<AEnemyBase>(HomingTargetActor);
	if (!TargetEnemy || TargetEnemy->IsDead())
	{
		return false;
	}

	UHealthComponent* EnemyHealth = TargetEnemy->GetHealthComponent();
	if (!EnemyHealth || EnemyHealth->IsDead())
	{
		return false;
	}

	const float CollisionRadius = CollisionComponent ? CollisionComponent->GetScaledSphereRadius() : 0.0f;
	if (DistanceToTarget > HomingHitForgivenessRadius + CollisionRadius)
	{
		return false;
	}

	EnemyHealth->ApplyDamage(ProjectileDamage);
	if (CVarHDLogNinjaHoming.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[NinjaProjectile] Near-miss forgiveness hit Distance=%.1f Target=%s"), DistanceToTarget, *GetNameSafe(TargetEnemy));
	}
	Destroy();
	return true;
}

FVector AAttackProjectileBase::GetHomingTargetCenter() const
{
	if (!HomingTargetActor)
	{
		return FVector::ZeroVector;
	}

	if (const AEnemyBase* EnemyTarget = Cast<AEnemyBase>(HomingTargetActor))
	{
		if (const UCapsuleComponent* CapsuleComponent = EnemyTarget->GetCapsuleComponent())
		{
			return CapsuleComponent->GetComponentLocation();
		}
	}

	return HomingTargetActor->GetActorLocation();
}
