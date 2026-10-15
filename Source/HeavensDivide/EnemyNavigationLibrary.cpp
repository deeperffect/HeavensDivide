#include "EnemyNavigationLibrary.h"
#if WITH_EDITOR
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Builders/CubeBuilder.h"
#include "ActorFactories/ActorFactory.h"
#include "NavigationSystem.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavMesh/RecastNavMesh.h"
#include "Components/BrushComponent.h"
#include "Engine/StaticMeshActor.h"
#include "AssetCompilingManager.h"
#include "UObject/UnrealType.h"
#endif

bool UEnemyNavigationLibrary::BakeOldVillageNavigation(UObject* WorldContext)
{
#if WITH_EDITOR
    UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
    if (!World || World->IsGameWorld()) return false;
    FAssetCompilingManager::Get().FinishAllCompilation();
    ANavMeshBoundsVolume* Bounds = nullptr;
    for (TActorIterator<ANavMeshBoundsVolume> It(World); It; ++It) { Bounds = *It; break; }
    if (!Bounds)
    {
        Bounds = World->SpawnActor<ANavMeshBoundsVolume>();
        Bounds->SetActorLabel(TEXT("Old Village - Baked Navigation Bounds"));
        Bounds->SetFolderPath(TEXT("80_Gameplay"));
    }
    FBox GroundBounds(ForceInit);
    for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
        if (It->ActorHasTag(TEXT("SurvivalMapGround")) || It->GetActorLabel().StartsWith(TEXT("Village ground"))) GroundBounds += It->GetComponentsBoundingBox();
    if (!GroundBounds.IsValid) return false;
    {
        UCubeBuilder* Builder = NewObject<UCubeBuilder>(Bounds);
        Builder->X = GroundBounds.GetSize().X; Builder->Y = GroundBounds.GetSize().Y; Builder->Z = 600;
        UActorFactory::CreateBrushForVolumeActor(Bounds, Builder);
        Bounds->GetBrushComponent()->BuildSimpleBrushCollision();
        Bounds->SetActorLocation(FVector(GroundBounds.GetCenter().X, GroundBounds.GetCenter().Y, 100));
    }
    UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    if (!Nav) return false;
    Nav->OnNavigationBoundsUpdated(Bounds);
    ARecastNavMesh* Mesh = Cast<ARecastNavMesh>(Nav->GetDefaultNavDataInstance(FNavigationSystem::Create));
    if (!Mesh) return false;
    Mesh->Modify();
    // A conservative shared clearance keeps ordinary and larger enemies off
    // fence/house corners. The map's main roads are 700-900 cm wide.
    Mesh->AgentRadius = 90.0f;
    Mesh->AgentHeight = 200.0f;
    Mesh->SetCellSize(ENavigationDataResolution::Default, 20.0f);
    Mesh->SetAgentMaxStepHeight(ENavigationDataResolution::Default, 10.0f);
    if (FEnumProperty* Property = FindFProperty<FEnumProperty>(Mesh->GetClass(), TEXT("RuntimeGeneration")))
        Property->GetUnderlyingProperty()->SetIntPropertyValue(Property->ContainerPtrToValuePtr<void>(Mesh), static_cast<uint64>(ERuntimeGenerationType::Static));
    else return false;
    Mesh->SetActorLabel(TEXT("Old Village - Static Navigation"));
    Mesh->SetFolderPath(TEXT("80_Gameplay"));
    // Commandlets do not pump the editor ticker that releases this lock.
    // All mesh compilation has completed, so the explicit bake can proceed.
    FAssetCompilingManager::Get().FinishAllCompilation();
    Nav->RemoveNavigationBuildLock(ENavigationBuildLock::AsyncLoadLock,
        UNavigationSystemV1::ELockRemovalRebuildAction::NoRebuild);
    Nav->Build();
    Mesh->EnsureBuildCompletion();
    World->MarkPackageDirty();
    return Mesh->GetRuntimeGenerationMode() == ERuntimeGenerationType::Static && Mesh->GetNumActiveTiles() > 0;
#else
    return false;
#endif
}
