#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "SamuraiVFXScaleSetupCommandlet.generated.h"

// Editor-only asset migration: secondary emitters use baseline scale, ribbon uses area scale.
UCLASS()
class USamuraiVFXScaleSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	USamuraiVFXScaleSetupCommandlet();
	virtual int32 Main(const FString& Params) override;
};
