#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "EncounterReviewCommandlet.generated.h"
UCLASS()
class UEncounterReviewCommandlet : public UCommandlet
{
 GENERATED_BODY()
public:
 UEncounterReviewCommandlet();
 virtual int32 Main(const FString& Params) override;
};
