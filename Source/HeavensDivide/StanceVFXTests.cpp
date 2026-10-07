#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemInstanceController.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraEmitterInstance.h"
#include "NinjaBuildComponent.h"
#include "NinjaCharacter.h"
#include "RenderingThread.h"
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "TextureResource.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStanceVFXTest,"HeavensDivide.Combat.StanceVFX",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FStanceVFXTest::RunTest(const FString&)
{
 auto* W=UWorld::CreateWorld(EWorldType::Game,false);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);W->InitializeActorsForPlay(FURL());W->BeginPlay();
 ON_SCOPE_EXIT {W->DestroyWorld(false);GEngine->DestroyWorldContext(W);};
 auto* C=LoadClass<ANinjaCharacter>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Ninja.BP_Ninja_C"));
 auto* B=C?C->GetDefaultObject<ANinjaCharacter>()->FindComponentByClass<UNinjaBuildComponent>():nullptr;
 if(!TestNotNull(TEXT("Saved Ninja build component"),B))return false;
 TestNotNull(TEXT("Toxic Ground assigned"),B->ToxicGroundVFX.Get());
 TestNotNull(TEXT("Venom Bloom assigned"),B->VenomBloomVFX.Get());
 TestTrue(TEXT("Poison clouds stay in a thin ground layer"),B->PoisonPoolVFXHeightScale<=.0051f);
 TestTrue(TEXT("Pool and Shrine effects differ"),B->ToxicGroundVFX!=B->VenomBloomVFX);
 TestNotNull(TEXT("Fang return burst assigned"),B->FangReturnBurstVFX.Get());
 TestNotNull(TEXT("Shuriken burst assigned"),B->ShurikenBurstVFX.Get());
 auto* Camera=W->SpawnActor<AActor>();
 auto* Capture=NewObject<USceneCaptureComponent2D>(Camera);Camera->AddInstanceComponent(Capture);Capture->RegisterComponent();
 Capture->SetWorldLocation(FVector(0,0,650));Capture->SetWorldRotation(FRotator(-90,0,0));
 Capture->ProjectionType=ECameraProjectionMode::Perspective;Capture->FOVAngle=65;
 Capture->CaptureSource=ESceneCaptureSource::SCS_SceneColorHDRNoAlpha;Capture->bAlwaysPersistRenderingState=true;Capture->bCaptureEveryFrame=false;Capture->bCaptureOnMovement=false;
 auto* Target=NewObject<UTextureRenderTarget2D>(Camera);Target->RenderTargetFormat=RTF_RGBA8;Target->ClearColor=FLinearColor(.035f,.035f,.045f,1);Target->InitAutoFormat(512,512);Target->UpdateResourceImmediate();Capture->TextureTarget=Target;
 // Opaque enemy-sized geometry must remain visible above the ground smoke.
 auto* Occluder=W->SpawnActor<AStaticMeshActor>(FVector(0,0,60),FRotator::ZeroRotator);
 Occluder->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
 Occluder->SetActorScale3D(FVector(.8f,.8f,1.2f));
 Capture->ShowFlags.SetLighting(false);
 const TCHAR* Names[]={TEXT("NS_BloodCritical"),TEXT("NS_NinjaCritical"),TEXT("NS_PoisonHit"),TEXT("NS_NinjaPressure"),TEXT("NS_GrindingHalt"),TEXT("NS_BloodRush"),TEXT("NS_ViperRush"),TEXT("NS_NinjaRadialBurst"),TEXT("NS_ToxicGround"),TEXT("NS_VenomBloom")};
 // Warm Niagara renderer resources before exporting the review pass.
 for(int32 Pass=0;Pass<2;++Pass)
 for(const TCHAR* Name:Names)
 {
  const FString Path=FString(TEXT("/Game/HeavensDivide/VFX/Stances/"))+Name;
  auto* System=LoadObject<UNiagaraSystem>(nullptr,*Path);
  if(!TestNotNull(Name,System))continue;
#if WITH_EDITORONLY_DATA
  System->WaitForCompilationComplete(true,false);
#endif
  FAssetCompilingManager::Get().FinishAllCompilation();
  const bool bPool=FString(Name).Contains(TEXT("Ground"))||FString(Name).Contains(TEXT("Bloom"));
  Occluder->SetActorHiddenInGame(!bPool);
  auto* FX=UNiagaraFunctionLibrary::SpawnSystemAtLocation(W,System,FVector::ZeroVector,FRotator::ZeroRotator,
   bPool?FVector(2.2f,2.2f,2.2f*B->PoisonPoolVFXHeightScale):FVector::OneVector,false,false,ENCPoolMethod::None,false);
  if(!TestNotNull(TEXT("Effect spawned"),FX))continue;
  FX->SetForceSolo(true);FX->Activate(true);FX->AdvanceSimulation(bPool?125:6,.02f);
  FAssetCompilingManager::Get().FinishAllCompilation();
  if(GShaderCompilingManager)GShaderCompilingManager->FinishAllCompilation();
  int32 Particles=0;
  if(auto Controller=FX->GetSystemInstanceController())if(auto* Instance=Controller->GetSystemInstance_Unsafe())
   for(const auto& Emitter:Instance->GetEmitters())Particles+=Emitter->GetNumParticles();
  TestTrue(*FString::Printf(TEXT("%s emits at %s"),Name,bPool?TEXT("2.5 seconds"):TEXT("0.12 seconds")),Particles>0);
  AddInfo(FString::Printf(TEXT("%s: %d particles"),Name,Particles));
  ++GFrameCounter;W->Tick(LEVELTICK_All,.001f);
  FX->SetVisibility(true);FX->UpdateBounds();FX->MarkRenderStateDirty();W->SendAllEndOfFrameUpdates();FlushRenderingCommands();
  FX->MarkRenderDynamicDataDirty();W->SendAllEndOfFrameUpdates();FlushRenderingCommands();
  // Give newly created Niagara scene proxies render frames before reading their capture.
  for(int32 Frame=0;Frame<3;++Frame) {
   ++GFrameCounter;W->Tick(LEVELTICK_All,.001f);FX->MarkRenderDynamicDataDirty();
   W->SendAllEndOfFrameUpdates();FlushRenderingCommands();Capture->CaptureScene();FlushRenderingCommands();
  }
  if(Pass==1 && bPool) {
   TArray<FColor> Pixels;Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
   if(TestTrue(TEXT("Pool occlusion capture has pixels"),Pixels.Num()==512*512)) {
    FX->SetVisibility(false);W->SendAllEndOfFrameUpdates();FlushRenderingCommands();Capture->CaptureScene();FlushRenderingCommands();
    TArray<FColor> Baseline;Target->GameThread_GetRenderTargetResource()->ReadPixels(Baseline);
    int32 Covered=0,BodyPixels=0;
    for(int32 Y=230;Y<282;++Y)for(int32 X=230;X<282;++X) {
     const int32 I=Y*512+X;const auto Base=Baseline[I];const auto Actual=Pixels[I];
     if(Base.R>20 && FMath::Abs(int32(Base.R)-int32(Base.G))<8 && FMath::Abs(int32(Base.G)-int32(Base.B))<8) {
      ++BodyPixels;if(FMath::Abs(int32(Actual.R)-int32(Base.R))>8 || FMath::Abs(int32(Actual.G)-int32(Base.G))>8 || FMath::Abs(int32(Actual.B)-int32(Base.B))>8)++Covered;
     }
    }
    TestTrue(TEXT("Opaque enemy silhouette stays visible above colored smoke"),BodyPixels>100 && Covered==0);
    AddInfo(FString::Printf(TEXT("Pool occlusion: %d covered of %d body pixels"),Covered,BodyPixels));
    FX->SetVisibility(true);W->SendAllEndOfFrameUpdates();FlushRenderingCommands();Capture->CaptureScene();FlushRenderingCommands();
   }
  }
  if(Pass==1) UKismetRenderingLibrary::ExportRenderTarget(W,Target,FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("StanceVFXAudit/Previews")),FString(Name)+TEXT(".png"));
  if(!bPool){FX->AdvanceSimulation(300,.02f);TestTrue(TEXT("Burst does not loop forever"),FX->IsComplete());}
  FX->DestroyComponent();FlushRenderingCommands();
 }
 return true;
}
#endif
