#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_BossTelegraphStart.generated.h"
UCLASS(meta=(DisplayName="Boss Telegraph Start"))
class HEAVENSDIVIDE_API UAnimNotify_BossTelegraphStart : public UAnimNotify
{
	GENERATED_BODY()
public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
