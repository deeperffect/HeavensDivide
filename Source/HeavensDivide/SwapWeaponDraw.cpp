#include "SwapPresentationComponent.h"
#include "AutoAttackComponent.h"
#include "SamuraiCharacter.h"
#include "Animation/AnimMontage.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"

void USwapPresentationComponent::StartEntranceWeaponDraw()
{
    RestoreEntranceWeapon();
    auto* Samurai=Cast<ASamuraiCharacter>(GetOwner());
    if(!bEnableArrivalWeaponDraw || !bEntranceOwnsPose || !Samurai || !EntranceMontage
        || !Samurai->GetMesh()->DoesSocketExist(WeaponBackSocket)) return;
    USceneComponent* Weapon=nullptr;
    if(ArrivalWeaponComponentName.IsNone() && Attack.IsValid()) Weapon=Attack->ResolveWeaponVisualComponent();
    if(!Weapon)
    {
        const FName Name=ArrivalWeaponComponentName.IsNone() ? FName(TEXT("Weapon")) : ArrivalWeaponComponentName;
        TInlineComponentArray<USceneComponent*> Components(Samurai);
        for(auto* Component:Components) if(Component->GetFName()==Name) { Weapon=Component; break; }
    }
    if(!Weapon || Weapon==Samurai->GetMesh() || Weapon==Samurai->GetVisualRoot() || !Weapon->GetAttachParent()) return;
    EntranceWeapon=Weapon;
    OriginalWeaponParent=Weapon->GetAttachParent();
    OriginalWeaponSocket=Weapon->GetAttachSocketName();
    OriginalWeaponTransform=Weapon->GetRelativeTransform();
    ActiveWeaponDrawTime=FMath::Clamp(ArrivalWeaponDrawTime,0.f,EntranceMontage->GetPlayLength());
    // Read only this cosmetic marker. Other montage gameplay notifies stay suppressed.
    bool bFoundMarker=false;
    for(const auto& Notify:EntranceMontage->Notifies)
        if(Notify.NotifyName==TEXT("SwapDrawWeapon") && (!bFoundMarker || Notify.GetTime()<ActiveWeaponDrawTime))
        {
            ActiveWeaponDrawTime=FMath::Clamp(static_cast<float>(Notify.GetTime()),0.f,EntranceMontage->GetPlayLength());
            bFoundMarker=true;
        }
    if(!Weapon->AttachToComponent(Samurai->GetMesh(),FAttachmentTransformRules::SnapToTargetNotIncludingScale,WeaponBackSocket))
    { RestoreEntranceWeapon(); return; }
    FTransform Offset=WeaponBackOffset;
    Offset.SetScale3D(Offset.GetScale3D()*OriginalWeaponTransform.GetScale3D());
    Weapon->SetRelativeTransform(Offset);
}

void USwapPresentationComponent::UpdateEntranceWeaponDraw()
{
    if(EntranceWeapon.IsValid() && EntrancePosition>=ActiveWeaponDrawTime) RestoreEntranceWeapon();
}

void USwapPresentationComponent::RestoreEntranceWeapon()
{
    if(EntranceWeapon.IsValid() && OriginalWeaponParent.IsValid())
    {
        EntranceWeapon->AttachToComponent(OriginalWeaponParent.Get(),FAttachmentTransformRules::KeepRelativeTransform,OriginalWeaponSocket);
        EntranceWeapon->SetRelativeTransform(OriginalWeaponTransform);
    }
    EntranceWeapon.Reset();
    OriginalWeaponParent.Reset();
}
