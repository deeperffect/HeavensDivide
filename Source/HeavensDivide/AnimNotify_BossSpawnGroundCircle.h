#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_BossSpawnGroundCircle.generated.h"
UCLASS(meta=(DisplayName="Boss Spawn Ground Circle"))
class HEAVENSDIVIDE_API UAnimNotify_BossSpawnGroundCircle : public UAnimNotify
{
	GENERATED_BODY()
public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
