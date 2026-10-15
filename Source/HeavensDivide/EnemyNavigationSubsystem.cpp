#include "EnemyNavigationSubsystem.h"
#include "Engine/World.h"

bool FEnemyPathRequestBudget::TryAcquire(uint64 Frame, double Now)
{
    if (Frame != LastFrame)
    {
        LastFrame = Frame;
        FrameRequests = 0;
        FrameMilliseconds = 0.0;
    }
    if (LastTime >= 0.0)
        Tokens = FMath::Min(2.0, Tokens + FMath::Max(0.0, Now - LastTime) * 30.0);
    LastTime = Now;
    // A synchronous query cannot be interrupted; stop admitting more work after
    // the time gate. The count/rate limits remain hard bounds even on slow frames.
    if (FrameRequests >= 2 || FrameMilliseconds >= 0.5 || Tokens < 1.0)
        return false;
    Tokens -= 1.0;
    ++FrameRequests;
    return true;
}

bool UEnemyNavigationSubsystem::TryAcquirePathRequest()
{
    if (!Budget.TryAcquire(GFrameCounter, GetWorld()->GetTimeSeconds()))
    {
        ++DeferredRequests;
        return false;
    }
    ++PathRequests;
    PeakRequestsPerFrame = FMath::Max(PeakRequestsPerFrame, Budget.FrameRequests);
    return true;
}

void UEnemyNavigationSubsystem::RecordPathRequest(double Milliseconds)
{
    Budget.RecordCost(Milliseconds);
    TotalPathMilliseconds += Milliseconds;
    PeakPathMilliseconds = FMath::Max(PeakPathMilliseconds, Milliseconds);
}
