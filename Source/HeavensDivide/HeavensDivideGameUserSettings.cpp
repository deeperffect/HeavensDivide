#include "HeavensDivideGameUserSettings.h"

#include "Engine/Engine.h"

void UHeavensDivideGameUserSettings::SetCameraShakeIntensity(float Intensity)
{
	const float NewValue = FMath::IsFinite(Intensity) ? FMath::Clamp(Intensity, 0.0f, 1.0f) : 1.0f;
	if (CameraShakeIntensity == NewValue) return;
	CameraShakeIntensity = NewValue;
	OnCameraShakeIntensityChanged.Broadcast(NewValue);
	SaveSettings();
}

UHeavensDivideGameUserSettings* UHeavensDivideGameUserSettings::GetHeavensDivideGameUserSettings()
{
	return GEngine ? Cast<UHeavensDivideGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
}

void UHeavensDivideGameUserSettings::SetAutoTargetingEnabled(bool bEnabled)
{
	if (bAutoTargetingEnabled == bEnabled)
	{
		return;
	}

	bAutoTargetingEnabled = bEnabled;
	SaveSettings();
}
