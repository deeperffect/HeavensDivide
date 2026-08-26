#include "AnimNotify_BossSpawnGroundCircle.h"
#include "Components/SkeletalMeshComponent.h"
#include "FinalBossBase.h"
void UAnimNotify_BossSpawnGroundCircle::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	if (AFinalBossBase* Boss = MeshComp ? Cast<AFinalBossBase>(MeshComp->GetOwner()) : nullptr) Boss->HandleBossSpawnGroundCircle();
}
