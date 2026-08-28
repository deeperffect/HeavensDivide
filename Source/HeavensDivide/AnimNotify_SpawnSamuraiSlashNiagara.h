// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_SpawnSamuraiSlashNiagara.generated.h"

class UNiagaraSystem;

UCLASS()
class HEAVENSDIVIDE_API UAnimNotify_SpawnSamuraiSlashNiagara : public UAnimNotify
{
	GENERATED_BODY()

public:
	UAnimNotify_SpawnSamuraiSlashNiagara();

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara", meta = (ToolTip = "Niagara system to spawn for the Samurai slash."))
	TObjectPtr<UNiagaraSystem> NiagaraSystem;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara|Attachment", meta = (DisplayName = "Weapon Component Name", ToolTip = "Name of the Samurai StaticMeshComponent that owns the trail attachment socket."))
	FName WeaponComponentName = TEXT("Weapon");

	// Keep the original property name so existing montage notify data remains serialized.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara|Attachment", meta = (DisplayName = "Weapon Socket Name", ToolTip = "Socket on the weapon Static Mesh to which the slash Niagara effect is attached."))
	FName AttachSocketName = TEXT("SwordTip");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara", meta = (ToolTip = "Local position offset from the attach socket."))
	FVector LocationOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara", meta = (ToolTip = "Local rotation offset from the attach socket."))
	FRotator RotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara", meta = (ToolTip = "Authored local scale for the Niagara component. Area scale is inherited from the weapon hierarchy."))
	FVector Scale = FVector::OneVector;
};
