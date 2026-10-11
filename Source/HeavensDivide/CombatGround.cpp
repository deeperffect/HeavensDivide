#include "CombatGround.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Volume.h"
#include "EnemySpawnArea.h"
#include "MinimapBounds.h"
#include "Components/BoxComponent.h"
#include "EngineUtils.h"

namespace
{
UBoxComponent* FindArenaBox(UWorld* World)
{
    if (!World) return nullptr;
    for (TActorIterator<AEnemySpawnArea> It(World);It;++It) return It->FindComponentByClass<UBoxComponent>();
    for (TActorIterator<AMinimapBounds> It(World);It;++It) return It->PlayableArea;
    return nullptr; // Standalone test/lab worlds may intentionally have no authored arena.
}
FVector PaddedExtent(UBoxComponent* Box,float Radius)
{
    const FVector Scale=Box->GetComponentScale().GetAbs();
    const FVector Extent=Box->GetUnscaledBoxExtent();
    return FVector(FMath::Max(0.,Extent.X-FMath::Max(0.f,Radius)/FMath::Max(.001,Scale.X)),
        FMath::Max(0.,Extent.Y-FMath::Max(0.f,Radius)/FMath::Max(.001,Scale.Y)),Extent.Z);
}
}
bool CombatGround::IsInsideArena(UWorld* World,const FVector& Location,float Radius)
{
    auto* Box=FindArenaBox(World);if(!Box)return true;
    const FVector WorldExtent=Box->GetScaledBoxExtent();
    if (Radius>FMath::Min(WorldExtent.X,WorldExtent.Y)) return false;
    const FVector Local=Box->GetComponentTransform().InverseTransformPosition(Location);
    const FVector Extent=PaddedExtent(Box,Radius);
    return FMath::Abs(Local.X)<=Extent.X && FMath::Abs(Local.Y)<=Extent.Y;
}
FVector CombatGround::ClampInsideArena(UWorld* World,const FVector& Location,float Radius)
{
    auto* Box=FindArenaBox(World);if(!Box)return Location;
    FVector Local=Box->GetComponentTransform().InverseTransformPosition(Location);
    const FVector Extent=PaddedExtent(Box,Radius);
    Local.X=FMath::Clamp(Local.X,-Extent.X,Extent.X);Local.Y=FMath::Clamp(Local.Y,-Extent.Y,Extent.Y);
    FVector Result=Box->GetComponentTransform().TransformPosition(Local);Result.Z=Location.Z;return Result;
}

bool CombatGround::FindSurface(UWorld* World, const FVector& Desired, const AActor* Ignore,
    FVector& OutSurface, float TraceUp, float TraceDown)
{
    if (!World) return false;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(CombatSpawnGround), false, Ignore);
    const FVector Start=Desired+FVector(0,0,TraceUp);
    const FVector End=Desired-FVector(0,0,TraceDown);
    TArray<FHitResult> Hits;
    for (int32 Attempt=0; Attempt<32; ++Attempt)
    {
        World->LineTraceMultiByChannel(Hits,Start,End,ECC_GameTraceChannel2,Query);
        AActor* Obstruction=nullptr;
        for (const FHitResult& Hit:Hits)
        {
            if (!Hit.bBlockingHit) continue;
            AActor* Actor=Hit.GetActor();
            if (Cast<ACharacter>(Actor) || Cast<AVolume>(Actor))
            {
                Obstruction=Actor;
                break;
            }
            if (Hit.GetComponent() && Hit.ImpactNormal.Z>=.7f)
            {
                OutSurface=Hit.ImpactPoint;
                return true;
            }
        }
        if (!Obstruction) return false;
        Query.AddIgnoredActor(Obstruction);
    }
    return false;
}
