// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNotify_TankSlamCommitFacing.h"

#include "Components/SkeletalMeshComponent.h"
#include "TankMeleeEnemyBase.h"

void UAnimNotify_TankSlamCommitFacing::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	ATankMeleeEnemyBase* TankEnemy = Cast<ATankMeleeEnemyBase>(MeshComp->GetOwner());
	if (!TankEnemy)
	{
		UE_LOG(LogTemp, Warning, TEXT("AnimNotify_TankSlamCommitFacing: owner is not TankMeleeEnemyBase."));
		return;
	}

	TankEnemy->CommitSlamFacing();
}
