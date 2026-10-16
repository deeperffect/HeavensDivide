#pragma once

#include "CoreMinimal.h"

class AActor;
struct FGeometry;
class FSlateWindowElementList;

/** Plain data, bounded per HUD: no actors, widget objects, timers, or particle systems per hit. */
struct FCombatNumber
{
    TWeakObjectPtr<AActor> Target;
    FVector WorldPosition = FVector::ZeroVector;
    double Amount = 0;
    float Age = 0;
    float Side = 0;
    bool bCritical = false;
    bool bStatus = false;
    FString Text;
    float Lifetime() const { return bCritical ? .62f : bStatus ? .42f : .46f; }
};

struct FCombatNumberAnimation
{
    FVector2D Offset;
    float Scale = 1;
    float Opacity = 1;
    float Burst = 0;
};

class FCombatNumberBuffer
{
public:
    static constexpr int32 Capacity = 96;
    void Add(AActor* Target, FVector Position, float Amount, bool bCritical, bool bStatus);
    void Advance(float Delta);
    void Reset() { Numbers.Reset(); Sequence = 0; }
    const TArray<FCombatNumber>& GetNumbers() const { return Numbers; }
private:
    TArray<FCombatNumber> Numbers;
    uint32 Sequence = 0;
};

namespace CombatNumbers
{
    FCombatNumberAnimation Animate(const FCombatNumber& Number);
    void Draw(const FCombatNumber& Number, FVector2D Anchor, const FGeometry& Geometry,
        FSlateWindowElementList& Out, int32 Layer, float Opacity = 1.f);
}
