#include "CharacterSimulationSetupCommandlet.h"
#if WITH_EDITOR
#include "Engine/SkeletalMesh.h"
#include "Engine/Blueprint.h"
#include "ClothingAssetFactory.h"
#include "ClothingAsset.h"
#include "ChaosCloth/ChaosClothConfig.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"
#include "Misc/PackageName.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "CharacterBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AssetCompilingManager.h"
#include <queue>

namespace CharacterSimulationSetup
{
bool Backup(UObject* Asset)
{
 const FString File=FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(),TEXT(".uasset"));
 const FString Dest=FPaths::ProjectSavedDir()/TEXT("Backups/CharacterSimulation20260928")/FPaths::GetCleanFilename(File);
 IFileManager::Get().MakeDirectory(*FPaths::GetPath(Dest),true);
 return IFileManager::Get().FileExists(*Dest)||IFileManager::Get().Copy(*Dest,*File)==COPY_OK;
}
bool Save(UObject* Asset)
{
 Asset->MarkPackageDirty();
 FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
 return UPackage::SavePackage(Asset->GetOutermost(),Asset,*FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(),TEXT(".uasset")),Args);
}
bool Bind(USkeletalMesh* Mesh,UClothingAssetCommon* Asset,int32 Section)
{
 FScopedSkeletalMeshPostEditChange Scope(Mesh);
 Mesh->Modify();
 if(auto* Current=Mesh->GetSectionClothingAsset(0,Section))Current->UnbindFromSkeletalMesh(Mesh,0,Section);
 if(!Asset->BindToSkeletalMesh(Mesh,0,Section,0))return false;
 auto& Model=Mesh->GetImportedModel()->LODModels[0];
 auto& UserData=Model.UserSectionsData.FindOrAdd(Model.Sections[Section].OriginalDataSectionIndex);
 UserData.CorrespondClothAssetIndex=Mesh->GetMeshClothingAssets().IndexOfByKey(Asset);
 UserData.ClothingData.AssetGuid=Asset->GetAssetGuid();
 UserData.ClothingData.AssetLodIndex=0;
 return true;
}
bool Cloth(USkeletalMesh* Mesh,float MaxTravel)
{
 if(!Mesh||!Mesh->GetImportedModel()||Mesh->GetMaterials().Num()<2||!Backup(Mesh))return false;
 Mesh->Modify();
 auto& Model=Mesh->GetImportedModel()->LODModels[0];
 TArray<int32> Sections;
 TArray<FVector3f> Body;
 for(int32 S=0;S<Model.Sections.Num();++S)
 {
  const auto& Section=Model.Sections[S];
  if(Section.MaterialIndex==1)Sections.Add(S);
  else for(const auto& V:Section.SoftVertices)Body.Add(V.Position);
 }
 if(Sections.IsEmpty())return false;
 for(int32 Section:Sections)
 {
  if(Model.Sections[Section].HasClothingData())
  {UE_LOG(LogTemp,Error,TEXT("Existing cloth on %s section %d; inspect before overwriting"),*Mesh->GetName(),Section);return false;}
  FSkeletalMeshClothBuildParams Build;
  Build.LodIndex=0;Build.SourceSection=Section;Build.bRemoveFromMesh=false;
  Build.AssetName=FString::Printf(TEXT("%s_Clothes_%d"),*Mesh->GetName(),Section);
  Build.PhysicsAsset=Mesh->GetPhysicsAsset();
  auto* Asset=Cast<UClothingAssetCommon>(NewObject<UClothingAssetFactory>()->CreateFromSkeletalMesh(Mesh,Build));
  if(!Asset)return false;
  Mesh->AddClothingAsset(Asset);
  auto& Lod=Asset->LodData[0];auto& Physical=Lod.PhysicalMeshData;
  const int32 N=Physical.Vertices.Num();
  TArray<TArray<int32>> Adj;Adj.SetNum(N);
  for(int32 I=0;I<Physical.Indices.Num();I+=3)
   for(int32 E=0;E<3;++E)
   {int32 A=Physical.Indices[I+E],B=Physical.Indices[I+(E+1)%3];Adj[A].AddUnique(B);Adj[B].AddUnique(A);}
  TArray<bool> Pinned;Pinned.Init(false,N);
  for(int32 I=0;I<N;++I)for(const FVector3f& B:Body)
   if(FVector3f::DistSquared(B,Physical.Vertices[I])<.0625f){Pinned[I]=true;break;}
  // Every disconnected panel needs its own anchors. Seam vertices take precedence;
  // a separate complete garment panel is attached along its top edge.
  TArray<bool> Seen;Seen.Init(false,N);int32 Panels=0,FallbackPanels=0;
  for(int32 Seed=0;Seed<N;++Seed)
  {
   if(Seen[Seed])continue;
   ++Panels;TArray<int32> Component;Component.Add(Seed);Seen[Seed]=true;
   float Top=-FLT_MAX,Bottom=FLT_MAX;int32 Anchors=0;
   for(int32 Q=0;Q<Component.Num();++Q)
   {
    int32 V=Component[Q];Top=FMath::Max(Top,Physical.Vertices[V].Z);Bottom=FMath::Min(Bottom,Physical.Vertices[V].Z);Anchors+=Pinned[V]?1:0;
    for(int32 B:Adj[V])if(!Seen[B]){Seen[B]=true;Component.Add(B);}
   }
   if(Anchors<2)
   {
    ++FallbackPanels;const float Band=FMath::Max(.75f,(Top-Bottom)*.12f);
    for(int32 V:Component)if(Physical.Vertices[V].Z>=Top-Band)Pinned[V]=true;
   }
  }
  TArray<float> Distance;Distance.Init(FLT_MAX,N);
  using Entry=std::pair<float,int32>;
  std::priority_queue<Entry,std::vector<Entry>,std::greater<Entry>> Queue;
  for(int32 I=0;I<N;++I)if(Pinned[I]){Distance[I]=0;Queue.push({0,I});}
  while(!Queue.empty())
  {
   auto [D,V]=Queue.top();Queue.pop();if(D>Distance[V])continue;
   for(int32 B:Adj[V])
   {float Next=D+FVector3f::Distance(Physical.Vertices[V],Physical.Vertices[B]);if(Next<Distance[B]){Distance[B]=Next;Queue.push({Next,B});}}
  }
  FPointWeightMap Mask(N);Mask.Name=TEXT("AttachmentToHem_MaxDistance_cm");Mask.CurrentTarget=(uint8)EWeightMapTargetCommon::MaxDistance;Mask.bEnabled=true;
  int32 Fixed=0,Moving=0;float Largest=0;
  FString CSV=TEXT("x,y,z,max_distance_cm\n");
  for(int32 I=0;I<N;++I)
  {
   if(!FMath::IsFinite(Distance[I])||Distance[I]==FLT_MAX)return false;
   Mask[I]=FMath::Clamp((Distance[I]-1.f)*.45f,0.f,MaxTravel);
   Fixed+=Mask[I]==0?1:0;Moving+=Mask[I]>0?1:0;Largest=FMath::Max(Largest,Mask[I]);
   const auto& P=Physical.Vertices[I];CSV+=FString::Printf(TEXT("%.3f,%.3f,%.3f,%.3f\n"),P.X,P.Y,P.Z,Mask[I]);
  }
  if(!Fixed||!Moving)return false;
  Lod.PointWeightMaps.Reset();Lod.PointWeightMaps.Add(Mask);Lod.bSmoothTransition=true;
  auto* Config=Asset->GetClothConfig<UChaosClothConfig>();
  if(!Config){Config=NewObject<UChaosClothConfig>(Asset);Asset->ClothConfigs.Add(UChaosClothConfig::StaticClass()->GetFName(),Config);}
  Config->EdgeStiffnessWeighted={1.f,1.f};Config->AreaStiffnessWeighted={1.f,1.f};
  Config->BendingStiffnessWeighted={.3f,.3f};Config->TetherStiffness={1.f,1.f};Config->TetherScale={1.f,1.f};
  Config->bUseGeodesicDistance=true;Config->DampingCoefficient=.25f;Config->LocalDampingCoefficient=.1f;
  Config->AnimDriveStiffness={.25f,.25f};Config->AnimDriveDamping={.5f,.5f};
  Config->CollisionThickness=.6f;Config->FrictionCoefficient=.3f;Config->bUseCCD=true;
  Config->GravityScale=.7f;Config->LinearVelocityScale=FVector(.35f);Config->AngularVelocityScale=.25f;
  Config->bUseSelfCollisions=false;
  auto* Shared=Asset->GetClothConfig<UChaosClothSharedSimConfig>();
  if(!Shared){Shared=NewObject<UChaosClothSharedSimConfig>(Asset);Asset->ClothConfigs.Add(UChaosClothSharedSimConfig::StaticClass()->GetFName(),Shared);}
  Shared->IterationCount=6;Shared->MaxIterationCount=12;Shared->SubdivisionCount=2;
  Asset->ApplyParameterMasks(true);Asset->InvalidateAllCachedData();
  if(!Bind(Mesh,Asset,Section))return false;
  const FString Dir=FPaths::ProjectSavedDir()/TEXT("CharacterSimulation");IFileManager::Get().MakeDirectory(*Dir,true);
  FFileHelper::SaveStringToFile(CSV,*(Dir/FString::Printf(TEXT("%s_%d_weights.csv"),*Mesh->GetName(),Section)));
  UE_LOG(LogTemp,Display,TEXT("CLOTH_SETUP %s section=%d vertices=%d pinned=%d moving=%d max=%.2f panels=%d fallback=%d"),*Mesh->GetName(),Section,N,Fixed,Moving,Largest,Panels,FallbackPanels);
 }
 Mesh->PostEditChange();return Save(Mesh);
}

bool Verify(const FString& OnlyCharacter=FString())
{
 auto* World=UWorld::CreateWorld(EWorldType::Game,false);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
 World->InitializeActorsForPlay(FURL());
 World->bShouldSimulatePhysics=true;
 bool Passed=true;
 for(const TCHAR* Name:{TEXT("Samurai"),TEXT("Ninja")})
 {
  if(!OnlyCharacter.IsEmpty()&&OnlyCharacter!=Name)continue;
  const FString Path=FString::Printf(TEXT("/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_%s.BP_%s_C"),Name,Name);
  auto* Class=LoadClass<ACharacterBase>(nullptr,*Path);
  if(!Class){Passed=false;continue;}
  FAssetCompilingManager::Get().FinishAllCompilation();
  FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  auto* Character=World->SpawnActor<ACharacterBase>(Class,FVector::ZeroVector,FRotator::ZeroRotator,Spawn);
  auto* Component=Character->GetMesh();auto* Mesh=Component->GetSkeletalMeshAsset();
  Character->SetCharacterMode(ECharacterMode::Active);
  Component->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
  Component->bEnableUpdateRateOptimizations=false;
  Component->SetForcedLOD(1);
  Component->InitAnim(true);
  UE_LOG(LogTemp,Display,TEXT("SIMULATION_START %s clothInstances=%d allowed=%d disabled=%d"),Name,Component->GetClothingSimulationInstances().Num(),Component->GetAllowClothActors(),Component->bDisableClothSimulation);
  const bool bExpectCloth=Mesh&&Mesh->GetMeshClothingAssets().Num()>0;
  bool Valid=Mesh&&Component->GetAnimInstance();
  float MaxHairRatio=0;int32 ClothSamples=0;double MaxClothRadius=0;
  FString CSV=TEXT("frame,hair_max_length_ratio,cloth_vertices,max_cloth_local_radius_cm\n");
  for(int32 Frame=0;Valid&&Frame<240;++Frame)
  {
   const float Dt=(Frame>=180&&Frame<210)?1.f/15.f:1.f/60.f;
   if(Frame==120)Character->SetCharacterMode(ECharacterMode::Inactive);
   FVector Location(Frame*10.f,100.f*FMath::Sin(Frame*.08f),0);
   if(Frame>=120)Location+=FVector(10000,10000,0);
   Character->SetActorLocation(Location,false,nullptr,Frame==120?ETeleportType::TeleportPhysics:ETeleportType::None);
   Character->SetVisualFacingRotation(FRotator(0,Frame>=60?Frame*17.f:0,0));
   if(Frame==120)Character->SetCharacterMode(ECharacterMode::Active);
   Character->GetCharacterMovement()->Velocity=Frame<60?FVector::ZeroVector:FVector(600,0,0);
   Component->TickAnimation(Dt,false);
   Component->RefreshBoneTransforms();
   // Manual component stepping: wait explicitly instead of attaching to a world tick completion event.
   Component->bWaitForParallelClothTask=false;
   Component->TickClothing(Dt,Component->ClothTickFunction);
   Component->WaitForExistingParallelClothSimulation_GameThread();
   Component->bWaitForParallelClothTask=true;
   const auto& Data=Component->GetCurrentClothingData_GameThread();
   int32 Vertices=0;
   for(const auto& Pair:Data)for(const auto& P:Pair.Value.Positions)
   {
    ++Vertices;
    // Solver output is relative to its reference bone, so translation/teleport must not inflate it.
    const double Radius=P.Size();MaxClothRadius=FMath::Max(MaxClothRadius,Radius);
    if(P.ContainsNaN()||!FMath::IsFinite(Radius)||Radius>500)Valid=false;
   }
   if(bExpectCloth&&Frame>5&&Vertices==0)Valid=false;
   ClothSamples+=Vertices;
   if(FCString::Strcmp(Name,TEXT("Ninja"))==0)
   {
    for(int32 I=2;I<=5;++I)
    {
     const FName Bone(*FString::Printf(TEXT("Ponytail%d"),I)),Parent(*FString::Printf(TEXT("Ponytail%d"),I-1));
     const int32 Index=Mesh->GetRefSkeleton().FindBoneIndex(Bone);
     if(Index==INDEX_NONE){Valid=false;break;}
     const double Rest=Mesh->GetRefSkeleton().GetRefBonePose()[Index].GetTranslation().Size();
     const double Current=FVector::Distance(Component->GetSocketTransform(Bone,RTS_Component).GetTranslation(),Component->GetSocketTransform(Parent,RTS_Component).GetTranslation());
     const float Ratio=Rest>UE_SMALL_NUMBER?Current/Rest:0;
     MaxHairRatio=FMath::Max(MaxHairRatio,Ratio);
     if(!FMath::IsFinite(Ratio)||Ratio>1.5f)Valid=false;
    }
   }
   CSV+=FString::Printf(TEXT("%d,%.4f,%d,%.3f\n"),Frame,MaxHairRatio,Vertices,MaxClothRadius);
  }
  const FString Dir=FPaths::ProjectSavedDir()/TEXT("CharacterSimulation");IFileManager::Get().MakeDirectory(*Dir,true);
  FFileHelper::SaveStringToFile(CSV,*(Dir/FString::Printf(TEXT("%s_stability.csv"),Name)));
  UE_LOG(LogTemp,Display,TEXT("CHARACTER_SIMULATION_VERIFY %s valid=%d clothSamples=%d maxHairLengthRatio=%.3f maxClothRadius=%.3f"),Name,Valid,ClothSamples,MaxHairRatio,MaxClothRadius);
  Passed&=Valid;Character->Destroy();
 }
 World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
 return Passed;
}
}
#endif

bool UCharacterSimulationSetupCommandlet::VerifySimulation(const FString& OnlyCharacter)
{
#if WITH_EDITOR
 return CharacterSimulationSetup::Verify(OnlyCharacter);
#else
 return false;
#endif
}
bool UCharacterSimulationSetupCommandlet::RepairClothBindings()
{
#if WITH_EDITOR
 for(const TCHAR* Path:{TEXT("/Game/Assets/PlayerCharacters/Samurai/fdsafdsa.fdsafdsa"),TEXT("/Game/Assets/PlayerCharacters/Ninja/NinjaCharacterV3.NinjaCharacterV3")})
 {
  auto* Mesh=LoadObject<USkeletalMesh>(nullptr,Path);
  if(!Mesh||Mesh->GetMeshClothingAssets().Num()!=1)return false;
  auto* Asset=Cast<UClothingAssetCommon>(Mesh->GetMeshClothingAssets()[0]);
  if(!Asset||!CharacterSimulationSetup::Bind(Mesh,Asset,1)||!CharacterSimulationSetup::Save(Mesh))return false;
 }
 return true;
#else
 return false;
#endif
}
bool UCharacterSimulationSetupCommandlet::RemoveGeneratedCloth()
{
#if WITH_EDITOR
 for(const TCHAR* Path:{TEXT("/Game/Assets/PlayerCharacters/Samurai/fdsafdsa.fdsafdsa"),TEXT("/Game/Assets/PlayerCharacters/Ninja/NinjaCharacterV3.NinjaCharacterV3")})
 {
  auto* Mesh=LoadObject<USkeletalMesh>(nullptr,Path);
  if(!Mesh)return false;
  {
   FScopedSkeletalMeshPostEditChange Scope(Mesh);
   Mesh->Modify();
   const TArray<TObjectPtr<UClothingAssetBase>> Assets=Mesh->GetMeshClothingAssets();
   for(const auto& Base:Assets)
   {
    auto* Asset=Cast<UClothingAssetCommon>(Base);
    if(!Asset||!Asset->GetName().StartsWith(Mesh->GetName()+TEXT("_Clothes")))continue;
    Asset->UnbindFromSkeletalMesh(Mesh,INDEX_NONE,INDEX_NONE);
    for(auto& Model:Mesh->GetImportedModel()->LODModels)
     for(auto& Pair:Model.UserSectionsData)
      if(Pair.Value.ClothingData.AssetGuid==Asset->GetAssetGuid())
      {Pair.Value.ClothingData=FClothingSectionData();Pair.Value.CorrespondClothAssetIndex=INDEX_NONE;}
    Mesh->GetMeshClothingAssets().Remove(Base);
   }
  }
  if(!CharacterSimulationSetup::Save(Mesh))return false;
  UE_LOG(LogTemp,Display,TEXT("GENERATED_CLOTH_REMOVED %s remaining=%d materials=%d bones=%d"),*Mesh->GetName(),Mesh->GetMeshClothingAssets().Num(),Mesh->GetMaterials().Num(),Mesh->GetRefSkeleton().GetNum());
 }
 return true;
#else
 return false;
#endif
}
// UE 5.8 skeletal render objects require a real scene even for offscreen simulation checks.
UCharacterSimulationSetupCommandlet::UCharacterSimulationSetupCommandlet(){IsClient=true;IsServer=false;IsEditor=true;LogToConsole=true;}
int32 UCharacterSimulationSetupCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
 using namespace CharacterSimulationSetup;
 if(Params.Contains(TEXT("Verify")))return Verify()?0:3;
 if(!Cloth(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Assets/PlayerCharacters/Samurai/fdsafdsa.fdsafdsa")),10.f))return 1;
 if(!Cloth(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Assets/PlayerCharacters/Ninja/NinjaCharacterV3.NinjaCharacterV3")),8.f))return 2;
 UE_LOG(LogTemp,Display,TEXT("CHARACTER_CLOTH_SETUP_PASS"));return 0;
#else
 return 1;
#endif
}
