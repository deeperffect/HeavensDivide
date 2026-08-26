#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_BossAttackExecute.generated.h"
UCLASS(meta=(DisplayName="Boss Attack Execute"))
class HEAVENSDIVIDE_API UAnimNotify_BossAttackExecute : public UAnimNotify
{
	GENERATED_BODY()
public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
