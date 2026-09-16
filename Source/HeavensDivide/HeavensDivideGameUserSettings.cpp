#include "HeavensDivideGameUserSettings.h"

#include "Engine/Engine.h"
#include "Misc/App.h"

TArray<FName> UHeavensDivideGameUserSettings::GetBindableActions()
{
	return {TEXT("MoveForward"),TEXT("MoveBackward"),TEXT("MoveLeft"),TEXT("MoveRight"),TEXT("Swap"),TEXT("Dash"),TEXT("Interact"),TEXT("ComboAbility")};
}

FKey UHeavensDivideGameUserSettings::GetDefaultBinding(FName Action)
{
	if(Action==TEXT("MoveForward"))return EKeys::W;
	if(Action==TEXT("MoveBackward"))return EKeys::S;
	if(Action==TEXT("MoveLeft"))return EKeys::A;
	if(Action==TEXT("MoveRight"))return EKeys::D;
	if(Action==TEXT("Swap"))return EKeys::RightMouseButton;
	if(Action==TEXT("Dash"))return EKeys::SpaceBar;
	if(Action==TEXT("Interact"))return EKeys::E;
	if(Action==TEXT("ComboAbility"))return EKeys::Q;
	return EKeys::Invalid;
}

FKey UHeavensDivideGameUserSettings::GetKeyBinding(FName Action) const
{
	const FKey* Key=KeybindOverrides.Find(Action);
	return Key&&Key->IsValid()?*Key:GetDefaultBinding(Action);
}

bool UHeavensDivideGameUserSettings::SetKeyBinding(FName Action,FKey Key)
{
	if(!GetDefaultBinding(Action).IsValid()||!Key.IsValid()||Key.IsGamepadKey()||Key.IsAnalog()||Key==EKeys::Escape||Key==EKeys::Tilde)return false;
	const FKey Previous=GetKeyBinding(Action);
	if(Previous==Key)return true;
	// Exchange conflicting bindings so no action becomes unreachable.
	for(FName Other:GetBindableActions())if(Other!=Action&&GetKeyBinding(Other)==Key)KeybindOverrides.Add(Other,Previous);
	KeybindOverrides.Add(Action,Key);
	SaveSettings();OnKeybindingsChanged.Broadcast();return true;
}

void UHeavensDivideGameUserSettings::ResetKeyBindings()
{
	KeybindOverrides.Reset();SaveSettings();OnKeybindingsChanged.Broadcast();
}

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

void UHeavensDivideGameUserSettings::LoadSettings(bool bForceReload)
{
    Super::LoadSettings(bForceReload);
    // Older saves may already use Q. Give the newly introduced action a free key.
    const FName ComboAction(TEXT("ComboAbility"));
    const auto IsOccupied = [this, ComboAction](FKey Key) {
        for (FName Action : GetBindableActions())
            if (Action != ComboAction && GetKeyBinding(Action) == Key) return true;
        return false;
    };
    if (IsOccupied(GetKeyBinding(ComboAction)))
    {
        for (FKey Candidate : {EKeys::Q, EKeys::F, EKeys::R, EKeys::T, EKeys::Y, EKeys::U, EKeys::I, EKeys::O})
            if (!IsOccupied(Candidate)) { KeybindOverrides.Add(ComboAction, Candidate); break; }
    }
    FApp::SetVolumeMultiplier(GetMasterVolume());
}

void UHeavensDivideGameUserSettings::ApplyNonResolutionSettings()
{
    Super::ApplyNonResolutionSettings();
    FApp::SetVolumeMultiplier(GetMasterVolume());
}

void UHeavensDivideGameUserSettings::SetMasterVolume(float Volume)
{
    MasterVolume = FMath::IsFinite(Volume) ? FMath::Clamp(Volume, 0.f, 1.f) : 1.f;
    FApp::SetVolumeMultiplier(MasterVolume);
    SaveSettings();
}
