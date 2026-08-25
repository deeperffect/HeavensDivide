// Copyright Epic Games, Inc. All Rights Reserved.
#include "SamuraiBladeWave.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnemyBase.h"
#include "EnemyStatusEffectComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "HealthComponent.h"
#include "PlayerUpgradeComponent.h"
#include "SamuraiCharacter.h"
#include "UObject/ConstructorHelpers.h"

ASamuraiBladeWave::ASamuraiBladeWave()
{
	PrimaryActorTick.bCanEverTick = false;
	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Collision->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Overlap);
	Collision->OnComponentBeginOverlap.AddDynamic(this, &ASamuraiBladeWave::HandleOverlap);
	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Collision);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded()) Visual->SetStaticMesh(Cube.Object);
	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->UpdatedComponent = Collision;
	Movement->ProjectileGravityScale = 0.0f;
	Movement->bRotationFollowsVelocity = true;
}

void ASamuraiBladeWave::InitializeBladeWave(ASamuraiCharacter* InSamurai, UPlayerUpgradeComponent* InUpgrades, FVector Direction,
	float InDamage, float InWidth, float InTravelDistance, float InSpeed, bool bInReturns)
{
	SourceSamurai = InSamurai;
	SourceUpgrades = InUpgrades;
	Damage = FMath::Max(0.0f, InDamage);
	Speed = FMath::Max(1.0f, InSpeed);
	bReturns = bInReturns;
	Direction.Z = 0.0f;
	if (!Direction.Normalize()) { Destroy(); return; }
	Collision->SetBoxExtent(FVector(WaveThickness * 0.5f, FMath::Max(1.0f, InWidth) * 0.5f, WaveHeight * 0.5f));
	Visual->SetRelativeScale3D(FVector(WaveThickness / 100.0f, FMath::Max(1.0f, InWidth) / 100.0f, WaveHeight / 100.0f));
	SetActorRotation(Direction.Rotation());
	Movement->InitialSpeed = Speed;
	Movement->MaxSpeed = Speed;
	Movement->Velocity = Direction * Speed;
	Collision->IgnoreActorWhenMoving(InSamurai, true);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	const float Duration = FMath::Max(0.01f, FMath::Max(1.0f, InTravelDistance) / Speed);
	GetWorldTimerManager().SetTimer(PhaseTimer, this, bReturns ? &ASamuraiBladeWave::BeginReturn : &ASamuraiBladeWave::FinishWave, Duration, false);
	OnOutboundStarted.Broadcast(this);
}

void ASamuraiBladeWave::HandleOverlap(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	AEnemyBase* Enemy = Cast<AEnemyBase>(Other);
	if (!Enemy || Enemy->IsDead() || HitThisPhase.Contains(Enemy) || !SourceSamurai.IsValid()) return;
	if (!Enemy->CanReceivePlayerDamage(EPlayerAttackSource::Samurai)) return;
	HitThisPhase.Add(Enemy);
	UHealthComponent* Health = Enemy->GetHealthComponent();
	const bool bApplied = Enemy->ApplyPlayerDamage(Damage, EPlayerAttackSource::Samurai);
	UPlayerUpgradeComponent* Upgrades = SourceUpgrades.Get();
	if (bApplied && Health && !Health->IsDead() && Upgrades && Upgrades->HasUpgradeId(TEXT("BleedingEdge")))
		Enemy->ApplyStatus(EEnemyStatusEffect::Bleed, Upgrades, EPlayerAttackSource::Samurai);
	if (Upgrades && Upgrades->HasUpgradeId(TEXT("MarkedBlade"))) Enemy->ApplyMark();
}

void ASamuraiBladeWave::BeginReturn()
{
	if (!SourceSamurai.IsValid()) { FinishWave(); return; }
	bReturning = true;
	HitThisPhase.Reset();
	FVector Direction = SourceSamurai->GetActorLocation() - GetActorLocation();
	Direction.Z = 0.0f;
	const float Distance = Direction.Size();
	if (!Direction.Normalize() || Distance <= KINDA_SMALL_NUMBER) { FinishWave(); return; }
	SetActorRotation(Direction.Rotation());
	Movement->Velocity = Direction * Speed;
	GetWorldTimerManager().SetTimer(PhaseTimer, this, &ASamuraiBladeWave::FinishWave, FMath::Max(0.01f, Distance / Speed), false);
	OnReturnStarted.Broadcast(this);
}

void ASamuraiBladeWave::FinishWave()
{
	GetWorldTimerManager().ClearTimer(PhaseTimer);
	OnWaveFinished.Broadcast(this);
	Destroy();
}

void ASamuraiBladeWave::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(PhaseTimer);
	Super::EndPlay(Reason);
}
