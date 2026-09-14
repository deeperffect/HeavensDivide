#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "HeavensDivideGameUserSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMasterVolumeTest,"HeavensDivide.Settings.MasterVolume",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMasterVolumeTest::RunTest(const FString&)
{
    const float PreviousVolume = FApp::GetVolumeMultiplier();
    const FString TestIni = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Automation/MasterVolume.ini"));
    TGuardValue<FString> UserIni(GGameUserSettingsIni, TestIni);
    TGuardValue<FString> EditorIni(GEditorSettingsIni, TestIni);
    auto* Settings = NewObject<UHeavensDivideGameUserSettings>();
    Settings->SetMasterVolume(.35f);
    TestEqual(TEXT("Volume applies immediately to game audio"),FApp::GetVolumeMultiplier(),.35f);
    auto* Reloaded = NewObject<UHeavensDivideGameUserSettings>();
    Reloaded->LoadConfig(nullptr,*TestIni);
    TestEqual(TEXT("Volume persists"),Reloaded->GetMasterVolume(),.35f);
    Settings->SetMasterVolume(0);
    TestEqual(TEXT("Zero mutes audio"),FApp::GetVolumeMultiplier(),0.f);
    Settings->SetMasterVolume(2);
    TestEqual(TEXT("Volume clamps to full"),FApp::GetVolumeMultiplier(),1.f);
    Settings->SetMasterVolume(-1);
    TestEqual(TEXT("Volume cannot be negative"),FApp::GetVolumeMultiplier(),0.f);
    Reloaded->ApplyNonResolutionSettings();
    TestEqual(TEXT("Applying saved settings restores volume"),FApp::GetVolumeMultiplier(),.35f);
    FApp::SetVolumeMultiplier(PreviousVolume);
    return true;
}
#endif
