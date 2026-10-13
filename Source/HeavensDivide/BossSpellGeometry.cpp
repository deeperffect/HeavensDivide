#include "BossSpellGeometry.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"

namespace
{
bool GroundIntersection(FVector Origin, FVector Direction, float GroundZ, FVector& Out)
{
    if (FMath::Abs(Direction.Z) < .001) return false;
    const double Distance = (GroundZ - Origin.Z) / Direction.Z;
    if (Distance <= 0 || Distance > 100000 || !FMath::IsFinite(Distance)) return false;
    Out = Origin + Direction * Distance;
    return !Out.ContainsNaN();
}
}

bool BossSpellGeometry::GroundViewCorners(const FMinimalViewInfo& View, float AspectRatio, float GroundZ, TArray<FVector>& Out)
{
    Out.Reset();
    const double Aspect = FMath::Max(.1f, AspectRatio);
    const double HalfWidth = View.ProjectionMode == ECameraProjectionMode::Orthographic
        ? View.OrthoWidth * .5 : FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(View.FOV, 5.f, 170.f) * .5f));
    for (const FVector2D UV : {FVector2D(-1, 1), FVector2D(1, 1), FVector2D(1, -1), FVector2D(-1, -1)})
    {
        FVector Origin = View.Location;
        FVector Direction;
        if (View.ProjectionMode == ECameraProjectionMode::Orthographic)
        {
            Origin += View.Rotation.RotateVector(FVector(0, UV.X * HalfWidth, UV.Y * HalfWidth / Aspect));
            Direction = View.Rotation.Vector();
        }
        else Direction = View.Rotation.RotateVector(FVector(1, UV.X * HalfWidth, UV.Y * HalfWidth / Aspect)).GetSafeNormal();
        FVector Point;
        if (!GroundIntersection(Origin, Direction, GroundZ, Point)) { Out.Reset(); return false; }
        Out.Add(Point);
    }
    return true;
}

bool BossSpellGeometry::GroundViewCorners(APlayerController* Controller, float GroundZ, TArray<FVector>& Out)
{
    Out.Reset();
    if (!Controller) return false;
    int32 Width = 0, Height = 0;
    Controller->GetViewportSize(Width, Height);
    if (Width > 1 && Height > 1)
    {
        for (const FVector2D UV : {FVector2D(0, 0), FVector2D(1, 0), FVector2D(1, 1), FVector2D(0, 1)})
        {
            FVector Origin, Direction, Point;
            if (!Controller->DeprojectScreenPositionToWorld(UV.X * (Width - 1), UV.Y * (Height - 1), Origin, Direction)
                || !GroundIntersection(Origin, Direction, GroundZ, Point)) { Out.Reset(); break; }
            Out.Add(Point);
        }
        if (Out.Num() == 4) return true;
    }
    FMinimalViewInfo View;
    if (AActor* Target = Controller->GetViewTarget()) Target->CalcCamera(0, View);
    else if (Controller->PlayerCameraManager) View = Controller->PlayerCameraManager->GetCameraCacheView();
    return GroundViewCorners(View, Width > 1 && Height > 1 ? float(Width) / Height : View.AspectRatio, GroundZ, Out);
}

void BossSpellGeometry::ScreenCircleCenters(const TArray<FVector>& Corners, float Spacing, int32 Maximum, TArray<FVector>& Out)
{
    Out.Reset();
    if (Corners.Num() != 4) return;
    const double Horizontal = FMath::Max(FVector::Dist2D(Corners[0], Corners[1]), FVector::Dist2D(Corners[3], Corners[2]));
    const double Vertical = FMath::Max(FVector::Dist2D(Corners[0], Corners[3]), FVector::Dist2D(Corners[1], Corners[2]));
    double Step = FMath::Max(50.f, Spacing);
    const int32 Limit = FMath::Clamp(Maximum, 4, 128);
    int32 Columns, Rows;
    do
    {
        Columns = FMath::Max(2, FMath::CeilToInt(Horizontal / Step) + 1);
        Rows = FMath::Max(2, FMath::CeilToInt(Vertical / Step) + 1);
        if (int64(Columns) * Rows <= Limit) break;
        Step *= FMath::Max(1.1, FMath::Sqrt(double(Columns) * Rows / Limit));
    } while (true);
    Out.Reserve(Columns * Rows);
    for (int32 Y = 0; Y < Rows; ++Y)
    {
        const float V = float(Y) / (Rows - 1);
        const FVector Left = FMath::Lerp(Corners[0], Corners[3], V);
        const FVector Right = FMath::Lerp(Corners[1], Corners[2], V);
        for (int32 X = 0; X < Columns; ++X) Out.Add(FMath::Lerp(Left, Right, float(X) / (Columns - 1)));
    }
}
