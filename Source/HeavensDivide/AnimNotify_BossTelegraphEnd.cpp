#include "AnimNotify_BossTelegraphEnd.h"
#include "Components/SkeletalMeshComponent.h"
#include "FinalBossBase.h"
void UAnimNotify_BossTelegraphEnd::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	if (AFinalBossBase* Boss = MeshComp ? Cast<AFinalBossBase>(MeshComp->GetOwner()) : nullptr) Boss->HandleBossTelegraphEnd();
}
