#include "ExperienceDropSubsystem.h"
#include "ExperiencePickup.h"
#include "ExperiencePickupDropSettings.h"
#include "ExperienceComponent.h"
#include "Engine/World.h"

FIntPoint UExperienceDropSubsystem::Cell(FVector Position)
{
    return FIntPoint(FMath::FloorToInt(Position.X/500.),FMath::FloorToInt(Position.Y/500.));
}

void UExperienceDropSubsystem::DropReward(AActor* Source,FVector Position,TSubclassOf<AExperiencePickup> Class,
    int32 XP,UExperienceComponent* Experience,UCharacterManagerComponent* Manager)
{
    if (XP<=0 || !Class || !Experience || !GetWorld()) return;
    const auto* Settings=GetDefault<UExperiencePickupDropSettings>();
    const int32 Limit=FMath::Clamp(Settings->RewardsPerPickup,1,16);
    const float Radius=FMath::Clamp(Settings->MergeRadius,0.f,2000.f);
    const FIntPoint Origin=Cell(Position);
    const int32 Reach=FMath::CeilToInt(Radius/500.f);
    auto Eligible=[&](const TWeakObjectPtr<AExperiencePickup>& Orb)
    {
        return Orb.IsValid() && !Orb->IsActorBeingDestroyed() && !Orb->bAttracting && !Orb->bCollected
            && !Orb->bRewardFlight && Orb->EnemyRewardContributions>0 && Orb->EnemyRewardContributions<Limit;
    };
    // Search only nearby cells; pickup actors never scan the map or merge each tick.
    AExperiencePickup* Nearest=nullptr;
    double BestDistance=FMath::Square(double(Radius));
    if (Limit>1 && Radius>0)
        for(int32 X=-Reach;X<=Reach;++X) for(int32 Y=-Reach;Y<=Reach;++Y)
        {
            const FIntPoint Key=Origin+FIntPoint(X,Y);
            auto* Bucket=RestingPickups.Find(Key);
            if (!Bucket) continue;
            Bucket->RemoveAll([&](const auto& Orb){return !Eligible(Orb);});
            for(auto Orb:*Bucket)
            {
                if (Orb->GetClass()!=Class.Get() || Orb->ExperienceComponent!=Experience || Orb->CharacterManager!=Manager
                    || int64(Orb->XPValue)+XP>MAX_int32 || FMath::Abs(Orb->GetActorLocation().Z-Position.Z)>100) continue;
                const double Distance=FVector::DistSquared2D(Position,Orb->GetActorLocation());
                if (Distance<=BestDistance) { Nearest=Orb.Get(); BestDistance=Distance; }
            }
            if (Bucket->IsEmpty()) RestingPickups.Remove(Key);
        }
    if (Nearest)
    {
        // Exact integer addition preserves every kill's authored reward.
        Nearest->XPValue+=XP;
        ++Nearest->EnemyRewardContributions;
        return;
    }
    FActorSpawnParameters Params; Params.Owner=Source;
    Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    if (auto* Orb=GetWorld()->SpawnActor<AExperiencePickup>(Class,Position,FRotator::ZeroRotator,Params))
    {
        Orb->EnemyRewardContributions=1;
        Orb->InitializePickup(XP,Experience,Manager);
        if (Eligible(Orb)) RestingPickups.FindOrAdd(Origin).Add(Orb);
    }
    else
    {
        // As with elite chests, a failed spawn cannot destroy earned XP.
        Experience->AddXP(XP);
    }
}
