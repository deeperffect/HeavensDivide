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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara|Attachment", meta = (DisplayName = "Weapon Component Name", ToolTip = "Name of the Samurai StaticMeshComponent whose origin the effect follows."))
	FName WeaponComponentName = TEXT("Weapon");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara|Attachment", meta = (ToolTip = "Optional socket on the character skeletal mesh. None attaches to the weapon origin. When set, the effect follows this socket and still uses weapon scale."))
	FName SkeletonSocketName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara", meta = (ToolTip = "Local position offset from the selected skeleton socket or weapon origin."))
	FVector LocationOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara", meta = (ToolTip = "Local rotation offset from the selected skeleton socket or weapon origin."))
	FRotator RotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara", meta = (ToolTip = "Authored local scale for the Niagara component. Weapon scale is inherited at the weapon origin, or copied at spawn when using a skeleton socket."))
	FVector Scale = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara|Area Scaling", meta = (ClampMin = "0.0", UIMin = "0.0", ToolTip = "Strength of positive attack-area bonuses for this VFX. 2 doubles the bonus: 1.75x attack area becomes 2.5x VFX area scaling. Normal size and area reductions stay unchanged."))
	float AreaBonusScaleMultiplier = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara|Area Scaling", meta = (ToolTip = "Vector user parameter containing an unscaled local particle offset. Multiplied by the attack-area bonus at the weapon origin; set to zero when attaching at a skeleton socket so particles start there. Set to None to leave the parameter unchanged."))
	FName AreaScaledOffsetParameter = TEXT("User.EndParticle_Position");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara|Area Scaling", meta = (ToolTip = "Float user parameters multiplied by the VFX area bonus before activation. Only list values that Niagara does not already scale with owner scale. Lightning radius and sprite/ribbon sizes already use owner scale in the current trail."))
	TArray<FName> AreaScaledFloatParameters;
};
