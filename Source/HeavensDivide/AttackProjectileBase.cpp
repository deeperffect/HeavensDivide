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
#include "SurvivorPlayerController.h"

AAttackProjectileBase::AAttackProjectileBase()
{
	PrimaryActorTick.bCanEverTick = false;

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

void AAttackProjectileBase::InitializeProjectile(AActor* InGameplayOwner, FVector Direction, float Damage, float Speed, EProjectileTargetType InTargetType, AActor* InHomingTarget)
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
	ConfigureHoming(InHomingTarget);
	bIsProjectileInitialized = true;
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	UE_LOG(LogTemp, Log, TEXT("Projectile initialized: Owner=%s Direction=%s Damage=%.2f Speed=%.2f Homing=%s HomingTarget=%s HomingAcceleration=%.2f"),
		*GetNameSafe(InGameplayOwner),
		*Direction.ToString(),
		ProjectileDamage,
		ProjectileSpeed,
		ProjectileMovement->bIsHomingProjectile ? TEXT("true") : TEXT("false"),
		*GetNameSafe(HomingTargetActor),
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

void AAttackProjectileBase::ConfigureHoming(AActor* InHomingTarget)
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
	USceneComponent* TargetComponent = InHomingTarget->GetRootComponent();
	if (!TargetComponent)
	{
		DisableHoming();
		return;
	}

	ProjectileMovement->bIsHomingProjectile = true;
	ProjectileMovement->HomingTargetComponent = TargetComponent;
	ProjectileMovement->HomingAccelerationMagnitude = HomingAccelerationMagnitude;

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
	if (ProjectileMovement)
	{
		ProjectileMovement->bIsHomingProjectile = false;
		ProjectileMovement->HomingTargetComponent = nullptr;
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
}
