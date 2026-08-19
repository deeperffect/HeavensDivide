// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNotify_SpawnSamuraiSlashNiagara.h"

#include "CharacterBase.h"
#include "CharacterStatsComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

UAnimNotify_SpawnSamuraiSlashNiagara::UAnimNotify_SpawnSamuraiSlashNiagara()
{
	Scale = FVector::OneVector;
}

void UAnimNotify_SpawnSamuraiSlashNiagara::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp || !NiagaraSystem)
	{
		return;
	}

	float AreaScale = 1.0f;
	if (const ACharacterBase* OwnerCharacter = Cast<ACharacterBase>(MeshComp->GetOwner()))
	{
		if (const UCharacterStatsComponent* CharacterStats = OwnerCharacter->GetCharacterStats())
		{
			AreaScale = CharacterStats->GetFinalAttackAreaMultiplier();
		}
	}

	UNiagaraComponent* NiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
		NiagaraSystem,
		MeshComp,
		AttachSocketName,
		LocationOffset,
		RotationOffset,
		EAttachLocation::KeepRelativeOffset,
		true,
		false,
		ENCPoolMethod::None,
		true);

	if (!NiagaraComponent)
	{
		return;
	}

	NiagaraComponent->SetVariableFloat(TEXT("User.AreaScale"), AreaScale);
	NiagaraComponent->SetRelativeScale3D(Scale);
	NiagaraComponent->Activate(true);
}
