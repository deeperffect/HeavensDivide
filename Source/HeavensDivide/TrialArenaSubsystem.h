#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TrialArenaSubsystem.generated.h"

class ATrialArenaAnchor;

USTRUCT()
struct FTrialArenaReservation
{
	GENERATED_BODY()
	TWeakObjectPtr<ATrialArenaAnchor> Anchor;
	TWeakObjectPtr<AActor> Owner;
};

UCLASS()
class HEAVENSDIVIDE_API UTrialArenaSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	ATrialArenaAnchor* ReserveAnchor(AActor* TrialOwner);
	void ReleaseAnchor(AActor* TrialOwner);
private:
	TArray<FTrialArenaReservation> Reservations;
};

