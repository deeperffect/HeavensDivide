#include "SwapPresentationComponent.h"
#include "AutoAttackComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimNotifies/AnimNotify_PlaySound.h"

void USwapPresentationComponent::StartEntrance()
{
    StopEntrance();
    PlayedEntranceSounds.Reset();
    auto* Character=Cast<ACharacterBase>(GetOwner());
    auto* Mesh=Character ? Character->GetMesh() : nullptr;
    if(!Mesh || !EntranceMontage || EntranceMontage->HasRootMotion() || EntranceMontage->IsValidAdditive()
        || EntranceMontage->SlotAnimTracks.IsEmpty() || EntranceMontage->GetPlayLength()<=0
        || !EntranceMontage->GetSkeleton() || !Mesh->GetSkeletalMeshAsset()
        || !EntranceMontage->GetSkeleton()->IsCompatibleMesh(Mesh->GetSkeletalMeshAsset())) return;

    // Playback is authored in real seconds. A short freeze must never speed
    // up the montage or override a deliberately slower character/asset rate.
    EntranceRate=FMath::Max(.01f,EntrancePlayRate*EntranceMontage->RateScale);
    PreviousAnimationMode=static_cast<uint8>(Mesh->GetAnimationMode());
    bPreviousMeshTickEnabled=Mesh->IsComponentTickEnabled();
    bPreviousClothTickEnabled=Mesh->ClothTickFunction.IsTickFunctionEnabled();
    Mesh->WaitForExistingParallelClothSimulation_GameThread();
    Mesh->ClothTickFunction.SetTickFunctionEnable(false);
    Mesh->ResumeClothingSimulation();
    Mesh->ForceClothNextUpdateTeleportAndReset();
    PreviousVisibilityBasedAnimTickOption=static_cast<uint8>(Mesh->VisibilityBasedAnimTickOption);
    bEntranceOwnsPose=true;
    EntrancePosition=0;
    EntranceRemaining=EntranceMontage->GetPlayLength()/EntranceRate;
    // A cosmetic full-body entrance must not blend with idle, locomotion or
    // an attack slot. Scrub explicitly so no gameplay notifies/root motion run.
    Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    // Manual ticking does not refresh bRecentlyRendered. Cooked builds gate
    // render pose uploads on visibility; GIsEditor hides this restriction in PIE.
    Mesh->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Mesh->SetComponentTickEnabled(false);
    auto* Anim=Mesh->GetSingleNodeInstance();
    if(!Anim) { StopEntrance(); return; }
    Anim->SetAnimationAsset(EntranceMontage,false,EntranceRate);
    Anim->SetPlaying(false);
    Anim->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
    if(auto* Instance=Anim->GetActiveInstanceForMontage(EntranceMontage)) Instance->bEnableAutoBlendOut=false;
    StartEntranceWeaponDraw();
    UpdateEntrance(0);
}

void USwapPresentationComponent::UpdateEntrance(float RealDelta)
{
    if(!bEntranceOwnsPose) return;
    // Keep the evaluated final frame visible for one frame before returning
    // control to the Animation Blueprint and allowing attacks again.
    if(EntrancePosition>=EntranceMontage->GetPlayLength())
    {
        StopEntrance();
        if(Attack.IsValid() && !GetOwner()->IsActorBeingDestroyed()) Attack->StartAutoAttack();
        return;
    }
    auto* Character=Cast<ACharacterBase>(GetOwner());
    auto* Mesh=Character ? Character->GetMesh() : nullptr;
    auto* Anim=Mesh ? Mesh->GetSingleNodeInstance() : nullptr;
    if(!Anim) { StopEntrance(); return; }
    Mesh->WaitForExistingParallelClothSimulation_GameThread();
    EntrancePosition=FMath::Min(EntranceMontage->GetPlayLength(),EntrancePosition+FMath::Max(0.f,RealDelta)*EntranceRate);
    EntranceRemaining=FMath::Max(SMALL_NUMBER,(EntranceMontage->GetPlayLength()-EntrancePosition)/EntranceRate);
    Anim->SetPosition(EntrancePosition,false);
    Anim->UpdateMontageWeightForTimeSkip(FMath::Max(.01f,EntranceMontage->BlendIn.GetBlendTime()));
    Mesh->TickAnimation(0.f,false);
    Mesh->RefreshBoneTransforms();
    // The entrance owns pose evaluation while world time is frozen. Cloth must
    // use the same real-time clock, once per frame, after the new pose is ready.
    if (RealDelta > 0.f && Mesh->GetSkeletalMeshAsset()->GetMeshClothingAssets().Num() > 0)
    {
        const bool bPreviousWait = Mesh->bWaitForParallelClothTask;
        Mesh->bWaitForParallelClothTask = false; // There is no scheduled tick completion handle here.
        if (RealDelta > .1f) Mesh->ForceClothNextUpdateTeleportAndReset();
        Mesh->TickClothing(FMath::Min(RealDelta, 1.f / 30.f), Mesh->ClothTickFunction);
        Mesh->WaitForExistingParallelClothSimulation_GameThread();
        Mesh->bWaitForParallelClothTask = bPreviousWait;
        Mesh->MarkRenderDynamicDataDirty();
    }
    if (bEnableSound && bPlayEntranceSoundNotifies)
        for (int32 Index=0; Index<EntranceMontage->Notifies.Num(); ++Index)
        {
            const FAnimNotifyEvent& Event = EntranceMontage->Notifies[Index];
            if (!PlayedEntranceSounds.Contains(Index) && Event.GetTriggerTime() <= EntrancePosition)
                if (auto* Sound = Cast<UAnimNotify_PlaySound>(Event.Notify))
                {
                    PlayedEntranceSounds.Add(Index);
                    Sound->Notify(Mesh, EntranceMontage, FAnimNotifyEventReference(&Event, EntranceMontage));
                }
        }
    UpdateEntranceWeaponDraw();
}

void USwapPresentationComponent::StopEntrance()
{
    RestoreEntranceWeapon();
    ResetArrivalMovement();
    if(bEntranceOwnsPose)
        if(auto* Character=Cast<ACharacterBase>(GetOwner()))
            if(auto* Mesh=Character->GetMesh())
            {
                Mesh->SetAnimationMode(static_cast<EAnimationMode::Type>(PreviousAnimationMode));
                Mesh->VisibilityBasedAnimTickOption=static_cast<EVisibilityBasedAnimTickOption>(PreviousVisibilityBasedAnimTickOption);
                Mesh->SetComponentTickEnabled(bPreviousMeshTickEnabled);
                Mesh->ClothTickFunction.SetTickFunctionEnable(bPreviousClothTickEnabled);
            }
    bEntranceOwnsPose=false;
    EntranceRemaining=0;
    EntrancePosition=0;
}
