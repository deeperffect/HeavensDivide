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
    virtual void LoadSettings(bool bForceReload = false) override;
    virtual void ApplyNonResolutionSettings() override;
    UFUNCTION(BlueprintPure, Category="Settings|Audio")
    float GetMasterVolume() const { return FMath::IsFinite(MasterVolume) ? FMath::Clamp(MasterVolume, 0.f, 1.f) : 1.f; }
    UFUNCTION(BlueprintCallable, Category="Settings|Audio")
    void SetMasterVolume(float Volume);
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
    UPROPERTY(Config) float MasterVolume = 1.f;
	UPROPERTY(Config) TMap<FName,FKey> KeybindOverrides;
	UPROPERTY(Config) float CameraShakeIntensity = 1.0f;
	UPROPERTY(Config)
	bool bAutoTargetingEnabled = true;
};
