#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "RunPreparationReviewCommandlet.generated.h"

// Read-only collision/navigation probe of the shipped survival map.
UCLASS()
class URunPreparationReviewCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    URunPreparationReviewCommandlet();
    virtual int32 Main(const FString& Params) override;
};
