// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNotify_SpawnAutoAttackProjectile.h"

#include "AutoAttackComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "ShadowClone.h"

void UAnimNotify_SpawnAutoAttackProjectile::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	UE_LOG(LogTemp, Log, TEXT("Spawn AutoAttack Projectile Notify Fired"));

	if (!MeshComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("Projectile notify skipped: MeshComp invalid."));
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
	{
		UE_LOG(LogTemp, Warning, TEXT("Projectile notify skipped: owning actor invalid."));
		return;
	}

	if (AShadowClone* ShadowClone = Cast<AShadowClone>(Owner))
	{
		ShadowClone->HandleAttackProjectileNotify();
		return;
	}

	UAutoAttackComponent* AutoAttackComponent = Owner->FindComponentByClass<UAutoAttackComponent>();
	if (!AutoAttackComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("Projectile notify skipped: %s has no AutoAttackComponent."), *GetNameSafe(Owner));
		return;
	}

	AutoAttackComponent->SpawnAutoAttackProjectile();
}
