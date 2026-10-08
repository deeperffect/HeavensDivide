#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "SwapPresentationComponent.h"
#include "CharacterBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "ClothingAsset.h"
#include "Utils/ClothingMeshUtils.h"
#include "AssetCompilingManager.h"
#include "Animation/AnimMontage.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSwapClothEntranceTest,"HeavensDivide.Encounters.NinjaArrivalCloth",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSwapClothEntranceTest::RunTest(const FString&)
{
 auto* W=UWorld::CreateWorld(EWorldType::Game,false);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 W->InitializeActorsForPlay(FURL());W->bShouldSimulatePhysics=true;W->GetWorldSettings()->TimeDilation=.0001f;
 auto* Class=LoadClass<ACharacterBase>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Ninja.BP_Ninja_C"));
 FAssetCompilingManager::Get().FinishAllCompilation();
 auto* C=W->SpawnActor<ACharacterBase>(Class);C->SetCharacterMode(ECharacterMode::Active);
 auto* Mesh=C->GetMesh();Mesh->SetForcedLOD(1);Mesh->RegisterAllComponentTickFunctions(true);
 auto* Cloth=Cast<UClothingAssetCommon>(Mesh->GetSkeletalMeshAsset()->GetMeshClothingAssets()[0]);
 const auto& Physical=Cloth->LodData[0].PhysicalMeshData;
 const auto& Limits=Physical.GetWeightMap(EWeightMapTargetCommon::MaxDistance);
 auto* Swap=C->SwapPresentation.Get();Swap->bEnableSound=false;Swap->SwapFreezeDuration=0;
 const bool OriginalClothTick=Mesh->ClothTickFunction.IsTickFunctionEnabled();
 Swap->PlayArrival();TestTrue(TEXT("Actual ninja flip montage starts"),Swap->bEntranceOwnsPose);
 TestFalse(TEXT("Scheduled cloth tick disabled to prevent double stepping"),Mesh->ClothTickFunction.IsTickFunctionEnabled());
 int32 Samples=0;double PeakDrift=0,PeakPinError=0;bool Finite=true;
 for(int32 Frame=0;Frame<600 && Swap->bEntranceOwnsPose;++Frame)
 {
  ++GFrameCounter;Swap->UpdateEntrance(1.f/60.f);
  if(!Swap->bEntranceOwnsPose)break;
  const auto& Data=Mesh->GetCurrentClothingData_GameThread();const auto* Sim=Data.Find(0);
  if(!Sim || Sim->Positions.Num()!=Physical.Vertices.Num())continue;
  ++Samples;TArray<FMatrix44f> Matrices;Mesh->GetCurrentRefToLocalMatrices(Matrices,0);
  TArray<FVector3f> Skin,Normals;
  ClothingMeshUtils::SkinPhysicsMesh(Cloth->UsedBoneIndices,Physical,FTransform::Identity,Matrices.GetData(),Matrices.Num(),Skin,Normals);
  for(int32 I=0;I<Sim->Positions.Num();++I)
  {
   const FVector P=Sim->ComponentRelativeTransform.TransformPosition(FVector(Sim->Positions[I]));
   Finite &= !P.ContainsNaN() && P.Size()<600;
   const double D=FVector::Distance(P,FVector(Skin[I]));
   if(Limits[I]>15)PeakDrift=FMath::Max(PeakDrift,D);
   if(Limits[I]<.01)PeakPinError=FMath::Max(PeakPinError,D);
  }
 }
 TestTrue(TEXT("Cloth simulated throughout arrival"),Samples>30);
 TestTrue(TEXT("Free ribbon vertices move independently of rigid skin"),PeakDrift>3);
 TestTrue(TEXT("Pinned scarf attachment remains stable"),PeakPinError<3);
 TestTrue(TEXT("Simulation remains finite through both flips"),Finite);
 TestEqual(TEXT("Normal cloth tick restored after arrival"),Mesh->ClothTickFunction.IsTickFunctionEnabled(),OriginalClothTick);
 AddInfo(FString::Printf(TEXT("Ninja frozen arrival: %d simulated frames, %.2f cm free motion, %.3f cm pin error"),Samples,PeakDrift,PeakPinError));
 W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
}
#endif
