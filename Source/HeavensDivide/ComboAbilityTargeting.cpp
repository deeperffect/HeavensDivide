#include "ComboAbilityComponent.h"
#include "CharacterBase.h"
#include "EnemyBase.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"

bool UComboAbilityComponent::FaceEnemyPack(ACharacterBase* Character)
{
    const FVector Origin = Character->GetActorLocation();
    const float SearchRadius = FMath::Max(1.f, ActiveSettings.PackSearchRadius);
    const float PackRadiusSquared = FMath::Square(FMath::Max(1.f, ActiveSettings.PackRadius));
    TArray<FOverlapResult> Hits;
    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_Pawn);
    Objects.AddObjectTypesToQuery(ECC_GameTraceChannel1);
    GetWorld()->OverlapMultiByObjectType(Hits, Origin, FQuat::Identity, Objects,
        FCollisionShape::MakeSphere(SearchRadius), FCollisionQueryParams(SCENE_QUERY_STAT(ComboPackFacing), false, Character));
    TSet<AEnemyBase*> Seen;
    TArray<FVector> Positions;
    for (const auto& Hit : Hits)
    {
        auto* Enemy = Cast<AEnemyBase>(Hit.GetActor());
        if (!IsValid(Enemy) || Enemy->IsDead() || Seen.Contains(Enemy)) continue;
        Seen.Add(Enemy);
        if (FVector::DistSquared(Origin, Enemy->GetActorLocation()) <= FMath::Square(SearchRadius))
            Positions.Add(Enemy->GetActorLocation());
    }
    int32 BestCount = 0;
    float BestDistance = MAX_flt;
    FVector BestCenter = Origin;
    for (const FVector& Candidate : Positions)
    {
        int32 Count = 0;
        FVector Sum = FVector::ZeroVector;
        for (const FVector& Position : Positions)
            if (FVector::DistSquared2D(Candidate, Position) <= PackRadiusSquared)
            {
                ++Count;
                Sum += Position;
            }
        const FVector Center = Sum / Count;
        const float Distance = FVector::DistSquared2D(Origin, Center);
        if (Distance > KINDA_SMALL_NUMBER && (Count > BestCount || (Count == BestCount && Distance < BestDistance)))
        {
            BestCount = Count;
            BestDistance = Distance;
            BestCenter = Center;
        }
    }
    if (!BestCount) return false;
    Character->SetFacingOverrideTarget(BestCenter);
    Character->SetVisualFacingRotation((BestCenter - Origin).GetSafeNormal2D().Rotation());
    return true;
}
