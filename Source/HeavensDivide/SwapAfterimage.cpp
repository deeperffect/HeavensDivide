#include "SwapAfterimage.h"
#include "CharacterBase.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/App.h"
ASwapAfterimage::ASwapAfterimage()
{
    PrimaryActorTick.bCanEverTick = true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    SetActorEnableCollision(false);
}
void ASwapAfterimage::Initialize(ACharacterBase* Source, UMaterialInterface* Material, FLinearColor Color, float Duration)
{
    if (!Source || !Source->GetMesh() || !Material) { Destroy(); return; }
    Lifetime = FMath::Max(.01f, Duration);
    SetLifeSpan(Lifetime + .05f);
    FadeMaterial = UMaterialInstanceDynamic::Create(Material, this);
    FadeMaterial->SetVectorParameterValue(TEXT("Tint"), Color);
    FadeMaterial->SetScalarParameterValue(TEXT("Opacity"), .65f);
    auto* Pose = NewObject<UPoseableMeshComponent>(this);
    Pose->SetSkinnedAssetAndUpdate(Source->GetMesh()->GetSkeletalMeshAsset());
    Pose->SetupAttachment(RootComponent);
    Pose->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Pose->SetCastShadow(false);
    Pose->RegisterComponent();
    Pose->SetWorldTransform(Source->GetMesh()->GetComponentTransform());
    Pose->CopyPoseFromSkeletalComponent(Source->GetMesh());
    for (int32 Slot=0; Slot<Pose->GetNumMaterials(); ++Slot) Pose->SetMaterial(Slot, FadeMaterial);
    TInlineComponentArray<UStaticMeshComponent*> Weapons(Source);
    for (auto* Mesh : Weapons)
    {
        if (!Mesh->GetStaticMesh() || !Mesh->IsVisible()) continue;
        auto* Copy = NewObject<UStaticMeshComponent>(this);
        Copy->SetStaticMesh(Mesh->GetStaticMesh());
        Copy->SetupAttachment(RootComponent);
        Copy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Copy->SetCastShadow(false);
        Copy->RegisterComponent();
        Copy->SetWorldTransform(Mesh->GetComponentTransform());
        for(int32 Slot=0;Slot<Copy->GetNumMaterials();++Slot) Copy->SetMaterial(Slot, FadeMaterial);
    }
}
void ASwapAfterimage::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    AdvanceVisual(AnimatedMesh || bRealTimeFade ? static_cast<float>(FApp::GetDeltaTime()) : DeltaSeconds);
}

void ASwapAfterimage::AdvanceVisual(float DeltaSeconds)
{
    if(AnimatedMesh && !bDepartureFinished)
    {
        // Actor ticks pause with the world, but the departure runs in real time
        // during swap dilation. Never boost the gameplay character's own tick.
        if(auto* Anim=AnimatedMesh->GetSingleNodeInstance())
        {
            Anim->SetPosition(FMath::Min(Age+DeltaSeconds,DepartureDuration)*DepartureRate,false);
            Anim->UpdateMontageWeightForTimeSkip(DeltaSeconds);
            AnimatedMesh->TickAnimation(0.f,false);
            AnimatedMesh->RefreshBoneTransforms();
        }
    }
    Age += DeltaSeconds;
    if (bPortalDash && AnimatedMesh)
    {
        const float Alpha = FMath::Clamp(Age / FMath::Max(.01f, DepartureDuration), 0.f, 1.f);
        SetActorLocation(FMath::Lerp(PortalStart, PortalEnd, Alpha * Alpha));
        if (Alpha >= 1.f) { Destroy(); return; }
        if (TrailDuration > 0 && Alpha >= .3f && TrailCount < 4 && Age - LastTrailAge >= .055f)
        {
            SpawnTrailSnapshot(); LastTrailAge = Age; ++TrailCount;
        }
    }
    if(AnimatedMesh && !bDepartureFinished && Age>=DepartureDuration)
    {
        bDepartureFinished=true;
        TInlineComponentArray<UMeshComponent*> Meshes(this);
        for(auto* Mesh:Meshes)
            for(int32 Slot=0;Slot<Mesh->GetNumMaterials();++Slot) Mesh->SetMaterial(Slot,FadeMaterial);
    }
    const float Fade=AnimatedMesh ? FMath::Clamp((Lifetime-Age)/FMath::Max(.01f,Lifetime-DepartureDuration),0.f,1.f) : FMath::Clamp(1-Age/Lifetime,0.f,1.f);
    if (FadeMaterial) FadeMaterial->SetScalarParameterValue(TEXT("Opacity"), (bRealTimeFade ? .35f : .65f) * FMath::Square(Fade));
    if(Age>=Lifetime) Destroy();
}

void ASwapAfterimage::SpawnTrailSnapshot()
{
    if (!AnimatedMesh || !FadeMaterial || !GetWorld()) return;
    FActorSpawnParameters Params; Params.Owner = GetOwner();
    auto* Trail = GetWorld()->SpawnActor<ASwapAfterimage>(GetActorLocation(),GetActorRotation(),Params);
    if (!Trail) return;
    Trail->Lifetime = TrailDuration;
    Trail->bRealTimeFade = true;
    Trail->FadeMaterial = UMaterialInstanceDynamic::Create(FadeMaterial,Trail);
    Trail->FadeMaterial->SetScalarParameterValue(TEXT("Opacity"),.35f);
    auto* Pose = NewObject<UPoseableMeshComponent>(Trail);
    Pose->SetupAttachment(Trail->GetRootComponent());
    Pose->SetSkinnedAssetAndUpdate(AnimatedMesh->GetSkeletalMeshAsset());
    Pose->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Pose->SetCastShadow(false);
    Pose->RegisterComponent();
    Pose->SetWorldTransform(AnimatedMesh->GetComponentTransform());
    Pose->CopyPoseFromSkeletalComponent(AnimatedMesh);
    for (int32 Slot=0;Slot<Pose->GetNumMaterials();++Slot) Pose->SetMaterial(Slot,Trail->FadeMaterial);
}

bool ASwapAfterimage::InitializeDeparture(ACharacterBase* Source,UMaterialInterface* Material,FLinearColor Color,
    UAnimMontage* Montage,float PlayRate,float FadeDuration)
{
    if(!Source || !Source->GetMesh() || !Source->GetMesh()->GetSkeletalMeshAsset() || !Material || !Montage) return false;
    if(Montage->GetPlayLength()<=0 || Montage->SlotAnimTracks.IsEmpty() || Montage->IsValidAdditive() || !Montage->GetSkeleton()
        || !Montage->GetSkeleton()->IsCompatibleMesh(Source->GetMesh()->GetSkeletalMeshAsset())) return false;
    // Match arrival: authored real-time speed, independent of the freeze window.
    DepartureRate=FMath::Max(.01f,PlayRate*Montage->RateScale);
    DepartureDuration=Montage->GetPlayLength()/DepartureRate;
    Lifetime=DepartureDuration+FMath::Max(.01f,FadeDuration);
    FadeMaterial=UMaterialInstanceDynamic::Create(Material,this);
    FadeMaterial->SetVectorParameterValue(TEXT("Tint"),Color);
    FadeMaterial->SetScalarParameterValue(TEXT("Opacity"),.65f);
    AnimatedMesh=NewObject<USkeletalMeshComponent>(this);
    AnimatedMesh->SetupAttachment(RootComponent);
    AnimatedMesh->SetSkeletalMeshAsset(Source->GetMesh()->GetSkeletalMeshAsset());
    AnimatedMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AnimatedMesh->SetCastShadow(false);
    AnimatedMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    AnimatedMesh->RegisterComponent();
    AnimatedMesh->SetWorldTransform(Source->GetMesh()->GetComponentTransform());
    AnimatedMesh->SetComponentTickEnabled(false); // Evaluated explicitly, without gameplay notifies.
    auto* Anim=AnimatedMesh->GetSingleNodeInstance();
    if(!Anim) { Destroy(); return false; }
    Anim->SetAnimationAsset(Montage,false,DepartureRate);
    Anim->SetPlaying(false);
    if(auto* Instance=Anim->GetActiveInstanceForMontage(Montage)) Instance->bEnableAutoBlendOut=false;
    Anim->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
    Anim->SetPosition(0.f,false);
    // Avoid a reference-pose flash before the first actor tick.
    Anim->UpdateMontageWeightForTimeSkip(FMath::Max(.01f,Montage->BlendIn.GetBlendTime()));
    AnimatedMesh->TickAnimation(0.f,false);
    AnimatedMesh->RefreshBoneTransforms();
    for(int32 Slot=0;Slot<AnimatedMesh->GetNumMaterials();++Slot) AnimatedMesh->SetMaterial(Slot,Source->GetMesh()->GetMaterial(Slot));
    TInlineComponentArray<UStaticMeshComponent*> Weapons(Source);
    for(auto* Mesh:Weapons)
    {
        if(!Mesh->GetStaticMesh() || !Mesh->IsVisible()) continue;
        auto* Copy=NewObject<UStaticMeshComponent>(this);
        Copy->SetStaticMesh(Mesh->GetStaticMesh());
        USceneComponent* Attachment=Mesh;
        while(Attachment->GetAttachParent() && Attachment->GetAttachParent()!=Source->GetMesh()) Attachment=Attachment->GetAttachParent();
        const bool OnCharacterMesh=Attachment->GetAttachParent()==Source->GetMesh();
        const FName Socket=OnCharacterMesh ? Attachment->GetAttachSocketName() : NAME_None;
        Copy->SetupAttachment(OnCharacterMesh ? AnimatedMesh.Get() : RootComponent.Get(),Socket);
        Copy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Copy->SetCastShadow(false);
        Copy->RegisterComponent();
        if(OnCharacterMesh) Copy->SetRelativeTransform(Mesh->GetComponentTransform().GetRelativeTransform(Source->GetMesh()->GetSocketTransform(Socket)));
        else Copy->SetWorldTransform(Mesh->GetComponentTransform());
        for(int32 Slot=0;Slot<Copy->GetNumMaterials();++Slot) Copy->SetMaterial(Slot,Mesh->GetMaterial(Slot));
    }
    return true;
}
