#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "InputCoreTypes.h"
#include "HeavensDivideGameUserSettings.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnCameraShakeIntensityChanged, float);
DECLARE_MULTICAST_DELEGATE(FOnKeybindingsChanged);

UCLASS()
class HEAVENSDIVIDE_API UHeavensDivideGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	FOnKeybindingsChanged OnKeybindingsChanged;
	static TArray<FName> GetBindableActions();
	static FKey GetDefaultBinding(FName Action);
	FKey GetKeyBinding(FName Action) const;
	bool SetKeyBinding(FName Action, FKey Key);
	void ResetKeyBindings();
	FOnCameraShakeIntensityChanged OnCameraShakeIntensityChanged;
	UFUNCTION(BlueprintPure, Category="Settings|Accessibility")
	float GetCameraShakeIntensity() const { return FMath::IsFinite(CameraShakeIntensity) ? FMath::Clamp(CameraShakeIntensity, 0.0f, 1.0f) : 1.0f; }
	UFUNCTION(BlueprintCallable, Category="Settings|Accessibility")
	void SetCameraShakeIntensity(float Intensity);
	UFUNCTION(BlueprintPure, Category = "Settings|Gameplay")
	static UHeavensDivideGameUserSettings* GetHeavensDivideGameUserSettings();

	UFUNCTION(BlueprintPure, Category = "Settings|Gameplay")
	bool IsAutoTargetingEnabled() const { return bAutoTargetingEnabled; }

	UFUNCTION(BlueprintCallable, Category = "Settings|Gameplay")
	void SetAutoTargetingEnabled(bool bEnabled);

private:
	UPROPERTY(Config) TMap<FName,FKey> KeybindOverrides;
	UPROPERTY(Config) float CameraShakeIntensity = 1.0f;
	UPROPERTY(Config)
	bool bAutoTargetingEnabled = true;
};
