#pragma once
#include "CoreMinimal.h"

class AActor;
class UWorld;

namespace CombatGround
{
HEAVENSDIVIDE_API bool IsInsideArena(UWorld* World, const FVector& Location, float Radius = 0.f);
HEAVENSDIVIDE_API FVector ClampInsideArena(UWorld* World, const FVector& Location, float Radius = 0.f);
// Maximum distance from a ground point to any authored arena corner.
HEAVENSDIVIDE_API bool ArenaCoveringRadius(UWorld* World, const FVector& Center, float& OutRadius);
// Spawn-time projection onto the authored EnemyGround surface. Characters and
// volumes must never hide the floor or become an attack/loot landing surface.
HEAVENSDIVIDE_API bool FindSurface(UWorld* World, const FVector& Desired, const AActor* Ignore,
    FVector& OutSurface, float TraceUp = 600.f, float TraceDown = 2000.f);
}
