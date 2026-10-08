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
#include "PhysicsEngine/SkeletalBodySetup.h"
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
#include "AutoAttackComponent.h"
#include "Utils/ClothingMeshUtils.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "TextureResource.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"
#include "ContentStreaming.h"
#include <queue>

namespace CharacterSimulationSetup
{
bool Inspect(USkeletalMesh* Mesh)
{
 if(!Mesh||!Mesh->GetImportedModel())return false;
 FAssetCompilingManager::Get().FinishAllCompilation();
 const FString Dir=FPaths::ProjectSavedDir()/TEXT("CharacterSimulation/Pass2");
 IFileManager::Get().MakeDirectory(*Dir,true);
 const auto& Model=Mesh->GetImportedModel()->LODModels[0];
 FString Vertices=TEXT("index,section,material,x,y,z,nx,ny,nz,bones\n");
 FString Triangles=TEXT("section,a,b,c\n");
 for(int32 S=0;S<Model.Sections.Num();++S)
 {
  const auto& Section=Model.Sections[S];
  for(int32 I=0;I<Section.SoftVertices.Num();++I)
  {
   const auto& V=Section.SoftVertices[I];FString Bones;
   for(int32 J=0;J<MAX_TOTAL_INFLUENCES;++J)if(V.InfluenceWeights[J])
    Bones+=FString::Printf(TEXT("%s:%u;"),*Mesh->GetRefSkeleton().GetBoneName(Section.BoneMap[V.InfluenceBones[J]]).ToString(),V.InfluenceWeights[J]);
   Vertices+=FString::Printf(TEXT("%u,%d,%d,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%s\n"),Section.BaseVertexIndex+I,S,Section.MaterialIndex,V.Position.X,V.Position.Y,V.Position.Z,V.TangentZ.X,V.TangentZ.Y,V.TangentZ.Z,*Bones);
  }
  for(uint32 I=Section.BaseIndex;I<Section.BaseIndex+Section.NumTriangles*3;I+=3)
   Triangles+=FString::Printf(TEXT("%d,%u,%u,%u\n"),S,Model.IndexBuffer[I],Model.IndexBuffer[I+1],Model.IndexBuffer[I+2]);
 }
 FString Bones=TEXT("index,name,parent,x,y,z\n");TArray<FTransform> Poses;
 for(int32 I=0;I<Mesh->GetRefSkeleton().GetNum();++I)
 {
  const int32 Parent=Mesh->GetRefSkeleton().GetParentIndex(I);
  Poses.Add(Mesh->GetRefSkeleton().GetRefBonePose()[I]*(Parent<0?FTransform::Identity:Poses[Parent]));
  const auto P=Poses[I].GetTranslation();
  Bones+=FString::Printf(TEXT("%d,%s,%d,%.5f,%.5f,%.5f\n"),I,*Mesh->GetRefSkeleton().GetBoneName(I).ToString(),Parent,P.X,P.Y,P.Z);
 }
 FString Collision=TEXT("bone,shape,x,y,z,radius,length,ax,ay,az,bx,by,bz\n");
 if(auto* Physics=Mesh->GetPhysicsAsset())for(const auto& Body:Physics->SkeletalBodySetups)
 {
  const int32 Bone=Mesh->GetRefSkeleton().FindBoneIndex(Body->BoneName);if(Bone<0)continue;
  for(const auto& C:Body->AggGeom.SphylElems)
  {
   const FTransform Transform=C.GetTransform()*Poses[Bone];const FVector P=Transform.GetTranslation();
   const FVector A=Transform.TransformPosition(FVector(0,0,-C.Length*.5f)),B=Transform.TransformPosition(FVector(0,0,C.Length*.5f));
   Collision+=FString::Printf(TEXT("%s,capsule,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f\n"),*Body->BoneName.ToString(),P.X,P.Y,P.Z,C.Radius,C.Length,A.X,A.Y,A.Z,B.X,B.Y,B.Z);
  }
  for(const auto& C:Body->AggGeom.SphereElems)
  {
   const FVector P=Poses[Bone].TransformPosition(C.Center);
   Collision+=FString::Printf(TEXT("%s,sphere,%.5f,%.5f,%.5f,%.5f,0,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f\n"),*Body->BoneName.ToString(),P.X,P.Y,P.Z,C.Radius,P.X,P.Y,P.Z,P.X,P.Y,P.Z);
  }
  UE_LOG(LogTemp,Display,TEXT("CLOTH_COLLISION %s %s capsules=%d spheres=%d boxes=%d convex=%d"),*Mesh->GetName(),*Body->BoneName.ToString(),Body->AggGeom.SphylElems.Num(),Body->AggGeom.SphereElems.Num(),Body->AggGeom.BoxElems.Num(),Body->AggGeom.ConvexElems.Num());
 }
 bool OK=true;
 OK&=FFileHelper::SaveStringToFile(Vertices,*(Dir/(Mesh->GetName()+TEXT("_vertices.csv"))));
 OK&=FFileHelper::SaveStringToFile(Triangles,*(Dir/(Mesh->GetName()+TEXT("_triangles.csv"))));
 OK&=FFileHelper::SaveStringToFile(Bones,*(Dir/(Mesh->GetName()+TEXT("_bones.csv"))));
 OK&=FFileHelper::SaveStringToFile(Collision,*(Dir/(Mesh->GetName()+TEXT("_collision.csv"))));
 UE_LOG(LogTemp,Display,TEXT("CLOTH_INSPECT %s lods=%d sections=%d cloth=%d"),*Mesh->GetName(),Mesh->GetLODNum(),Model.Sections.Num(),Mesh->GetMeshClothingAssets().Num());
 return OK;
}

bool Backup(UObject* Asset)
{
 const FString File=FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(),TEXT(".uasset"));
 const FString Dest=FPaths::ProjectSavedDir()/TEXT("Backups/CharacterClothPass2_20261008/BeforeSetup")/FPaths::GetCleanFilename(File);
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
FClothVertBoneData BlendBoneData(const FClothVertBoneData& A,const FClothVertBoneData& B,float Alpha)
{
 TMap<uint16,float> Weights;
 for(int32 I=0;I<FClothVertBoneData::MaxTotalInfluences;++I)
 {
  if(A.BoneWeights[I]>0)Weights.FindOrAdd(A.BoneIndices[I])+=A.BoneWeights[I]*(1-Alpha);
  if(B.BoneWeights[I]>0)Weights.FindOrAdd(B.BoneIndices[I])+=B.BoneWeights[I]*Alpha;
 }
 TArray<TPair<uint16,float>> Sorted;for(const auto& P:Weights)if(P.Value>UE_SMALL_NUMBER)Sorted.Add(P);
 Sorted.Sort([](const auto& L,const auto& R){return L.Value>R.Value;});
 FClothVertBoneData Result;Result.NumInfluences=FMath::Min(Sorted.Num(),int32(FClothVertBoneData::MaxTotalInfluences));
 float Sum=0;for(int32 I=0;I<Result.NumInfluences;++I)Sum+=Sorted[I].Value;
 for(int32 I=0;I<Result.NumInfluences;++I){Result.BoneIndices[I]=Sorted[I].Key;Result.BoneWeights[I]=Sorted[I].Value/Sum;}
 return Result;
}

// Split shared edges together, preserving winding and avoiding simulation T-junctions.
// Imported render geometry and skin weights are never edited.
bool RefineCloth(FClothPhysicalMeshData& Mesh,TArray<bool>& Seam,float EdgeLimit)
{
 while(Mesh.Vertices.Num()<4000)
 {
  int32 A=INDEX_NONE,B=INDEX_NONE;float Longest=FMath::Square(EdgeLimit);
  for(int32 T=0;T<Mesh.Indices.Num();T+=3)for(int32 E=0;E<3;++E)
  {
   const int32 X=Mesh.Indices[T+E],Y=Mesh.Indices[T+(E+1)%3];
   const float Length=FVector3f::DistSquared(Mesh.Vertices[X],Mesh.Vertices[Y]);
   if(Length>Longest){A=X;B=Y;Longest=Length;}
  }
  if(A==INDEX_NONE)return true;
  const int32 Mid=Mesh.Vertices.Add((Mesh.Vertices[A]+Mesh.Vertices[B])*.5f);
  Mesh.Normals.Add((Mesh.Normals[A]+Mesh.Normals[B]).GetSafeNormal());
  Mesh.BoneData.Add(BlendBoneData(Mesh.BoneData[A],Mesh.BoneData[B],.5f));
  if(!Mesh.VertexColors.IsEmpty())Mesh.VertexColors.Add(FColor::White);
  Seam.Add(Seam[A]&&Seam[B]);
  const int32 Count=Mesh.Indices.Num();
  for(int32 T=0;T<Count;T+=3)for(int32 E=0;E<3;++E)
  {
   const int32 X=Mesh.Indices[T+E],Y=Mesh.Indices[T+(E+1)%3],Z=Mesh.Indices[T+(E+2)%3];
   if((X==A&&Y==B)||(X==B&&Y==A))
   {
    Mesh.Indices[T]=X;Mesh.Indices[T+1]=Mid;Mesh.Indices[T+2]=Z;
    Mesh.Indices.Add(Mid);Mesh.Indices.Add(Y);Mesh.Indices.Add(Z);break;
   }
  }
 }
 return false;
}

UPhysicsAsset* MakeClothCollision(USkeletalMesh* Mesh,bool bNinja)
{
 const FString Name=bNinja?TEXT("PA_NinjaCloth"):TEXT("PA_SamuraiCloth");
 const FString PackageName=FPackageName::GetLongPackagePath(Mesh->GetOutermost()->GetName())/Name;
 // A dedicated asset leaves gameplay/ragdoll collision untouched. Do not replace an artist's edit.
 if(FPackageName::DoesPackageExist(PackageName))
 {UE_LOG(LogTemp,Error,TEXT("Cloth collision already exists: %s. Inspect it before reauthoring."),*PackageName);return nullptr;}
 auto* Physics=NewObject<UPhysicsAsset>(CreatePackage(*PackageName),*Name,RF_Public|RF_Standalone);
 TArray<FTransform> Poses;const auto& Ref=Mesh->GetRefSkeleton();
 for(int32 I=0;I<Ref.GetNum();++I){const int32 Parent=Ref.GetParentIndex(I);Poses.Add(Ref.GetRefBonePose()[I]*(Parent<0?FTransform::Identity:Poses[Parent]));}
 auto Capsule=[&](const TCHAR* Bone,const TCHAR* EndBone,float Radius)
 {
  const int32 I=Ref.FindBoneIndex(Bone),J=Ref.FindBoneIndex(EndBone);if(I<0||J<0)return false;
  const FVector Start=Poses[I].GetTranslation(),End=Poses[J].GetTranslation();
  const FVector A=FMath::Lerp(Start,End,.2),B=FMath::Lerp(Start,End,.8);
  auto* Body=NewObject<USkeletalBodySetup>(Physics);Body->BoneName=Bone;Body->PhysicsType=PhysType_Kinematic;
  FKSphylElem Shape;Shape.Radius=Radius;Shape.Length=FVector::Distance(A,B);
  const FTransform WorldShape(FQuat::FindBetweenNormals(FVector::UpVector,(B-A).GetSafeNormal()),(A+B)*.5);
  Shape.SetTransform(WorldShape.GetRelativeTransform(Poses[I]));Body->AggGeom.SphylElems.Add(Shape);
  Physics->SkeletalBodySetups.Add(Body);return true;
 };
 bool OK=Capsule(TEXT("Hips"),TEXT("Spine"),bNinja?4.5f:9.f);
 OK&=Capsule(TEXT("Spine"),TEXT("Spine2"),bNinja?5.f:9.f);
 OK&=Capsule(TEXT("Spine2"),TEXT("Neck"),bNinja?4.5f:8.f);
 for(const TCHAR* Side:{TEXT("Left"),TEXT("Right")})
 {
  OK&=Capsule(*(FString(Side)+TEXT("Arm")),*(FString(Side)+TEXT("ForeArm")),bNinja?2.1f:4.f);
  OK&=Capsule(*(FString(Side)+TEXT("ForeArm")),*(FString(Side)+TEXT("Hand")),bNinja?1.7f:3.3f);
  OK&=Capsule(*(FString(Side)+TEXT("UpLeg")),*(FString(Side)+TEXT("Leg")),bNinja?3.7f:6.f);
  OK&=Capsule(*(FString(Side)+TEXT("Leg")),*(FString(Side)+TEXT("Foot")),bNinja?2.8f:4.5f);
 }
 Physics->SetPreviewMesh(Mesh,false);Physics->UpdateBodySetupIndexMap();Physics->UpdateBoundsBodiesArray();
 return OK?Physics:nullptr;
}

bool Cloth(USkeletalMesh* Mesh,float MaxTravel)
{
 if(!Mesh||!Mesh->GetImportedModel()||Mesh->GetMaterials().Num()!=2||!Backup(Mesh))return false;
 const bool bV6=Mesh->GetName()==TEXT("SK_NinjaV6");
 const bool bNinja=bV6||Mesh->GetName()==TEXT("NinjaCharacterV3");
 const FName Material=bV6?TEXT("M_NinjaV6_ScarfRibbons"):(bNinja?TEXT("CharacterClothes"):TEXT("SamuraiClothes"));
 const int32 MaterialIndex=Mesh->GetMaterials().IndexOfByPredicate([&](const FSkeletalMaterial& Slot){return Slot.MaterialSlotName==Material;});
 if(MaterialIndex==INDEX_NONE||!Mesh->GetMeshClothingAssets().IsEmpty())return false;
 Mesh->Modify();auto& Model=Mesh->GetImportedModel()->LODModels[0];
 TArray<int32> Sections;TArray<FVector3f> Body;
 for(int32 S=0;S<Model.Sections.Num();++S)
 {
  const auto& Section=Model.Sections[S];
  if(Section.MaterialIndex==MaterialIndex)Sections.Add(S);
  else for(const auto& V:Section.SoftVertices)Body.Add(V.Position);
 }
 if(Sections.Num()!=1)return false;
 auto* Physics=MakeClothCollision(Mesh,bNinja);if(!Physics)return false;
 for(int32 Section:Sections)
 {
  if(Model.Sections[Section].HasClothingData())return false;
  FSkeletalMeshClothBuildParams Build;
  Build.LodIndex=0;Build.SourceSection=Section;Build.bRemoveFromMesh=false;
  Build.AssetName=FString::Printf(TEXT("%s_Clothes_Pass2_%d"),*Mesh->GetName(),Section);Build.PhysicsAsset=Physics;
  auto* Asset=Cast<UClothingAssetCommon>(NewObject<UClothingAssetFactory>()->CreateFromSkeletalMesh(Mesh,Build));
  if(!Asset)return false;
  Mesh->AddClothingAsset(Asset);auto& Lod=Asset->LodData[0];auto& Physical=Lod.PhysicalMeshData;
  const int32 SourceCount=Physical.Vertices.Num();
  TArray<bool> Seam;Seam.Init(false,SourceCount);
  for(int32 I=0;I<SourceCount;++I)
  {
   // V6's two detached scarf panels overlap the neck wrap rather than sharing
   // its vertices. Their inspected attachment band is z=77.5..79.25 cm.
   if(bV6){Seam[I]=Physical.Vertices[I].Z>=77.5f;continue;}
   for(const FVector3f& B:Body)
    if(FVector3f::DistSquared(B,Physical.Vertices[I])<.1225f){Seam[I]=true;break;}
  }
  if(!RefineCloth(Physical,Seam,bNinja?3.f:5.f))return false;
  const int32 N=Physical.Vertices.Num();TArray<TArray<int32>> Adj;Adj.SetNum(N);
  for(int32 I=0;I<Physical.Indices.Num();I+=3)for(int32 E=0;E<3;++E)
  {const int32 A=Physical.Indices[I+E],B=Physical.Indices[I+(E+1)%3];Adj[A].AddUnique(B);Adj[B].AddUnique(A);}
  TArray<float> Distance;Distance.Init(FLT_MAX,N);
  using Entry=std::pair<float,int32>;std::priority_queue<Entry,std::vector<Entry>,std::greater<Entry>> Queue;
  for(int32 I=0;I<N;++I)if(Seam[I]){Distance[I]=0;Queue.push({0,I});}
  while(!Queue.empty())
  {
   auto [D,V]=Queue.top();Queue.pop();if(D>Distance[V])continue;
   for(int32 B:Adj[V]){const float Next=D+FVector3f::Distance(Physical.Vertices[V],Physical.Vertices[B]);if(Next<Distance[B]){Distance[B]=Next;Queue.push({Next,B});}}
  }
  // Every inspected ribbon must have a connected, explicitly identified attachment.
  const float PinBand=bNinja?1.6f:4.f;
  TArray<bool> Seen;Seen.Init(false,N);int32 Panels=0;
  for(int32 Seed=0;Seed<N;++Seed)
  {
   if(Seen[Seed])continue;
   ++Panels;TArray<int32> Panel;Panel.Add(Seed);Seen[Seed]=true;
   FClothVertBoneData Attachment;int32 Anchors=0;
   for(int32 Q=0;Q<Panel.Num();++Q)
   {
    const int32 V=Panel[Q];
    if(Seam[V]){Attachment=BlendBoneData(Attachment,Physical.BoneData[V],1.f/++Anchors);}
    for(int32 B:Adj[V])if(!Seen[B]){Seen[B]=true;Panel.Add(B);}
   }
   if(Anchors<2){UE_LOG(LogTemp,Error,TEXT("Unanchored cloth panel %d on %s"),Panels,*Mesh->GetName());return false;}
   // Imported nearest-body weights put some ribbon tips on unrelated leg bones.
   // Use their actual attachment's weights only in the simulation mesh, feathered
   // beyond the fixed band; the render mesh and attachment seam remain unchanged.
   for(int32 V:Panel)if(Distance[V]>PinBand)
    Physical.BoneData[V]=BlendBoneData(Physical.BoneData[V],Attachment,FMath::Clamp((Distance[V]-PinBand)/(PinBand*2.f),0.f,1.f));
  }
  if(Panels!=(bV6?2:(bNinja?3:4)))return false;
  FPointWeightMap MaxDistance(N),Drive(N);
  MaxDistance.Name=TEXT("Pass2_FixedSeam_To_FreeEdge_cm");MaxDistance.CurrentTarget=(uint8)EWeightMapTargetCommon::MaxDistance;MaxDistance.bEnabled=true;
  Drive.Name=TEXT("Pass2_Attachment_AnimationDrive");Drive.CurrentTarget=(uint8)EWeightMapTargetCommon::AnimDriveStiffness;Drive.bEnabled=true;
  int32 Fixed=0,Moving=0;FString CSV=TEXT("vertex,x,y,z,max_distance_cm,animation_drive_weight,seam_distance_cm\n");
  for(int32 I=0;I<N;++I)
  {
   if(!FMath::IsFinite(Distance[I])||Distance[I]==FLT_MAX)return false;
   const float T=FMath::Clamp((Distance[I]-PinBand)/(bNinja?10.f:22.f),0.f,1.f);
   MaxDistance[I]=FMath::Min(MaxTravel,FMath::Max(0.f,Distance[I]-PinBand)*1.75f)*T*T*(3.f-2.f*T);Drive[I]=1.f-T;
   Fixed+=MaxDistance[I]==0?1:0;Moving+=MaxDistance[I]>0?1:0;
   const auto& P=Physical.Vertices[I];CSV+=FString::Printf(TEXT("%d,%.5f,%.5f,%.5f,%.3f,%.3f,%.3f\n"),I,P.X,P.Y,P.Z,MaxDistance[I],Drive[I],Distance[I]);
  }
  if(!Fixed||!Moving)return false;
  Lod.PointWeightMaps.Reset();Lod.PointWeightMaps.Add(MaxDistance);Lod.PointWeightMaps.Add(Drive);
  Lod.bSmoothTransition=true;
  auto* Config=Asset->GetClothConfig<UChaosClothConfig>();
  if(!Config){Config=NewObject<UChaosClothConfig>(Asset);Asset->ClothConfigs.Add(UChaosClothConfig::StaticClass()->GetFName(),Config);}
  Config->EdgeStiffnessWeighted={1.f,1.f};Config->AreaStiffnessWeighted={1.f,1.f};
  Config->BendingStiffnessWeighted={.04f,.04f};Config->bUseBendingElements=true;
  Config->TetherStiffness={1.f,1.f};Config->TetherScale={1.f,1.f};Config->bUseGeodesicDistance=true;
  Config->DampingCoefficient=.3f;Config->LocalDampingCoefficient=.15f;
  Config->AnimDriveStiffness={0.f,.35f};Config->AnimDriveDamping={.6f,.6f};
  Config->CollisionThickness=bNinja?.25f:.4f;Config->FrictionCoefficient=.2f;Config->bUseCCD=false;
  Config->GravityScale=1.f;Config->LinearVelocityScale=FVector(.2f);Config->AngularVelocityScale=.15f;Config->FictitiousAngularScale=0.f;
  Config->bEnableLinearAccelerationClamping=true;Config->MaxLinearAcceleration=FVector3f(2500.f);
  Config->bEnableAngularVelocityClamping=true;Config->MaxAngularVelocity=8.f;
  Config->bUseSelfCollisions=false;
  auto* Shared=Asset->GetClothConfig<UChaosClothSharedSimConfig>();
  if(!Shared){Shared=NewObject<UChaosClothSharedSimConfig>(Asset);Asset->ClothConfigs.Add(UChaosClothSharedSimConfig::StaticClass()->GetFName(),Shared);}
  Shared->IterationCount=bNinja?12:20;Shared->MaxIterationCount=bNinja?24:40;Shared->SubdivisionCount=bNinja?4:6;
  Physical.InverseMasses.SetNumZeroed(N);Physical.CalculateNumInfluences();
  Asset->ApplyParameterMasks(true);Asset->InvalidateAllCachedData();
  if(!Bind(Mesh,Asset,Section))return false;
  const FString Dir=FPaths::ProjectSavedDir()/TEXT("CharacterSimulation/Pass2");IFileManager::Get().MakeDirectory(*Dir,true);
  FFileHelper::SaveStringToFile(CSV,*(Dir/(Mesh->GetName()+TEXT("_weights.csv"))));
  UE_LOG(LogTemp,Display,TEXT("CLOTH_SETUP_PASS2 %s section=%d sourceVertices=%d simulationVertices=%d pinned=%d moving=%d max=%.2f panels=%d collision=%s"),*Mesh->GetName(),Section,SourceCount,N,Fixed,Moving,MaxTravel,Panels,*Physics->GetPathName());
 }
 Mesh->PostEditChange();return Save(Physics)&&Save(Mesh);
}

bool CaptureCloth(UWorld* World,USkeletalMeshComponent* Component,const FString& Name,int32 Frame)
{
 auto* Camera=World->SpawnActor<AActor>();
 auto* Capture=NewObject<USceneCaptureComponent2D>(Camera);
 Camera->AddInstanceComponent(Capture);Capture->RegisterComponent();
 Capture->ProjectionType=ECameraProjectionMode::Orthographic;
 Capture->CaptureSource=ESceneCaptureSource::SCS_BaseColor;
 Capture->bCaptureEveryFrame=false;Capture->bCaptureOnMovement=false;
 Capture->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
 Capture->ShowOnlyComponent(Component);
 auto* Target=NewObject<UTextureRenderTarget2D>(Camera);
 Target->RenderTargetFormat=RTF_RGBA8;Target->InitAutoFormat(640,640);Target->UpdateResourceImmediate();Capture->TextureTarget=Target;
 FAssetCompilingManager::Get().FinishAllCompilation();
 if(GShaderCompilingManager)GShaderCompilingManager->FinishAllCompilation();
 Component->SetTextureForceResidentFlag(true);IStreamingManager::Get().StreamAllResources(30.f);
 FBox ReviewBox(ForceInit);
 for(const FTransform& Bone:Component->GetComponentSpaceTransforms())ReviewBox+=Component->GetComponentTransform().TransformPosition(Bone.GetTranslation());
 for(const auto& Pair:Component->GetCurrentClothingData_GameThread())for(const FVector3f& P:Pair.Value.Positions)ReviewBox+=Pair.Value.Transform.TransformPosition(FVector(P));
 const FBoxSphereBounds Bounds(ReviewBox);
 const FVector Center=Bounds.Origin;const double Width=FMath::Max3(Bounds.BoxExtent.X,Bounds.BoxExtent.Y,Bounds.BoxExtent.Z)*2.35;
 Capture->OrthoWidth=Width;
 const FString Dir=FPaths::ProjectSavedDir()/TEXT("CharacterSimulation/Pass2/Previews");
 IFileManager::Get().MakeDirectory(*Dir,true);
 bool OK=true;const float OriginalBlend=Component->ClothBlendWeight;
 for(int32 View=0;View<3;++View)
 {
  // Mesh-space views keep framing consistent during rapid actor turns.
  const FVector Direction=Component->GetComponentTransform().TransformVectorNoScale(View==0?FVector(0,-1,0):View==1?FVector(0,1,0):FVector(1,0,0));
  Capture->SetWorldLocation(Center+Direction*Width*2.5);
  Capture->SetWorldRotation((-Direction).Rotation());
  for(int32 Sim=0;Sim<2;++Sim)
  {
   Component->ClothBlendWeight=Sim?OriginalBlend:0.f;
   Component->MarkRenderDynamicDataDirty();Component->UpdateBounds();
   World->SendAllEndOfFrameUpdates();FlushRenderingCommands();
   ++GFrameCounter;Capture->CaptureScene();FlushRenderingCommands();
   TArray<FColor> Pixels;
   OK&=Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
   int32 Visible=0;for(auto& Pixel:Pixels){Visible+=(Pixel.R>3||Pixel.G>3||Pixel.B>3)?1:0;Pixel.A=255;}
   OK&=Visible>1000;
   TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(640,640,Pixels,PNG);
   OK&=FFileHelper::SaveArrayToFile(PNG,*(Dir/FString::Printf(TEXT("%s_%03d_%d_%s.png"),*Name,Frame,View,Sim?TEXT("cloth"):TEXT("skin"))));
  }
 }
 Component->ClothBlendWeight=OriginalBlend;Component->MarkRenderDynamicDataDirty();Camera->Destroy();
 return OK;
}

bool Verify(const FString& OnlyCharacter=FString(),bool bCapture=false)
{
 auto* World=UWorld::CreateWorld(EWorldType::Game,false);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
 World->InitializeActorsForPlay(FURL());World->bShouldSimulatePhysics=true;
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
  auto* Component=Character->GetMesh();
  auto* Mesh=Component->GetSkeletalMeshAsset();
  if(!Mesh||Mesh->GetMeshClothingAssets().Num()!=1){Passed=false;Character->Destroy();continue;}
  if(!Inspect(Mesh)){Passed=false;Character->Destroy();continue;}
  auto* Asset=Cast<UClothingAssetCommon>(Mesh->GetMeshClothingAssets()[0]);
  if(!Asset||Asset->LodData.Num()!=1||!Asset->PhysicsAsset){Passed=false;Character->Destroy();continue;}
  const auto& Physical=Asset->LodData[0].PhysicalMeshData;
  const auto& Limits=Physical.GetWeightMap(EWeightMapTargetCommon::MaxDistance);
  bool Bound=false;for(int32 S=0;S<Mesh->GetImportedModel()->LODModels[0].Sections.Num();++S)Bound|=Mesh->GetSectionClothingAsset(0,S)==Asset;
  Character->SetCharacterMode(ECharacterMode::Active);
  Component->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
  Component->bEnableUpdateRateOptimizations=false;Component->SetForcedLOD(1);Component->InitAnim(true);
  // This isolated world does not dispatch BeginPlay; register ticks explicitly
  // so mode-transition checks exercise the normal cloth registration lifecycle.
  Component->RegisterAllComponentTickFunctions(true);
  bool Valid=Bound&&Component->GetAnimInstance()&&Component->ClothBlendWeight>.99f;
  float MaxHairRatio=0;int32 ClothSamples=0,Frames=0,Attacks=0;
  double MaxDrift=0,MaxPinError=0,MaxLimitExcess=0,MaxEdgeRatio=0,MaxExtraPenetration=0;
  TArray<double> ClothTimes;
  FString CSV=TEXT("frame,phase,particles,max_drift_cm,pin_error_cm,limit_excess_cm,edge_stretch_ratio,extra_capsule_penetration_cm,hair_length_ratio\n");
  FVector Location=FVector::ZeroVector;float Facing=0;
  auto* AttacksComponent=Character->FindComponentByClass<UAutoAttackComponent>();
  auto ReadMontage=[&](const TCHAR* Property)->UAnimMontage*
  {
   if(!AttacksComponent)return nullptr;
   auto* P=FindFProperty<FObjectPropertyBase>(AttacksComponent->GetClass(),Property);
   return P?Cast<UAnimMontage>(P->GetObjectPropertyValue_InContainer(AttacksComponent)):nullptr;
  };
  UE_LOG(LogTemp,Display,TEXT("SIMULATION_START_PASS2 %s particles=%d physics=%s bound=%d"),Name,Physical.Vertices.Num(),*Asset->PhysicsAsset->GetName(),Bound);
  for(int32 Frame=0;Valid&&Frame<420;++Frame)
  {
   ++GFrameCounter;
   const float Dt=(Frame>=210&&Frame<240)?1.f/15.f:1.f/60.f;
   const TCHAR* Phase=Frame<60?TEXT("idle"):Frame<120?TEXT("run"):Frame<180?TEXT("turn"):Frame<210?TEXT("swap"):Frame<240?TEXT("15fps"):Frame<300?TEXT("attack"):Frame<360?TEXT("alternate"):TEXT("special");
   const bool Moving=Frame>=60&&Frame<240;
   if(Frame>=120&&Frame<180)Facing+=17.f;
   if(Frame==180){Character->SetCharacterMode(ECharacterMode::Inactive);Location+=FVector(10000,10000,0);}
   const FVector Velocity=Moving?FRotator(0,Facing,0).Vector()*600.f:FVector::ZeroVector;
   Location+=Velocity*Dt;
   Character->SetActorLocation(Location,false,nullptr,Frame==180?ETeleportType::TeleportPhysics:ETeleportType::None);
   Character->SetVisualFacingRotation(FRotator(0,Facing,0));
   if(Frame==180)Character->SetCharacterMode(ECharacterMode::Active);
   Character->GetCharacterMovement()->Velocity=Velocity;
   if(Frame==240||Frame==300||Frame==360)
   {
    UAnimMontage* Montage=ReadMontage(Frame==240?TEXT("AttackMontage"):Frame==300?(FCString::Strcmp(Name,TEXT("Ninja"))==0?TEXT("AlternateAttackMontage"):TEXT("DoubleCutMontage")):(FCString::Strcmp(Name,TEXT("Ninja"))==0?TEXT("FangMontage"):TEXT("CrescentMontage")));
    if(!Montage||Component->GetAnimInstance()->Montage_Play(Montage,Frame==300?2.f:1.f)<=0){Valid=false;break;}
    ++Attacks;UE_LOG(LogTemp,Display,TEXT("CLOTH_TEST_ANIMATION %s %s"),Name,*Montage->GetName());
   }
   Component->TickAnimation(Dt,false);Component->RefreshBoneTransforms();
   const double ClothStart=FPlatformTime::Seconds();
   Component->bWaitForParallelClothTask=false;Component->TickClothing(Dt,Component->ClothTickFunction);
   Component->WaitForExistingParallelClothSimulation_GameThread();Component->bWaitForParallelClothTask=true;
   if(Frame>=10&&!(Frame>=210&&Frame<240))ClothTimes.Add((FPlatformTime::Seconds()-ClothStart)*1000.);
   const auto& Data=Component->GetCurrentClothingData_GameThread();
   const FClothSimulData* Sim=Data.Find(0);
   if(!Sim||Sim->Positions.Num()!=Physical.Vertices.Num()){Valid=false;break;}
   TArray<FMatrix44f> Matrices;Component->GetCurrentRefToLocalMatrices(Matrices,0);
   TArray<FVector3f> Skin,Normals;
   ClothingMeshUtils::SkinPhysicsMesh(Asset->UsedBoneIndices,Physical,FTransform::Identity,Matrices.GetData(),Matrices.Num(),Skin,Normals);
   TArray<FVector> Positions;Positions.Reserve(Sim->Positions.Num());
   double Drift=0,PinError=0,LimitExcess=0,EdgeRatio=0,ExtraPenetration=0;
   for(int32 I=0;I<Sim->Positions.Num();++I)
   {
    const FVector P=Sim->ComponentRelativeTransform.TransformPosition(FVector(Sim->Positions[I]));Positions.Add(P);
    if(P.ContainsNaN()||P.Size()>500){Valid=false;break;}
    const double D=FVector::Distance(P,FVector(Skin[I]));Drift=FMath::Max(Drift,D);
    if(Limits[I]<=UE_SMALL_NUMBER)PinError=FMath::Max(PinError,D);
    LimitExcess=FMath::Max(LimitExcess,D-Limits[I]);
   }
   if(!Valid)break;
   for(int32 T=0;T<Physical.Indices.Num();T+=3)for(int32 E=0;E<3;++E)
   {
    const int32 A=Physical.Indices[T+E],B=Physical.Indices[T+(E+1)%3];
    const double Rest=FVector3f::Distance(Physical.Vertices[A],Physical.Vertices[B]);
    const double Animated=FVector3f::Distance(Skin[A],Skin[B]);
    if(Rest>1.f)EdgeRatio=FMath::Max(EdgeRatio,FVector::Distance(Positions[A],Positions[B])/FMath::Max(Rest,Animated));
   }
   for(const auto& Body:Asset->PhysicsAsset->SkeletalBodySetups)
   {
    const int32 Bone=Mesh->GetRefSkeleton().FindBoneIndex(Body->BoneName);if(Bone<0){Valid=false;break;}
    for(const auto& Shape:Body->AggGeom.SphylElems)
    {
     const FTransform T=Shape.GetTransform()*Component->GetComponentSpaceTransforms()[Bone];
     const FVector A=T.TransformPosition(FVector(0,0,-Shape.Length*.5f)),B=T.TransformPosition(FVector(0,0,Shape.Length*.5f));
     for(int32 I=0;I<Positions.Num();++I)if(Limits[I]>0)
     {
      const double Penetration=FMath::Max(0.,Shape.Radius-FMath::PointDistToSegment(Positions[I],A,B));
      const double Baseline=FMath::Max(0.,Shape.Radius-FMath::PointDistToSegment(FVector(Skin[I]),A,B));
      ExtraPenetration=FMath::Max(ExtraPenetration,Penetration-Baseline);
     }
    }
   }
   if(FCString::Strcmp(Name,TEXT("Ninja"))==0)for(int32 I=2;I<=5;++I)
   {
    const FName Bone(*FString::Printf(TEXT("Ponytail%d"),I)),Parent(*FString::Printf(TEXT("Ponytail%d"),I-1));
    const int32 Index=Mesh->GetRefSkeleton().FindBoneIndex(Bone);if(Index<0){Valid=false;break;}
    const double Rest=Mesh->GetRefSkeleton().GetRefBonePose()[Index].GetTranslation().Size();
    const double Current=FVector::Distance(Component->GetSocketTransform(Bone,RTS_Component).GetTranslation(),Component->GetSocketTransform(Parent,RTS_Component).GetTranslation());
    const float Ratio=Rest>UE_SMALL_NUMBER?Current/Rest:0;MaxHairRatio=FMath::Max(MaxHairRatio,Ratio);
    if(!FMath::IsFinite(Ratio)||Ratio>1.5f)Valid=false;
   }
   ++Frames;ClothSamples+=Positions.Num();
   MaxDrift=FMath::Max(MaxDrift,Drift);MaxPinError=FMath::Max(MaxPinError,PinError);
   MaxLimitExcess=FMath::Max(MaxLimitExcess,LimitExcess);MaxEdgeRatio=FMath::Max(MaxEdgeRatio,EdgeRatio);MaxExtraPenetration=FMath::Max(MaxExtraPenetration,ExtraPenetration);
   CSV+=FString::Printf(TEXT("%d,%s,%d,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f\n"),Frame,Phase,Positions.Num(),Drift,PinError,LimitExcess,EdgeRatio,ExtraPenetration,MaxHairRatio);
   if(bCapture&&(Frame==59||Frame==119||Frame==179||Frame==239||Frame==257||Frame==317||Frame==377))Valid&=CaptureCloth(World,Component,Name,Frame);
  }
  Valid&=Frames==420&&Attacks==3&&MaxDrift>.1&&MaxPinError<.1&&MaxLimitExcess<.35&&MaxEdgeRatio<1.35&&MaxExtraPenetration<1.0;
  const FString Dir=FPaths::ProjectSavedDir()/TEXT("CharacterSimulation/Pass2");IFileManager::Get().MakeDirectory(*Dir,true);
  FFileHelper::SaveStringToFile(CSV,*(Dir/FString::Printf(TEXT("%s_stability.csv"),Name)));
  UE_LOG(LogTemp,Display,TEXT("CHARACTER_CLOTH_VERIFY_PASS2 %s valid=%d frames=%d attacks=%d samples=%d drift=%.4f pins=%.4f limitExcess=%.4f edgeRatio=%.4f extraPenetration=%.4f hair=%.4f"),Name,Valid,Frames,Attacks,ClothSamples,MaxDrift,MaxPinError,MaxLimitExcess,MaxEdgeRatio,MaxExtraPenetration,MaxHairRatio);
  // Inactive characters must not retain the expensive independent cloth tick.
  Character->SetCharacterMode(ECharacterMode::Inactive);
  Component->TickComponent(1.f/60.f,LEVELTICK_All,nullptr);
  const bool HiddenStopped=Component->IsClothingSimulationSuspended()&&!Component->ClothTickFunction.IsTickFunctionRegistered();
  Character->SetCharacterMode(ECharacterMode::Assisting);
  Component->TickComponent(1.f/60.f,LEVELTICK_All,nullptr);
  const bool AssistingResumed=!Component->IsClothingSimulationSuspended()&&Component->ClothTickFunction.IsTickFunctionRegistered();
  Character->SetCharacterMode(ECharacterMode::Inactive);
  Character->SetCharacterMode(ECharacterMode::Active);
  Component->TickComponent(1.f/60.f,LEVELTICK_All,nullptr);
  const bool ActiveResumed=!Component->IsClothingSimulationSuspended()&&Component->ClothTickFunction.IsTickFunctionRegistered();
  Valid&=HiddenStopped&&AssistingResumed&&ActiveResumed;
  UE_LOG(LogTemp,Display,TEXT("CHARACTER_CLOTH_MODE_CHECK %s hidden_stopped=%d assisting_resumed=%d active_resumed=%d pass=%d"),Name,HiddenStopped,AssistingResumed,ActiveResumed,Valid);
  ClothTimes.Sort();double Sum=0;for(double T:ClothTimes)Sum+=T;
  const double ClothMeanMs=ClothTimes.IsEmpty()?DBL_MAX:Sum/ClothTimes.Num();
  const auto* Quality=Asset->GetClothConfig<UChaosClothSharedSimConfig>();
  const FString Perf=FString::Printf(TEXT("%s,%d,%d,%d,%d,%.4f,%.4f,%d\n"),*Mesh->GetName(),Physical.Vertices.Num(),Quality->IterationCount,Quality->MaxIterationCount,Quality->SubdivisionCount,ClothMeanMs,ClothTimes.IsEmpty()?0:ClothTimes[FMath::Min(ClothTimes.Num()-1,FMath::FloorToInt(ClothTimes.Num()*.95))],Valid);
  const FString PerfDir=FPaths::ProjectSavedDir()/TEXT("CharacterSimulation/Performance");IFileManager::Get().MakeDirectory(*PerfDir,true);
  FFileHelper::SaveStringToFile(Perf,*(PerfDir/TEXT("ClothTiming.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,&IFileManager::Get(),FILEWRITE_Append);
  UE_LOG(LogTemp,Display,TEXT("CHARACTER_CLOTH_TIMING %s"),*Perf);
  Passed&=Valid;Character->Destroy();
 }
 World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return Passed;
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
 if(FParse::Param(*Params,TEXT("NinjaV6Apply")))
  return Cloth(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Assets/PlayerCharacters/Ninja/V6/SK_NinjaV6.SK_NinjaV6")),45.f)?0:7;
 if(FParse::Param(*Params,TEXT("NinjaV6Review")))
  return Verify(TEXT("Ninja"),true)?0:8;
 if(FParse::Param(*Params,TEXT("TuneSamurai")))
 {
  auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Assets/PlayerCharacters/Samurai/fdsafdsa.fdsafdsa"));
  if(!Mesh||Mesh->GetMeshClothingAssets().Num()!=1)return 6;
  auto* Asset=Cast<UClothingAssetCommon>(Mesh->GetMeshClothingAssets()[0]);
  auto* Shared=Asset?Asset->GetClothConfig<UChaosClothSharedSimConfig>():nullptr;
  if(!Shared||!Asset->GetName().Contains(TEXT("Pass2")))return 6;
  for(const FIntPoint Quality:{FIntPoint(20,6),FIntPoint(24,8),FIntPoint(32,8)})
  {
   Shared->IterationCount=Quality.X;Shared->MaxIterationCount=Quality.X*2;Shared->SubdivisionCount=Quality.Y;
   const bool Passed=Verify(TEXT("Samurai"));
   UE_LOG(LogTemp,Display,TEXT("CLOTH_SOLVER_TUNE iterations=%d substeps=%d pass=%d"),Quality.X,Quality.Y,Passed);
   if(Passed)return Save(Mesh)?0:6;
  }
  return 6;
 }
 if(FParse::Param(*Params,TEXT("Inspect")))return Inspect(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Assets/PlayerCharacters/Samurai/fdsafdsa.fdsafdsa")))&&Inspect(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Assets/PlayerCharacters/Ninja/NinjaCharacterV3.NinjaCharacterV3")))?0:4;
 if(FParse::Param(*Params,TEXT("Verify"))||FParse::Param(*Params,TEXT("Review")))return Verify(FString(),FParse::Param(*Params,TEXT("Review")))?0:3;
 if(!FParse::Param(*Params,TEXT("Apply"))){UE_LOG(LogTemp,Error,TEXT("Specify -Inspect, -Apply, -Verify, or -Review."));return 1;}
 if(!Cloth(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Assets/PlayerCharacters/Samurai/fdsafdsa.fdsafdsa")),120.f))return 1;
 if(!Cloth(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Assets/PlayerCharacters/Ninja/NinjaCharacterV3.NinjaCharacterV3")),60.f))return 2;
 UE_LOG(LogTemp,Display,TEXT("CHARACTER_CLOTH_SETUP_PASS"));return 0;
#else
 return 1;
#endif
}
