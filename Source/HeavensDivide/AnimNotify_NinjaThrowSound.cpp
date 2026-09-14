#include "AnimNotify_NinjaThrowSound.h"
#include "NinjaBuildComponent.h"
#include "NinjaCharacter.h"
#include "ShadowClone.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

USoundBase* UAnimNotify_NinjaThrowSound::ResolveSound(AActor* Owner) const
{
    if (const auto* Clone = Cast<AShadowClone>(Owner)) Owner = Clone->SourceNinja.Get();
    const auto* Build = Owner ? Owner->FindComponentByClass<UNinjaBuildComponent>() : nullptr;
    if (Build && Build->Has(TEXT("GreatShuriken")) && !Build->Has(TEXT("ReturningFang")) && Build->ShurikenThrowSound)
        return Build->ShurikenThrowSound;
    return Sound;
}

void UAnimNotify_NinjaThrowSound::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
    const FAnimNotifyEventReference& EventReference)
{
    USoundBase* Selected = ResolveSound(MeshComp ? MeshComp->GetOwner() : nullptr);
    if (Selected == Sound)
    {
        Super::Notify(MeshComp, Animation, EventReference);
        return;
    }
    if (!MeshComp || !Selected || Selected->IsLooping()) return;
    // Never mutate Sound on the shared notify: Ninja and clones can use it simultaneously.
    if (bFollow)
        UGameplayStatics::SpawnSoundAttached(Selected, MeshComp, AttachName, FVector::ZeroVector,
            EAttachLocation::KeepRelativeOffset, false, VolumeMultiplier, PitchMultiplier);
    else
        UGameplayStatics::PlaySoundAtLocation(MeshComp, Selected, MeshComp->GetComponentLocation(), VolumeMultiplier, PitchMultiplier);
}
