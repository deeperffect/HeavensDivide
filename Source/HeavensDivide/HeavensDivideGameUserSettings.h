#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "HeavensDivideGameUserSettings.generated.h"

UCLASS()
class HEAVENSDIVIDE_API UHeavensDivideGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Settings|Gameplay")
	static UHeavensDivideGameUserSettings* GetHeavensDivideGameUserSettings();

	UFUNCTION(BlueprintPure, Category = "Settings|Gameplay")
	bool IsAutoTargetingEnabled() const { return bAutoTargetingEnabled; }

	UFUNCTION(BlueprintCallable, Category = "Settings|Gameplay")
	void SetAutoTargetingEnabled(bool bEnabled);

private:
	UPROPERTY(Config)
	bool bAutoTargetingEnabled = true;
};
