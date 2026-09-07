// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNotifyState_SamuraiSlashNiagara.h"

#include "CharacterBase.h"
#include "CharacterStatsComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

UAnimNotifyState_SamuraiSlashNiagara::UAnimNotifyState_SamuraiSlashNiagara()
{
	Scale = FVector::OneVector;
}

void UAnimNotifyState_SamuraiSlashNiagara::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp)
	{
		return;
	}

	// Clean up an earlier activation before starting another on this mesh.
	if (TWeakObjectPtr<UNiagaraComponent>* Previous = ActiveEffects.Find(MeshComp))
	{
		if (Previous->IsValid()) Previous->Get()->Deactivate();
		ActiveEffects.Remove(MeshComp);
	}
	for (auto It = ActiveEffects.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || !It.Value().IsValid()) It.RemoveCurrent();
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

	if (!WeaponComponent && SkeletonSocketName.IsNone())
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Warning, TEXT("Samurai slash Niagara notify could not find StaticMeshComponent '%s' on %s."),
			*WeaponComponentName.ToString(), *GetNameSafe(OwnerActor));
#endif
		return;
	}

	if (!SkeletonSocketName.IsNone() && !MeshComp->DoesSocketExist(SkeletonSocketName))
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Warning, TEXT("Samurai slash Niagara notify could not find skeleton socket '%s' on %s."),
			*SkeletonSocketName.ToString(), *GetNameSafe(MeshComp));
#endif
		return;
	}

	const ACharacterBase* Character = Cast<ACharacterBase>(OwnerActor);
	const UCharacterStatsComponent* Stats = Character ? Character->GetCharacterStats() : nullptr;
	const float AreaMultiplier = Stats ? FMath::Max(0.0f, Stats->GetFinalAttackAreaMultiplier()) : 1.0f;
	const float VFXAreaMultiplier = AreaMultiplier > 1.0f
		? 1.0f + (AreaMultiplier - 1.0f) * FMath::Max(0.0f, AreaBonusScaleMultiplier)
		: AreaMultiplier;
	// Weapon scale already includes the normal area bonus. Apply only the extra ratio.
	const float AdditionalAreaScale = AreaMultiplier > UE_SMALL_NUMBER ? VFXAreaMultiplier / AreaMultiplier : 1.0f;
	USceneComponent* AttachComponent = SkeletonSocketName.IsNone() ? static_cast<USceneComponent*>(WeaponComponent) : MeshComp;
	FVector SpawnScale = Scale * AdditionalAreaScale;
	if (!SkeletonSocketName.IsNone())
	{
		// The skeleton is not enlarged with the weapon. Preserve VFX size without
		// moving the authored socket or scaling the character mesh.
		const FVector WeaponWorldScale = WeaponComponent ? WeaponComponent->GetComponentScale() : MeshComp->GetComponentScale() * AreaMultiplier;
		const FVector SocketWorldScale = MeshComp->GetSocketTransform(SkeletonSocketName).GetScale3D();
		SpawnScale *= WeaponWorldScale * FTransform::GetSafeScaleReciprocal(SocketWorldScale);
	}

	UNiagaraComponent* NiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
		NiagaraSystem,
		AttachComponent,
		SkeletonSocketName,
		LocationOffset,
		RotationOffset,
		SpawnScale,
		EAttachLocation::KeepRelativeOffset,
		true,
		ENCPoolMethod::None,
		false,
		true);

	if (!NiagaraComponent)
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Warning, TEXT("Samurai slash Niagara notify failed to spawn '%s' on %s."),
			*GetNameSafe(NiagaraSystem), *GetNameSafe(AttachComponent));
#endif
		return;
	}

	ActiveEffects.Add(MeshComp, NiagaraComponent);
	// A tip socket already supplies the particle origin; avoid adding the blade offset twice.
	if (!AreaScaledOffsetParameter.IsNone())
	{
		bool bHasOffset = false;
		const FVector AuthoredOffset = NiagaraComponent->GetVariableVec3(AreaScaledOffsetParameter, bHasOffset);
		if (bHasOffset)
		{
			NiagaraComponent->SetVariableVec3(AreaScaledOffsetParameter, SkeletonSocketName.IsNone() ? AuthoredOffset * VFXAreaMultiplier : FVector::ZeroVector);
		}
	}
	// Read each authored scalar once; repeated parameter names must not compound scale.
	// Secondary emitters retain the size they have without area upgrades.
	const FVector SecondaryEffectScale = VFXAreaMultiplier > UE_SMALL_NUMBER
		? NiagaraComponent->GetComponentScale() / VFXAreaMultiplier
		: Scale * MeshComp->GetComponentScale();
	NiagaraComponent->SetVariableVec3(TEXT("User.SecondaryEffectScale"), SecondaryEffectScale);
	TSet<FName> ScaledParameters;
	for (const FName ParameterName : AreaScaledFloatParameters)
	{
		if (ParameterName.IsNone() || ScaledParameters.Contains(ParameterName)) continue;
		ScaledParameters.Add(ParameterName);
		bool bHasValue = false;
		const float AuthoredValue = NiagaraComponent->GetVariableFloat(ParameterName, bHasValue);
		if (bHasValue)
		{
			NiagaraComponent->SetVariableFloat(ParameterName, AuthoredValue * VFXAreaMultiplier);
		}
	}
	NiagaraComponent->Activate(true);
}

void UAnimNotifyState_SamuraiSlashNiagara::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	TWeakObjectPtr<UNiagaraComponent> Effect;
	if (MeshComp && ActiveEffects.RemoveAndCopyValue(MeshComp, Effect) && Effect.IsValid())
	{
		if (bDestroyImmediately)
		{
			Effect->DestroyComponent();
		}
		else
		{
			Effect->Deactivate();
		}
	}
	Super::NotifyEnd(MeshComp, Animation, EventReference);
}
