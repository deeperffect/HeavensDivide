#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "BarrageVFXReviewCommandlet.generated.h"
UCLASS()
class UBarrageVFXReviewCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UBarrageVFXReviewCommandlet();
    virtual int32 Main(const FString& Params) override;
};
