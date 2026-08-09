// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemySpawner.h"

#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "EnemyBase.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "SurvivorPlayerController.h"

AEnemySpawner::AEnemySpawner()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AEnemySpawner::BeginPlay()
{
	Super::BeginPlay();

	if (bSpawningEnabled)
	{
		StartSpawning();
	}
}

void AEnemySpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopSpawning();

	for (TWeakObjectPtr<AEnemyBase>& EnemyPtr : SpawnedEnemies)
	{
		if (AEnemyBase* Enemy = EnemyPtr.Get())
		{
			Enemy->OnDestroyed.RemoveDynamic(this, &AEnemySpawner::HandleSpawnedEnemyDestroyed);
		}
	}

	SpawnedEnemies.Empty();

	Super::EndPlay(EndPlayReason);
}

void AEnemySpawner::StartSpawning()
{
	if (!bSpawningEnabled || SpawnInterval <= 0.0f)
	{
		return;
	}

	GetWorldTimerManager().SetTimer(SpawnTimerHandle, this, &AEnemySpawner::HandleSpawnTimerElapsed, SpawnInterval, true);
}

void AEnemySpawner::StopSpawning()
{
	GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
}

AEnemyBase* AEnemySpawner::SpawnEnemy()
{
	PruneTrackedEnemies();

	const int32 AliveEnemyCount = GetAliveEnemyCount();
	if (AliveEnemyCount >= MaxAliveEnemies)
	{
		if (bDebugSpawning)
		{
			UE_LOG(LogTemp, Log, TEXT("EnemySpawner %s skipped spawn. Alive=%d Max=%d"), *GetNameSafe(this), AliveEnemyCount, MaxAliveEnemies);
		}
		return nullptr;
	}

	const FEnemySpawnEntry* SpawnEntry = ChooseSpawnEntry();
	if (!SpawnEntry)
	{
		UE_LOG(LogTemp, Warning, TEXT("EnemySpawner %s has no valid enemy spawn entries."), *GetNameSafe(this));
		return nullptr;
	}

	ACharacterBase* ActivePlayer = GetActivePlayerCharacter();
	if (!ActivePlayer)
	{
		UE_LOG(LogTemp, Warning, TEXT("EnemySpawner %s could not find an active player character."), *GetNameSafe(this));
		return nullptr;
	}

	FVector SpawnLocation;
	if (!FindSpawnLocation(ActivePlayer->GetActorLocation(), SpawnLocation))
	{
		UE_LOG(LogTemp, Warning, TEXT("EnemySpawner %s could not find a valid spawn location."), *GetNameSafe(this));
		return nullptr;
	}

	if (const AEnemyBase* EnemyDefaultObject = SpawnEntry->EnemyClass->GetDefaultObject<AEnemyBase>())
	{
		const UCapsuleComponent* CapsuleComponent = EnemyDefaultObject->GetCapsuleComponent();
		if (CapsuleComponent)
		{
			SpawnLocation.Z += CapsuleComponent->GetScaledCapsuleHalfHeight();
		}
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AEnemyBase* SpawnedEnemy = GetWorld()->SpawnActor<AEnemyBase>(SpawnEntry->EnemyClass, SpawnLocation, FRotator::ZeroRotator, SpawnParameters);
	if (!SpawnedEnemy)
	{
		UE_LOG(LogTemp, Warning, TEXT("EnemySpawner %s failed to spawn %s."), *GetNameSafe(this), *GetNameSafe(SpawnEntry->EnemyClass.Get()));
		return nullptr;
	}

	SpawnedEnemy->SpawnDefaultController();
	SpawnedEnemy->OnDestroyed.AddDynamic(this, &AEnemySpawner::HandleSpawnedEnemyDestroyed);
	SpawnedEnemies.Add(SpawnedEnemy);

	if (bDebugSpawning)
	{
		UE_LOG(LogTemp, Log, TEXT("EnemySpawner spawned %s at %s. Alive=%d Max=%d"),
			*GetNameSafe(SpawnEntry->EnemyClass.Get()),
			*SpawnLocation.ToString(),
			GetAliveEnemyCount(),
			MaxAliveEnemies);

		DrawDebugSphere(GetWorld(), SpawnLocation, 40.0f, 16, FColor::Red, false, 2.0f, 0, 2.0f);
	}

	return SpawnedEnemy;
}

ACharacterBase* AEnemySpawner::GetActivePlayerCharacter() const
{
	const ASurvivorPlayerController* SurvivorController = Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	if (!SurvivorController)
	{
		return nullptr;
	}

	const UCharacterManagerComponent* CharacterManager = SurvivorController->GetCharacterManager();
	if (!CharacterManager)
	{
		return nullptr;
	}

	return CharacterManager->GetActiveCharacter();
}

const FEnemySpawnEntry* AEnemySpawner::ChooseSpawnEntry() const
{
	float TotalWeight = 0.0f;
	for (const FEnemySpawnEntry& Entry : EnemySpawnEntries)
	{
		if (Entry.bEnabled && Entry.EnemyClass && Entry.SpawnWeight > 0.0f)
		{
			TotalWeight += Entry.SpawnWeight;
		}
	}

	if (TotalWeight <= 0.0f)
	{
		return nullptr;
	}

	float Roll = FMath::FRandRange(0.0f, TotalWeight);
	for (const FEnemySpawnEntry& Entry : EnemySpawnEntries)
	{
		if (!Entry.bEnabled || !Entry.EnemyClass || Entry.SpawnWeight <= 0.0f)
		{
			continue;
		}

		Roll -= Entry.SpawnWeight;
		if (Roll <= 0.0f)
		{
			return &Entry;
		}
	}

	return nullptr;
}

bool AEnemySpawner::FindSpawnLocation(const FVector& ActivePlayerLocation, FVector& OutSpawnLocation) const
{
	const float SafeMaxRadius = FMath::Max(MinSpawnRadius, MaxSpawnRadius);
	const float SafeMinRadius = FMath::Min(MinSpawnRadius, SafeMaxRadius);
	const float AngleRadians = FMath::FRandRange(0.0f, 2.0f * PI);
	const float Distance = FMath::FRandRange(SafeMinRadius, SafeMaxRadius);
	const FVector SpawnOffset(FMath::Cos(AngleRadians) * Distance, FMath::Sin(AngleRadians) * Distance, 0.0f);
	const FVector CandidateLocation = ActivePlayerLocation + SpawnOffset;

	const FVector TraceStart = CandidateLocation + FVector(0.0f, 0.0f, GroundTraceHeight);
	const FVector TraceEnd = CandidateLocation - FVector(0.0f, 0.0f, GroundTraceDepth);

	FHitResult HitResult;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemySpawnerGroundTrace), false, this);
	const bool bHitGround = GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams);

	if (bDebugSpawning)
	{
		DrawDebugLine(GetWorld(), TraceStart, TraceEnd, bHitGround ? FColor::Green : FColor::Red, false, 2.0f, 0, 1.5f);
		DrawDebugCircle(GetWorld(), ActivePlayerLocation, MinSpawnRadius, 64, FColor::Yellow, false, 2.0f, 0, 1.0f, FVector::ForwardVector, FVector::RightVector);
		DrawDebugCircle(GetWorld(), ActivePlayerLocation, MaxSpawnRadius, 64, FColor::Orange, false, 2.0f, 0, 1.0f, FVector::ForwardVector, FVector::RightVector);
	}

	if (bHitGround)
	{
		OutSpawnLocation = HitResult.Location;
		return true;
	}

	OutSpawnLocation = CandidateLocation;
	return true;
}

int32 AEnemySpawner::GetAliveEnemyCount()
{
	PruneTrackedEnemies();

	int32 AliveCount = 0;
	for (const TWeakObjectPtr<AEnemyBase>& EnemyPtr : SpawnedEnemies)
	{
		const AEnemyBase* Enemy = EnemyPtr.Get();
		if (Enemy && !Enemy->IsDead())
		{
			++AliveCount;
		}
	}

	return AliveCount;
}

void AEnemySpawner::PruneTrackedEnemies()
{
	SpawnedEnemies.RemoveAll([](const TWeakObjectPtr<AEnemyBase>& EnemyPtr)
	{
		return !EnemyPtr.IsValid();
	});
}

void AEnemySpawner::HandleSpawnTimerElapsed()
{
	SpawnEnemy();
}

void AEnemySpawner::HandleSpawnedEnemyDestroyed(AActor* DestroyedActor)
{
	SpawnedEnemies.RemoveAll([DestroyedActor](const TWeakObjectPtr<AEnemyBase>& EnemyPtr)
	{
		return !EnemyPtr.IsValid() || EnemyPtr.Get() == DestroyedActor;
	});
}
