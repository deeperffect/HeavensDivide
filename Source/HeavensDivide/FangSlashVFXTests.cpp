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
#include "NinjaBuildComponent.h"
#include "NinjaCharacter.h"
#include "RenderingThread.h"
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFangSlashVFXTest,"HeavensDivide.Combat.FangSlashVFX",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FFangSlashVFXTest::RunTest(const FString&)
{
 auto* W=UWorld::CreateWorld(EWorldType::Game,false);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);W->InitializeActorsForPlay(FURL());W->BeginPlay();
 ON_SCOPE_EXIT {W->DestroyWorld(false);GEngine->DestroyWorldContext(W);};
 auto* Class=LoadClass<ANinjaCharacter>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Ninja.BP_Ninja_C"));
 auto* B=Class?Class->GetDefaultObject<ANinjaCharacter>()->FindComponentByClass<UNinjaBuildComponent>():nullptr;
 if(!TestNotNull(TEXT("Saved Ninja"),B))return false;
 auto* Circle=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/HeavensDivide/VFX/Stances/NS_FangSlash_360.NS_FangSlash_360"));
 if(!TestNotNull(TEXT("Full circle system"),Circle))return false;
 TestTrue(TEXT("Fang uses full circle slash"),B->FangReturnBurstVFX==Circle);
 auto* Original=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/Assets/VFX/SlashesV1/Particles/NiagaraSystems/NS_Slash_15.NS_Slash_15"));
 auto* Camera=W->SpawnActor<AActor>();
 auto* Capture=NewObject<USceneCaptureComponent2D>(Camera);Camera->AddInstanceComponent(Capture);Capture->RegisterComponent();
 Capture->SetWorldLocation(FVector(0,0,800));Capture->SetWorldRotation(FRotator(-90,0,0));
 Capture->ProjectionType=ECameraProjectionMode::Orthographic;Capture->OrthoWidth=700;
 Capture->ShowFlags.SetLighting(false);
 auto* Target=NewObject<UTextureRenderTarget2D>(Camera);Target->RenderTargetFormat=RTF_RGBA8;
 Target->ClearColor=FLinearColor(.035f,.035f,.045f,1);Target->InitAutoFormat(512,512);Target->UpdateResourceImmediate();Capture->TextureTarget=Target;
 for(int32 Pass=0;Pass<2;++Pass)
 for(auto* System:{Original,Circle})
 {
  if(!TestNotNull(TEXT("Preview system"),System))continue;
#if WITH_EDITORONLY_DATA
  System->WaitForCompilationComplete(true,false);
#endif
  FAssetCompilingManager::Get().FinishAllCompilation();
  auto* FX=UNiagaraFunctionLibrary::SpawnSystemAtLocation(W,System,FVector::ZeroVector,FRotator::ZeroRotator,FVector::OneVector,false,false,ENCPoolMethod::None,false);
  if(!TestNotNull(TEXT("Spawned preview"),FX))continue;
  FX->SetForceSolo(true);FX->Activate(true);FX->AdvanceSimulation(6,.02f);
  FAssetCompilingManager::Get().FinishAllCompilation();if(GShaderCompilingManager)GShaderCompilingManager->FinishAllCompilation();
  FX->UpdateBounds();FX->MarkRenderStateDirty();W->SendAllEndOfFrameUpdates();FlushRenderingCommands();
  for(int32 Frame=0;Frame<4;++Frame){++GFrameCounter;W->Tick(LEVELTICK_All,.001f);FX->MarkRenderDynamicDataDirty();W->SendAllEndOfFrameUpdates();FlushRenderingCommands();Capture->CaptureScene();FlushRenderingCommands();}
  if(Pass==1)UKismetRenderingLibrary::ExportRenderTarget(W,Target,FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("FangSlash15/Previews")),System==Circle?TEXT("Fang360.png"):TEXT("Original.png"));
  FX->AdvanceSimulation(300,.02f);TestTrue(TEXT("Slash finishes"),FX->IsComplete());FX->DestroyComponent();FlushRenderingCommands();
 }
 return true;
}
#endif
