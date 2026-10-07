#include "AnimNotify_AbilityNiagara.h"
#include "CharacterBase.h"
#include "SwapPresentationComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Engine/World.h"
#include "Misc/App.h"

UFXSystemComponent* UAnimNotify_AbilityNiagara::SpawnEffect(USkeletalMeshComponent* MeshComp,UAnimSequenceBase*)
{
 if(!MeshComp||!Template||Template->IsLooping())return nullptr;
 UNiagaraComponent* Effect=nullptr;
 if(Attached)
  Effect=UNiagaraFunctionLibrary::SpawnSystemAttached(Template,MeshComp,SocketName,LocationOffset,RotationOffset,
   EAttachLocation::KeepRelativeOffset,true,false,ENCPoolMethod::None,false);
 else {
  const FTransform Origin=MeshComp->GetSocketTransform(SocketName);
  Effect=UNiagaraFunctionLibrary::SpawnSystemAtLocation(MeshComp->GetWorld(),Template,
   Origin.TransformPosition(LocationOffset),(Origin.GetRotation()*RotationOffset.Quaternion()).Rotator(),
   FVector::OneVector,true,false,ENCPoolMethod::None,false);
 }
 if(!Effect)return nullptr;
 // Configure transforms and clocks before activation; each notify owns an independent instance.
 Effect->SetUsingAbsoluteScale(bAbsoluteScale);
 Effect->SetRelativeScale3D(Scale);
 if(auto* Character=Cast<ACharacterBase>(MeshComp->GetOwner()))if(Character->SwapPresentation) {
  Character->SwapPresentation->RegisterFreezeEffect(Effect);
  if(Character->SwapPresentation->IsSwapFreezeActive()&&MeshComp->GetWorld()->GetDeltaSeconds()>0)
   Effect->SetCustomTimeDilation(float(FApp::GetDeltaTime())/MeshComp->GetWorld()->GetDeltaSeconds());
 }
 Effect->Activate(true);
 return Effect;
}
