#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_BossTelegraphEnd.generated.h"
UCLASS(meta=(DisplayName="Boss Telegraph End"))
class HEAVENSDIVIDE_API UAnimNotify_BossTelegraphEnd : public UAnimNotify
{
	GENERATED_BODY()
public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
