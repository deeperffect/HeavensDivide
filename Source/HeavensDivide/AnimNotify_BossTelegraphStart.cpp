#include "AnimNotify_BossTelegraphStart.h"
#include "Components/SkeletalMeshComponent.h"
#include "FinalBossBase.h"
void UAnimNotify_BossTelegraphStart::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	if (AFinalBossBase* Boss = MeshComp ? Cast<AFinalBossBase>(MeshComp->GetOwner()) : nullptr) Boss->HandleBossTelegraphStart();
}
