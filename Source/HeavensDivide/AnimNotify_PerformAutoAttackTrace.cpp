// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNotify_PerformAutoAttackTrace.h"

#include "AutoAttackComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

void UAnimNotify_PerformAutoAttackTrace::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	UE_LOG(LogTemp, Log, TEXT("Attack Trace Notify Fired"));

	if (!MeshComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("Attack Trace Notify: MeshComp invalid."));
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
	{
		UE_LOG(LogTemp, Warning, TEXT("Attack Trace Notify: owning character invalid."));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Attack Trace Notify Owner: %s"), *GetNameSafe(Owner));

	UAutoAttackComponent* AutoAttackComponent = Owner->FindComponentByClass<UAutoAttackComponent>();
	if (!AutoAttackComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("AnimNotify_PerformAutoAttackTrace: %s has no AutoAttackComponent."), *GetNameSafe(Owner));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("AutoAttackComponent Found"));
	AutoAttackComponent->PerformAttackTrace();
}
