#pragma once
#include "CoreMinimal.h"
#include "Camera/CameraTypes.h"

class APlayerController;

namespace BossSpellGeometry
{
// Ordered top-left, top-right, bottom-right, bottom-left on the ground plane.
bool GroundViewCorners(const FMinimalViewInfo& View, float AspectRatio, float GroundZ, TArray<FVector>& Out);
bool GroundViewCorners(APlayerController* Controller, float GroundZ, TArray<FVector>& Out);
void ScreenCircleCenters(const TArray<FVector>& Corners, float Spacing, int32 Maximum, TArray<FVector>& Out);
}
