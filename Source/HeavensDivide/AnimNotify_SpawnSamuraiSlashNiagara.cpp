// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNotify_SpawnSamuraiSlashNiagara.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

UAnimNotify_SpawnSamuraiSlashNiagara::UAnimNotify_SpawnSamuraiSlashNiagara()
{
	Scale = FVector::OneVector;
	AttachSocketName = TEXT("SwordTip");
}

void UAnimNotify_SpawnSamuraiSlashNiagara::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!NiagaraSystem)
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Warning, TEXT("Samurai slash Niagara notify on %s has no Niagara System assigned."), *GetNameSafe(OwnerActor));
#endif
		return;
	}

	UStaticMeshComponent* WeaponComponent = nullptr;
	if (OwnerActor)
	{
		TArray<UStaticMeshComponent*> StaticMeshComponents;
		OwnerActor->GetComponents<UStaticMeshComponent>(StaticMeshComponents);
		for (UStaticMeshComponent* Candidate : StaticMeshComponents)
		{
			if (IsValid(Candidate) && Candidate->GetFName() == WeaponComponentName)
			{
				WeaponComponent = Candidate;
				break;
			}
		}
	}

	if (!WeaponComponent)
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Warning, TEXT("Samurai slash Niagara notify could not find StaticMeshComponent '%s' on %s."),
			*WeaponComponentName.ToString(), *GetNameSafe(OwnerActor));
#endif
		return;
	}

	if (AttachSocketName.IsNone() || !WeaponComponent->DoesSocketExist(AttachSocketName))
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Warning, TEXT("Samurai slash Niagara notify could not find socket '%s' on weapon component %s."),
			*AttachSocketName.ToString(), *GetNameSafe(WeaponComponent));
#endif
		return;
	}

	UNiagaraComponent* NiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
		NiagaraSystem,
		WeaponComponent,
		AttachSocketName,
		LocationOffset,
		RotationOffset,
		Scale,
		EAttachLocation::KeepRelativeOffset,
		true,
		ENCPoolMethod::None,
		false,
		true);

	if (!NiagaraComponent)
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Warning, TEXT("Samurai slash Niagara notify failed to spawn '%s' on %s.%s."),
			*GetNameSafe(NiagaraSystem), *GetNameSafe(WeaponComponent), *AttachSocketName.ToString());
#endif
		return;
	}

	NiagaraComponent->Activate(true);
}
