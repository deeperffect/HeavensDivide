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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara", meta = (ToolTip = "Niagara system to spawn for the Samurai slash. The notify sets User.AreaScale before activation."))
	TObjectPtr<UNiagaraSystem> NiagaraSystem;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara", meta = (ToolTip = "Socket or bone on the mesh to attach the slash Niagara effect to."))
	FName AttachSocketName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara", meta = (ToolTip = "Local position offset from the attach socket."))
	FVector LocationOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara", meta = (ToolTip = "Local rotation offset from the attach socket."))
	FRotator RotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara", meta = (ToolTip = "Local scale applied to the spawned Niagara component. Area scaling is still passed separately through User.AreaScale."))
	FVector Scale = FVector::OneVector;
};
