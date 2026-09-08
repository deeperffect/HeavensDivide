// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNotify_EnemyAttackHit.h"

#include "Components/SkeletalMeshComponent.h"
#include "MontageMeleeEnemyBase.h"

void UAnimNotify_EnemyAttackHit::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	AMontageMeleeEnemyBase* Enemy = Cast<AMontageMeleeEnemyBase>(MeshComp->GetOwner());
	if (!Enemy)
	{
		UE_LOG(LogTemp, Warning, TEXT("AnimNotify_EnemyAttackHit: owner is not MontageMeleeEnemyBase."));
		return;
	}

	Enemy->PerformAttackHit();
}
