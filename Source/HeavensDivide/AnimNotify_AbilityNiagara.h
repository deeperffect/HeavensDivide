#pragma once
#include "CoreMinimal.h"
#include "AnimNotify_PlayNiagaraEffect.h"
#include "AnimNotify_AbilityNiagara.generated.h"

/** Montage-authored ability effects share the character's real-time activation freeze. */
UCLASS(meta=(DisplayName="Ability Niagara (Freeze Aware)"))
class HEAVENSDIVIDE_API UAnimNotify_AbilityNiagara : public UAnimNotify_PlayNiagaraEffect
{
 GENERATED_BODY()
protected:
 virtual UFXSystemComponent* SpawnEffect(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
};
