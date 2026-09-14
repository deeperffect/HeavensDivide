#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify_PlaySound.h"
#include "AnimNotify_NinjaThrowSound.generated.h"

/** Preserves the authored throw timing while selecting audio for Ninja's current stance. */
UCLASS(meta=(DisplayName="Ninja Throw Sound"))
class HEAVENSDIVIDE_API UAnimNotify_NinjaThrowSound : public UAnimNotify_PlaySound
{
    GENERATED_BODY()
public:
    USoundBase* ResolveSound(AActor* Owner) const;
    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
        const FAnimNotifyEventReference& EventReference) override;
};
