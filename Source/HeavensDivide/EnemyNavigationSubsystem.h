#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "EnemyNavigationSubsystem.generated.h"

/** Shared admission budget; idle enemies do not tick this subsystem. */
struct FEnemyPathRequestBudget
{
    bool TryAcquire(uint64 Frame, double Now);
    void RecordCost(double Milliseconds) { FrameMilliseconds += Milliseconds; }
    uint64 LastFrame = MAX_uint64;
    double LastTime = -1.0;
    double Tokens = 2.0;
    double FrameMilliseconds = 0.0;
    int32 FrameRequests = 0;
};

UCLASS()
class HEAVENSDIVIDE_API UEnemyNavigationSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    bool TryAcquirePathRequest();
    void RecordPathRequest(double Milliseconds);

    UPROPERTY(BlueprintReadOnly, Category="Enemy Navigation") int32 PathRequests = 0;
    UPROPERTY(BlueprintReadOnly, Category="Enemy Navigation") int32 DeferredRequests = 0;
    UPROPERTY(BlueprintReadOnly, Category="Enemy Navigation") int32 PeakRequestsPerFrame = 0;
    UPROPERTY(BlueprintReadOnly, Category="Enemy Navigation") double TotalPathMilliseconds = 0.0;
    UPROPERTY(BlueprintReadOnly, Category="Enemy Navigation") double PeakPathMilliseconds = 0.0;
private:
    FEnemyPathRequestBudget Budget;
};
