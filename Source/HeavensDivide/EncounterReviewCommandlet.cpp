#include "EncounterReviewCommandlet.h"
#if WITH_EDITOR
#include "FileHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "CharacterBase.h"
#include "SurvivorPlayerController.h"
#include "TacticalEnemy.h"
#include "CinderOracleBoss.h"
#include "EncounterDirector.h"
#include "EncounterHazard.h"
#include "EnemySpawner.h"
#include "HealingUrn.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Camera/PlayerCameraManager.h"
#include "AssetCompilingManager.h"
#include "NavigationSystem.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "TextureResource.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"
#include "ContentStreaming.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#endif
UEncounterReviewCommandlet::UEncounterReviewCommandlet() { IsClient=true;IsServer=false;IsEditor=true;LogToConsole=true; }
int32 UEncounterReviewCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
 auto* W=UEditorLoadingAndSavingUtils::LoadMap(TEXT("/Game/Maps/Lvl_B1_Lvl1"));if(!W)return 1;
 W->FlushLevelStreaming(EFlushLevelStreamingType::Full);
 W->WorldType=EWorldType::Game;
 // Editor map loading in a commandlet intentionally omits render/physics scenes.
 // Reinitialize this unsaved review world before evaluating collision or meshes.
 if(!W->Scene || !W->GetPhysicsScene())
 {
  W->CleanupWorld(false,true);
  W->InitWorld(UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).EnableTraceCollision(true));
  W->UpdateWorldComponents(true,false);
  W->FlushLevelStreaming(EFlushLevelStreamingType::Full);
 }
 W->InitializeActorsForPlay(FURL());
 FAssetCompilingManager::Get().FinishAllCompilation();
 FVector Center=FVector::ZeroVector;
 for(TActorIterator<APlayerStart> It(W);It;++It){Center=It->GetActorLocation();break;}
 auto Ground=[&](FVector P,float Height)
 {
  FHitResult H;
  if(W->LineTraceSingleByChannel(H,P+FVector(0,0,1500),P-FVector(0,0,3000),ECC_GameTraceChannel2)) P.Z=H.ImpactPoint.Z+Height;
  return P;
 };
 FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* PC=W->SpawnActor<ASurvivorPlayerController>(FVector::ZeroVector,FRotator::ZeroRotator,Spawn);
 auto* Player=W->SpawnActor<ACharacterBase>(Center,FRotator::ZeroRotator,Spawn);PC->Possess(Player);
 if(PC->PlayerCameraManager)PC->PlayerCameraManager->SetActorRotation(FRotator(-55,0,0));
 AEncounterDirector* Director=nullptr;
 for(TActorIterator<AEncounterDirector> It(W);It;++It){Director=*It;break;}
 if(!Director)return 2;
 bool March=false,Urn=false;
 // Probe realistic spawn positions along the starting play space, not an empty fixture.
 for(int32 I=0;I<8&&!March;++I)
 {
  Player->SetActorLocation(Ground(Center+FVector(I*200,0,0),90));
  March=Director->SpawnMarch();
 }
 Urn=Director->SpawnUrn();
 for(TActorIterator<AHealingUrn> It(W);It;++It)It->DispatchBeginPlay();
 UE_LOG(LogTemp,Display,TEXT("ENCOUNTER_LEVEL_PROBE center=%s march=%d urn=%d"),*Center.ToString(),March,Urn);
 if(!FParse::Param(*Params,TEXT("Capture")))return March&&Urn?0:3;

 for(TActorIterator<ATacticalEnemy> It(W);It;++It)It->Destroy();
 for(TActorIterator<AEncounterHazard> It(W);It;++It)It->Destroy();
 Player->Destroy();
 TArray<ATacticalEnemy*> Cast;
 const TCHAR* Names[]={TEXT("AshSeer"),TEXT("HexSniper"),TEXT("MireWeaver"),TEXT("HornLancer"),TEXT("FangStalker"),TEXT("GraveCantor"),TEXT("WarDrummer"),TEXT("OgreWarden"),TEXT("StormGorilla"),TEXT("FrostOracle")};
 for(int32 I=0;I<10;++I)
 {
  const FString Name=Names[I];const FString Path=TEXT("/Game/HeavensDivide/Blueprints/EnemyCharacters/Tactical/BP_")+Name+TEXT(".BP_")+Name+TEXT("_C");
  auto* Class=LoadClass<ATacticalEnemy>(nullptr,*Path);if(!Class)return 4;
  const float Half=Class->GetDefaultObject<ATacticalEnemy>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
  FVector P=Ground(Center+FVector((I/5-.5f)*550,(I%5-2)*360,0),Half+2);
  auto* E=W->SpawnActor<ATacticalEnemy>(Class,P,FRotator(0,180,0),Spawn);E->DispatchBeginPlay();
  E->SetGameplaySuspended(true);auto* M=E->GetMesh();M->bEnableUpdateRateOptimizations=false;
  M->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
  M->TickAnimation(.016f,false);M->RefreshBoneTransforms();M->MarkRenderDynamicDataDirty();Cast.Add(E);
 }
 auto* Owner=W->SpawnActor<AActor>();
 auto* Circle=AEncounterHazard::Spawn(Owner,Ground(Center+FVector(-50,0,0),2),EEncounterShape::Circle,180,0,1,0);
 auto* Lane=AEncounterHazard::Spawn(Owner,Ground(Center+FVector(350,600,0),2),EEncounterShape::Lane,750,140,1,0,.2f,90);
 if(Circle)Circle->Tick(.6f);if(Lane)Lane->Tick(.4f);
 auto* Camera=W->SpawnActor<AActor>();auto* Capture=NewObject<USceneCaptureComponent2D>(Camera);
 Camera->AddInstanceComponent(Capture);Capture->RegisterComponent();
 Capture->ProjectionType=ECameraProjectionMode::Perspective;Capture->FOVAngle=55;
 Capture->CaptureSource=ESceneCaptureSource::SCS_FinalColorLDR;
 Capture->bAlwaysPersistRenderingState=true;Capture->bCaptureEveryFrame=false;Capture->bCaptureOnMovement=false;
 Capture->PostProcessBlendWeight=0;
 Capture->SetWorldLocation(Center+FVector(-1800,0,2400));Capture->SetWorldRotation((Center+FVector(0,0,30)-Capture->GetComponentLocation()).Rotation());
 auto* Target=NewObject<UTextureRenderTarget2D>(Camera);Target->RenderTargetFormat=RTF_RGBA8;
 Target->InitAutoFormat(1600,1000);Target->UpdateResourceImmediate();Capture->TextureTarget=Target;
 FAssetCompilingManager::Get().FinishAllCompilation();if(GShaderCompilingManager)GShaderCompilingManager->FinishAllCompilation();
 IStreamingManager::Get().StreamAllResources(30.f);
 for(int32 Frame=0;Frame<12;++Frame)
 {++GFrameCounter;W->SendAllEndOfFrameUpdates();Capture->CaptureScene();FlushRenderingCommands();}
 TArray<FColor> Pixels;bool OK=Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
 int32 Visible=0;for(auto& P:Pixels){if(P.R>5||P.G>5||P.B>5)++Visible;P.A=255;}
 const FString Dir=FPaths::ProjectSavedDir()/TEXT("EncounterExpansion/Previews");IFileManager::Get().MakeDirectory(*Dir,true);
 TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(1600,1000,Pixels,PNG);
 OK &= FFileHelper::SaveArrayToFile(PNG,*(Dir/TEXT("encounters.png"))) && Visible>10000;
 UE_LOG(LogTemp,Display,TEXT("ENCOUNTER_REVIEW_RENDER ok=%d visible=%d"),OK,Visible);
 return OK&&March&&Urn?0:5;
#else
 return 1;
#endif
}
