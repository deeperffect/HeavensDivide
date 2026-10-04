#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "SurvivorAbilityComponent.h"
#include "UpgradeDefinition.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraDecalRendererProperties.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraSystemInstanceController.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraEmitterInstance.h"
#include "Engine/StaticMesh.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGroundSlashTrailTest,"HeavensDivide.Combat.GroundSlashTrailDirection",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGroundSlashTrailTest::RunTest(const FString&)
{
 auto* Upgrade=LoadObject<UUpgradeDefinition>(nullptr,TEXT("/Game/HeavensDivide/Upgrades/Samurai/DA_Upgrade_SamuraiCrescentStance.DA_Upgrade_SamuraiCrescentStance"));
 if(!TestNotNull(TEXT("Saved wave upgrade"),Upgrade)) return false;
 auto* System=Upgrade->Presentation.PulseSystem.Get();
 if(!TestNotNull(TEXT("Saved wave effect"),System)) return false;
 int32 Decals=0;
 for(const auto& Handle:System->GetEmitterHandles())
  if(Handle.GetName()==TEXT("DecalBrightTUT"))
   if(auto* Data=Handle.GetEmitterData())
   {
    TestFalse(TEXT("Ground marks stay in world space"),Data->bLocalSpace);
    for(auto* Renderer:Data->GetRenderers())
     if(auto* Decal=Cast<UNiagaraDecalRendererProperties>(Renderer))
     {
      ++Decals;
      TestEqual(TEXT("Saved renderer consumes heading quaternion"),
       Decal->DecalOrientationBinding.GetParamMapBindableVariable().GetName(),FName(TEXT("User.GroundTrailOrientation")));
     }
   }
 TestEqual(TEXT("Exactly one ground trail renderer checked"),Decals,1);
 auto* World=UWorld::CreateWorld(EWorldType::Game,false);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
 World->InitializeActorsForPlay(FURL());
 for(float Yaw:{0.f,45.f,90.f,180.f,270.f})
 {
  const FRotator Heading(0,Yaw,0);
  auto* Accent=World->SpawnActor<AAbilityAccent>(FVector(0,0,10),Heading);
  Accent->Initialize(Heading.Vector()*500,150,FLinearColor::White,5,false,&Upgrade->Presentation);
  Accent->SetActorRotation(Heading);
  Accent->StartGroundSlash(Upgrade->Presentation.DebrisMaterial,5);
  auto* Niagara=Accent->FindComponentByClass<UNiagaraComponent>();
  bool bValid=false;
  const FQuat Orientation=Niagara->GetVariableQuat(TEXT("User.GroundTrailOrientation"),bValid);
  TestTrue(TEXT("Runtime supplies trail orientation"),bValid);
  // Authored 150cm Y extent is the long ground axis; X projects onto the floor.
  TestTrue(TEXT("Long decal axis follows attack in every direction"),
   FMath::Abs(FVector::DotProduct(Orientation.RotateVector(FVector::YAxisVector),Heading.Vector()))>.999f);
  TestTrue(TEXT("Decal still projects down onto terrain"),
   Orientation.RotateVector(FVector::XAxisVector).Equals(-FVector::ZAxisVector,.001f));
  Accent->ReleaseGroundSlash(5);
  TestTrue(TEXT("Released trail keeps its original heading"),
   Niagara->GetVariableQuat(TEXT("User.GroundTrailOrientation"),bValid).Equals(Orientation,.001f));
 }
 World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGroundSlashFullEffectsTest,"HeavensDivide.Combat.GroundSlashFullEffects",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FGroundSlashFullEffectsTest::RunTest(const FString&)
{
 auto* Upgrade=LoadObject<UUpgradeDefinition>(nullptr,TEXT("/Game/HeavensDivide/Upgrades/Samurai/DA_Upgrade_SamuraiCrescentStance.DA_Upgrade_SamuraiCrescentStance"));
 if(!TestNotNull(TEXT("Saved upgrade"),Upgrade)) return false;
 auto* System=Upgrade->Presentation.PulseSystem.Get();
 if(!TestNotNull(TEXT("Saved effect"),System)) return false;
 int32 TrailCount=0, SparkCount=0, MeshCount=0;
 for(const auto& Handle:System->GetEmitterHandles())
 {
  const FString Name=Handle.GetName().ToString();
  const auto* Data=Handle.GetEmitterData();
  if(Name.StartsWith(TEXT("Trail")))
  {
   ++TrailCount;
   TestTrue(TEXT("Trail layer enabled"),Handle.GetIsEnabled());
   for(const auto& Event:Data->GetEventHandlers())
   {
    const auto* Source=System->GetEmitterHandles().FindByPredicate(
     [&Event](const FNiagaraEmitterHandle& Other){return Other.GetId()==Event.SourceEmitterID;});
    TestTrue(TEXT("Trail event points at its copied source"),Source && Source->GetName().ToString()==Name+TEXT("_origin"));
   }
  }
  if(Name==TEXT("StarParticles") || Name==TEXT("StarParticles002"))
  {
   ++SparkCount;
   TestTrue(TEXT("Spark layer enabled"),Handle.GetIsEnabled());
  }
  if(Name==TEXT("SlashMeshTUT"))
   for(auto* Renderer:Data->GetRenderers())
    if(auto* Mesh=Cast<UNiagaraMeshRendererProperties>(Renderer))
     for(const auto& Entry:Mesh->Meshes)
     {
      ++MeshCount;
      TestTrue(TEXT("Crescent is rolled horizontally"),Entry.Rotation.Equals(FRotator(0,0,90)));
      TestTrue(TEXT("Pivot correction follows mesh scale and direction"),Entry.PivotOffsetSpace==ENiagaraMeshPivotOffsetSpace::Mesh);
      if(TestNotNull(TEXT("Crescent mesh"),Entry.Mesh.Get()))
      {
       const FVector Center=Entry.Rotation.RotateVector(Entry.Mesh->GetBounds().Origin*Entry.Scale+Entry.PivotOffset);
       TestTrue(TEXT("Horizontal crescent is centered across the ground trail"),FMath::IsNearlyZero(Center.Y,.01));
       TestTrue(TEXT("Center stays at the authored ground height"),FMath::IsNearlyZero(Center.Z,.01));
      }
     }
 }
 TestEqual(TEXT("Four trails and four sources"),TrailCount,8);
 TestEqual(TEXT("Two original spark layers"),SparkCount,2);
 TestEqual(TEXT("One horizontal slash mesh"),MeshCount,1);
 const auto* LensFlare=IConsoleManager::Get().FindConsoleVariable(TEXT("r.LensFlareQuality"));
 TestTrue(TEXT("Project disables lens-flare post processing"),LensFlare && LensFlare->GetInt()==0);
 auto* World=UWorld::CreateWorld(EWorldType::Game,false);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
 World->InitializeActorsForPlay(FURL());
 auto* Accent=World->SpawnActor<AAbilityAccent>(FVector(0,0,10),FRotator::ZeroRotator);
 Accent->Initialize(FVector(650,0,10),150,FLinearColor::White,5,false,&Upgrade->Presentation);
 Accent->StartGroundSlash(Upgrade->Presentation.DebrisMaterial,5);
 auto* Niagara=Accent->FindComponentByClass<UNiagaraComponent>();
 TSet<FName> EmittingLayers;
 for(int32 Step=0;Step<40;++Step)
 {
  Accent->SetActorLocation(FVector(Step*15,0,10));
  Niagara->AdvanceSimulation(1,.02f);
  if(auto Controller=Niagara->GetSystemInstanceController())
   if(auto* Instance=Controller->GetSystemInstance_Unsafe())
    for(const auto& Emitter:Instance->GetEmitters())
     if(Emitter->GetNumParticles()>0) EmittingLayers.Add(Emitter->GetEmitterHandle().GetName());
 }
 for(const TCHAR* Name:{TEXT("Trail01"),TEXT("Trail02"),TEXT("Trail03"),TEXT("Trail04"),TEXT("StarParticles"),TEXT("StarParticles002")})
  TestTrue(FString::Printf(TEXT("%s actually emits particles"),Name),EmittingLayers.Contains(FName(Name)));
 World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
 return true;
}
#endif
