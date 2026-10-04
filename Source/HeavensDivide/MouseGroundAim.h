#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

namespace MouseGroundAim
{
// Cursor aim uses terrain, never pawn capsules or animated body/weapon meshes.
// A multi-object trace lets us reject a wall or a pawn mesh accidentally authored
// as WorldStatic without losing the floor farther along the same cursor ray.
inline bool Resolve(UWorld* World, const FVector& RayOrigin, const FVector& RayDirection,
    float FallbackGroundZ, const AActor* IgnoredActor, FVector& OutPosition)
{
    const FVector Direction = RayDirection.GetSafeNormal();
    if (!World || Direction.IsNearlyZero() || Direction.ContainsNaN() || RayOrigin.ContainsNaN()) return false;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(MouseGroundAim), false, IgnoredActor);
    TArray<FHitResult> Hits;
    World->LineTraceMultiByObjectType(Hits, RayOrigin, RayOrigin + Direction * 100000.f,
        FCollisionObjectQueryParams(ECC_WorldStatic), Query);
    for (const FHitResult& Hit : Hits)
    {
        const auto* Actor = Hit.GetActor();
        if (Actor && (Actor->IsA<APawn>() || (Actor->GetOwner() && Actor->GetOwner()->IsA<APawn>()))) continue;
        if (!Hit.GetComponent() || Hit.ImpactNormal.Z < .7f) continue;
        OutPosition = Hit.ImpactPoint;
        return true;
    }

    // Empty ground still aims consistently at foot height, not capsule-center
    // height. Never fall back to Visibility, which would reintroduce mob snapping.
    if (FMath::IsNearlyZero(Direction.Z)) return false;
    const float Distance = (FallbackGroundZ - RayOrigin.Z) / Direction.Z;
    if (Distance <= 0.f) return false;
    OutPosition = RayOrigin + Direction * Distance;
    return true;
}
}
