// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemySpawnArea.h"

#include "Components/BoxComponent.h"

AEnemySpawnArea::AEnemySpawnArea()
{
	PrimaryActorTick.bCanEverTick = false;

	SpawnBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("SpawnBounds"));
	SetRootComponent(SpawnBounds);
	SpawnBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpawnBounds->SetBoxExtent(FVector(5000.0f, 5000.0f, 500.0f));
}

bool AEnemySpawnArea::ContainsSpawnLocation(const FVector& WorldLocation, float EdgePadding) const
{
	if (!SpawnBounds)
	{
		return false;
	}

	const FVector LocalLocation = SpawnBounds->GetComponentTransform().InverseTransformPosition(WorldLocation);
	const FVector BoxExtent = SpawnBounds->GetUnscaledBoxExtent();
	const float SafePadding = FMath::Max(0.0f, EdgePadding);
	const FVector PaddedExtent(
		FMath::Max(0.0f, BoxExtent.X - SafePadding),
		FMath::Max(0.0f, BoxExtent.Y - SafePadding),
		BoxExtent.Z);

	return FMath::Abs(LocalLocation.X) <= PaddedExtent.X
		&& FMath::Abs(LocalLocation.Y) <= PaddedExtent.Y;
}

FVector AEnemySpawnArea::GetRandomLocationInside(float EdgePadding) const
{
	if (!SpawnBounds)
	{
		return GetActorLocation();
	}

	const FVector BoxExtent = SpawnBounds->GetUnscaledBoxExtent();
	const float SafePadding = FMath::Max(0.0f, EdgePadding);
	const FVector PaddedExtent(
		FMath::Max(0.0f, BoxExtent.X - SafePadding),
		FMath::Max(0.0f, BoxExtent.Y - SafePadding),
		BoxExtent.Z);
	const FVector LocalLocation(
		FMath::FRandRange(-PaddedExtent.X, PaddedExtent.X),
		FMath::FRandRange(-PaddedExtent.Y, PaddedExtent.Y),
		0.0f);

	return SpawnBounds->GetComponentTransform().TransformPosition(LocalLocation);
}
