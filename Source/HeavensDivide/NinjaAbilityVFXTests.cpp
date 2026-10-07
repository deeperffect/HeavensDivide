#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AnimNotify_AbilityNiagara.h"
#include "Animation/AnimMontage.h"
#include "NinjaCharacter.h"
#include "ComboAbilityComponent.h"
#include "SwapPresentationComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemInstanceController.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraDataSetAccessor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNinjaAbilityVFXTest,"HeavensDivide.Combat.NinjaAbilityVFX",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FNinjaAbilityVFXTest::RunTest(const FString&)
{
 auto* C=LoadClass<ANinjaCharacter>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Ninja.BP_Ninja_C"));
 if(!TestNotNull(TEXT("Saved Ninja"),C))return false;
 const auto& Settings=C->GetDefaultObject<ANinjaCharacter>()->ComboAbility;
 TestFalse(TEXT("Montage owns Ninja VFX; code slot disabled"),Settings.bEnableCodeVFX);
 auto* Montage=Settings.Montage.Get();if(!TestNotNull(TEXT("Ninja ability montage"),Montage))return false;
 TArray<UAnimNotify_AbilityNiagara*> Notifies;
 for(const auto& Event:Montage->Notifies)if(auto* Notify=Cast<UAnimNotify_AbilityNiagara>(Event.Notify)) {
  Notifies.Add(Notify);TestTrue(TEXT("Every slash occurs within the montage"),Event.GetTriggerTime()>0 && Event.GetTriggerTime()<Montage->GetPlayLength());
 }
 if(!TestEqual(TEXT("All four authored slash notifies retained"),Notifies.Num(),4))return false;
 auto* W=UWorld::CreateWorld(EWorldType::Game,false);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);W->InitializeActorsForPlay(FURL());
 ON_SCOPE_EXIT {W->DestroyWorld(false);GEngine->DestroyWorldContext(W);};
 auto* Ninja=W->SpawnActor<ANinjaCharacter>(C);
 ++GFrameCounter;W->Tick(LEVELTICK_All,.016f);
 TestTrue(TEXT("Ability freeze begins"),Ninja->SwapPresentation->StartAbilityFreeze(.5f,.12f));
 TSet<UNiagaraComponent*> Instances;
 TMap<FName,FVector3f> Baseline;
 TMap<FName,FQuat4f> Orientations;
 for(int32 I=0;I<4;++I) {
  auto* Notify=Notifies[I];
  TestFalse(TEXT("Slash takes a fixed facing at spawn"),bool(Notify->Attached));
  Ninja->GetMesh()->SetWorldRotation(FRotator(0,I*90.f,0));
  Notify->Notify(Ninja->GetMesh(),Montage,FAnimNotifyEventReference());
  auto* FX=Cast<UNiagaraComponent>(Notify->GetSpawnedEffect());
  if(!TestNotNull(TEXT("Each montage notify creates its own effect"),FX))continue;
  Instances.Add(FX);TestNull(TEXT("Slash cannot be bent by later character turns"),FX->GetAttachParent());
  TestTrue(TEXT("Authored notify scale applied before simulation"),FX->GetComponentScale().Equals(Notify->Scale));
  const FTransform Spawn=FX->GetComponentTransform();
  Ninja->AddActorWorldOffset(FVector(100,0,0));
  TestTrue(TEXT("An existing slash keeps its world transform"),FX->GetComponentTransform().Equals(Spawn));
  FX->SetForceSolo(true);FX->AdvanceSimulation(12,.01f);
  int32 Count=0,Slashes=0;
  if(auto Controller=FX->GetSystemInstanceController())if(auto* Instance=Controller->GetSystemInstance_Unsafe())
   for(const auto& Emitter:Instance->GetEmitters()) {
    Count+=Emitter->GetNumParticles();
    if(!Emitter->GetEmitterHandle().GetName().ToString().StartsWith(TEXT("SlashMesh"))||Emitter->GetNumParticles()==0)continue;
    auto Positions=FNiagaraDataSetAccessor<FNiagaraPosition>::CreateReader(Emitter->GetParticleData(),TEXT("Position"));
    if(!Positions.IsValid())continue;
    ++Slashes;const auto P=FVector3f(Positions.GetSafe(0,FNiagaraPosition(0,0,0)));
    const auto Name=Emitter->GetEmitterHandle().GetName();
    if(I==0)Baseline.Add(Name,P);
    else if(const auto* Original=Baseline.Find(Name))TestTrue(TEXT("Local slash trajectory is invariant under character facing"),P.Equals(*Original,.1f));
    auto Rotation=FNiagaraDataSetAccessor<FQuat4f>::CreateReader(Emitter->GetParticleData(),TEXT("MeshOrientation"));
    if(Rotation.IsValid()) {
     const auto Q=Rotation.GetSafe(0,FQuat4f::Identity);
     if(I==0)Orientations.Add(Name,Q);
     else if(const auto* Original=Orientations.Find(Name))TestTrue(TEXT("Local slash mesh alignment is invariant under facing"),Q.Equals(*Original,.01f));
    }
   }
  TestTrue(TEXT("Every slash notify emits particles"),Count>0);
  TestTrue(TEXT("Slash mesh actually simulates"),Slashes>0);
 }
 TestEqual(TEXT("Four separate instances survive together"),Instances.Num(),4);
 Ninja->SwapPresentation->FinishSwapFreeze(false);
 for(auto* FX:Instances){TestEqual(TEXT("Freeze completion restores Niagara playback speed"),FX->GetCustomTimeDilation(),1.f);FX->DestroyComponent();}
 return true;
}
#endif
