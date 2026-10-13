#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ExperienceDropSubsystem.generated.h"
class AExperiencePickup;
class UExperienceComponent;
class UCharacterManagerComponent;

/** Index resting enemy rewards by position; no ticking or deferred XP balance. */
UCLASS()
class HEAVENSDIVIDE_API UExperienceDropSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    void DropReward(AActor* Source,FVector Position,TSubclassOf<AExperiencePickup> Class,int32 XP,
        UExperienceComponent* Experience,UCharacterManagerComponent* Manager);
private:
    static FIntPoint Cell(FVector Position);
    TMap<FIntPoint,TArray<TWeakObjectPtr<AExperiencePickup>>> RestingPickups;
};
