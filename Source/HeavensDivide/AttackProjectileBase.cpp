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
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

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
	SetActorRotation(Direction.Rotation());

	UE_LOG(LogTemp, Log, TEXT("Projectile initialized: Owner=%s Direction=%s Damage=%.2f Speed=%.2f"),
		*GetNameSafe(InGameplayOwner),
		*Direction.ToString(),
		ProjectileDamage,
		ProjectileSpeed);
}

void AAttackProjectileBase::HandleProjectileOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == this || OtherActor == GameplayOwner)
	{
		return;
	}

	const AActor* GameplayOwnerOwner = GameplayOwner ? GameplayOwner->GetOwner() : nullptr;
	if (GameplayOwnerOwner && OtherActor->GetOwner() == GameplayOwnerOwner)
	{
		return;
	}

	if (TargetType == EProjectileTargetType::Enemies)
	{
		AEnemyBase* HitEnemy = Cast<AEnemyBase>(OtherActor);
		if (!HitEnemy || HitEnemy->IsDead())
		{
			return;
		}

		UHealthComponent* EnemyHealth = HitEnemy->GetHealthComponent();
		if (!EnemyHealth || EnemyHealth->IsDead())
		{
			return;
		}

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
			return;
		}

		ACharacterBase* HitPlayerCharacter = Cast<ACharacterBase>(OtherActor);
		if (!HitPlayerCharacter)
		{
			return;
		}

		ASurvivorPlayerController* SurvivorController = Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
		UCharacterManagerComponent* CharacterManager = SurvivorController ? SurvivorController->GetCharacterManager() : nullptr;
		if (!CharacterManager || CharacterManager->GetActiveCharacter() != HitPlayerCharacter)
		{
			return;
		}

		UHealthComponent* PlayerHealth = SurvivorController->GetPlayerHealthComponent();
		if (!PlayerHealth || PlayerHealth->IsDead())
		{
			return;
		}

		PlayerHealth->ApplyDamage(ProjectileDamage);
		UE_LOG(LogTemp, Log, TEXT("Projectile hit active player: %s Damage=%.2f RemainingHealth=%.2f"),
			*GetNameSafe(HitPlayerCharacter),
			ProjectileDamage,
			PlayerHealth->GetCurrentHealth());

		Destroy();
	}
}
