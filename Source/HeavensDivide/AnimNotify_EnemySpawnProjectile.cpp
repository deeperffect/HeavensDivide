// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNotify_EnemySpawnProjectile.h"

#include "Components/SkeletalMeshComponent.h"
#include "RangedEnemyBase.h"

void UAnimNotify_EnemySpawnProjectile::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	UE_LOG(LogTemp, Log, TEXT("AnimNotify_EnemySpawnProjectile fired"));

	if (!MeshComp)
	{
		return;
	}

	ARangedEnemyBase* RangedEnemy = Cast<ARangedEnemyBase>(MeshComp->GetOwner());
	if (!RangedEnemy)
	{
		UE_LOG(LogTemp, Warning, TEXT("AnimNotify_EnemySpawnProjectile: owner is not RangedEnemyBase."));
		return;
	}

	RangedEnemy->SpawnAttackProjectile();
}
