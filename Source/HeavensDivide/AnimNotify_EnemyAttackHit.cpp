// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNotify_EnemyAttackHit.h"

#include "Components/SkeletalMeshComponent.h"
#include "MeleeEnemyBase.h"

void UAnimNotify_EnemyAttackHit::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	UE_LOG(LogTemp, Log, TEXT("AnimNotify_EnemyAttackHit fired"));

	if (!MeshComp)
	{
		return;
	}

	AMeleeEnemyBase* Enemy = Cast<AMeleeEnemyBase>(MeshComp->GetOwner());
	if (!Enemy)
	{
		UE_LOG(LogTemp, Warning, TEXT("AnimNotify_EnemyAttackHit: owner is not MeleeEnemyBase."));
		return;
	}

	Enemy->PerformAttackHit();
}
