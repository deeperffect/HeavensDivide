#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraSystemInstanceController.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraDataSetAccessor.h"
#include "AssetCompilingManager.h"
#include "AnimNotify_SpawnSamuraiSlashNiagara.h"
#include "Animation/AnimMontage.h"
#include "SamuraiCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBloodSlashHitboxTest,"HeavensDivide.RunFixes.BloodSlashHitbox",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FBloodSlashHitboxTest::RunTest(const FString&)
{
    auto* W=UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);W->InitializeActorsForPlay(FURL());W->BeginPlay();
    auto* System=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/Assets/VFX/SlashesV1/Particles/NiagaraSystems/NS_Slash_BloodStance"));
    if(!TestNotNull(TEXT("Authored Blood slash"),System))return false;
    System->WaitForCompilationComplete(true,false);FAssetCompilingManager::Get().FinishAllCompilation();
    auto* FX=UNiagaraFunctionLibrary::SpawnSystemAtLocation(W,System,FVector::ZeroVector,FRotator::ZeroRotator,FVector::OneVector,false,false,ENCPoolMethod::None,false);
    if(!TestNotNull(TEXT("Blood slash simulation"),FX))return false;
    FX->SetForceSolo(true);FX->Activate(true);
    double Radius=0;FBox Envelope(ForceInit);
    for(int32 Frame=0;Frame<80;++Frame)
    {
        FX->AdvanceSimulation(1,.01f);
        auto Controller=FX->GetSystemInstanceController();if(!Controller.IsValid())continue;
        auto* Instance=Controller->GetSystemInstance_Unsafe();if(!Instance)continue;
        for(auto& Emitter:Instance->GetEmitters())
        {
            const auto* EmitterData=Emitter->GetEmitterHandle().GetEmitterData();
            if(!EmitterData)continue;
            const auto& Data=Emitter->GetParticleData();const auto* Buffer=Data.GetCurrentData();if(!Buffer)continue;
            auto Positions=FNiagaraDataSetAccessor<FNiagaraPosition>::CreateReader(Data,TEXT("Position"));
            auto Scales=FNiagaraDataSetAccessor<FVector3f>::CreateReader(Data,TEXT("Scale"));
            auto Rotations=FNiagaraDataSetAccessor<FQuat4f>::CreateReader(Data,TEXT("MeshOrientation"));
            for(UNiagaraRendererProperties* Renderer:EmitterData->GetRenderers())
                if(auto* MeshRenderer=Cast<UNiagaraMeshRendererProperties>(Renderer))
                    for(const auto& Entry:MeshRenderer->Meshes)
                    {
                        UStaticMesh* Mesh=Entry.Mesh;if(!Mesh||!Mesh->GetRenderData()||Mesh->GetRenderData()->LODResources.IsEmpty())continue;
                        const auto& Vertices=Mesh->GetRenderData()->LODResources[0].VertexBuffers.PositionVertexBuffer;
                        for(uint32 Particle=0;Particle<Buffer->GetNumInstances();++Particle)
                        {
                            const FVector Position=Positions.IsValid()?FVector(Positions.Get(Particle)):FVector::ZeroVector;
                            const FVector Scale=Scales.IsValid()?FVector(Scales.Get(Particle)):FVector::OneVector;
                            const FQuat Rotation=Rotations.IsValid()?FQuat(Rotations.Get(Particle)):FQuat::Identity;
                            for(uint32 Vertex=0;Vertex<Vertices.GetNumVertices();++Vertex)
                            {
                                const FVector P=Position+Rotation.RotateVector(FQuat(Entry.Rotation.Quaternion()).RotateVector(FVector(Vertices.VertexPosition(Vertex))*FVector(Entry.Scale))*Scale);
                                Radius=FMath::Max(Radius,P.Size2D());Envelope+=P;
                            }
                        }
                    }
        }
    }
    TestTrue(TEXT("Slash has a measurable horizontal footprint"),Radius>1 && Radius<5000);
    const FString Folder=FPaths::ProjectSavedDir()/TEXT("RunFixes");IFileManager::Get().MakeDirectory(*Folder,true);
    FFileHelper::SaveStringToFile(FString::Printf(TEXT("{\"radius\":%.6f,\"min_z\":%.6f,\"max_z\":%.6f}"),Radius,Envelope.Min.Z,Envelope.Max.Z),*(Folder/TEXT("BloodCalibration.json")));
    AddInfo(FString::Printf(TEXT("Blood slash unit-scale radius = %.3f cm"),Radius));
    FX->DestroyComponent();
    auto* Montage=LoadObject<UAnimMontage>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/PlayerCharacters/Montages/Samurai/AM_AutoAttackSamurai"));
    auto* Character=W->SpawnActor<ASamuraiCharacter>();
    if(Montage)for(const auto& Event:Montage->Notifies)
        if(auto* Notify=Cast<UAnimNotify_SpawnSamuraiSlashNiagara>(Event.Notify))
        {
            auto* Copy=DuplicateObject<UAnimNotify_SpawnSamuraiSlashNiagara>(Notify,GetTransientPackage());
            FindFProperty<FFloatProperty>(Copy->GetClass(),TEXT("HitboxReferenceRadius"))->SetPropertyValue_InContainer(Copy,Radius);
            for(float Area:{.5f,1.f,1.35f,2.5f})
            {
                const FVector Center(120,50,100);const float HitRadius=240.f*Area;
                // Component scale is independent of cosmetic mesh size.
                Character->GetMesh()->SetWorldScale3D(FVector(1.22));
                Copy->SpawnForHitbox(Character->GetMesh(),Center,HitRadius,FVector::ForwardVector,false);
                TArray<UObject*> Objects;GetObjectsOfClass(UNiagaraComponent::StaticClass(),Objects,true);
                bool Found=false;
                for(auto* Object:Objects)if(auto* Effect=Cast<UNiagaraComponent>(Object))
                    if(Effect->IsRegistered() && Effect->IsActive() && Effect->GetWorld()==W && Effect->GetAsset()==System && Effect->GetComponentLocation().Equals(Center))
                    {
                        Found=true;
                        TestTrue(TEXT("Particle radius matches upgraded hitbox once"),FMath::IsNearlyEqual(float(Effect->GetComponentScale().X*Radius),HitRadius,.1f));
                        TestTrue(TEXT("Effect remains centered on the committed hitbox"),Effect->GetComponentLocation().Equals(Center));
                        Effect->DeactivateImmediate();
                    }
                TestTrue(TEXT("Hitbox effect spawned"),Found);
            }
            break;
        }
    W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
}
#endif
