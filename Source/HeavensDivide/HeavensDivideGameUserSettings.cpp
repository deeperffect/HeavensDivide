#include "HeavensDivideGameUserSettings.h"

#include "Engine/Engine.h"

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
