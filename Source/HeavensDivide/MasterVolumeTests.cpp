#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "HeavensDivideGameUserSettings.h"
#include "LevelAudioSubsystem.h"
#include "AudioDevice.h"
#include "AudioDeviceManager.h"
#include "AudioThread.h"
#include "ActiveSound.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundWave.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformProcess.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMasterVolumeTest,"HeavensDivide.Settings.MasterVolume",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMasterVolumeTest::RunTest(const FString&)
{
    const float PreviousVolume = FApp::GetVolumeMultiplier();
    const FString TestIni = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Automation/MasterVolume.ini"));
    TGuardValue<FString> UserIni(GGameUserSettingsIni, TestIni);
    TGuardValue<FString> EditorIni(GEditorSettingsIni, TestIni);
    auto* Settings = NewObject<UHeavensDivideGameUserSettings>();
    int32 Notifications=0;
    Settings->OnAudioVolumesChanged.AddLambda([&Notifications]{++Notifications;});
    Settings->SetMasterVolume(.35f);
    TestEqual(TEXT("Master changes notify the active game mix"),Notifications,1);
    Settings->SetMusicVolume(.2f);
    Settings->SetSoundEffectsVolume(.6f);
    TestEqual(TEXT("Game settings preserve the engine's focus/background gain"),FApp::GetVolumeMultiplier(),PreviousVolume);
    auto* Reloaded = NewObject<UHeavensDivideGameUserSettings>();
    Reloaded->LoadConfig(nullptr,*TestIni);
    TestEqual(TEXT("Volume persists"),Reloaded->GetMasterVolume(),.35f);
    TestEqual(TEXT("Music volume persists separately"),Reloaded->GetMusicVolume(),.2f);
    TestEqual(TEXT("Effects volume persists separately"),Reloaded->GetSoundEffectsVolume(),.6f);
    Settings->SetMusicVolume(-1); Settings->SetSoundEffectsVolume(2);
    TestEqual(TEXT("Music clamps to mute"),Settings->GetMusicVolume(),0.f);
    TestEqual(TEXT("Effects clamps to full"),Settings->GetSoundEffectsVolume(),1.f);
    TestEqual(TEXT("Independent gains preserve master"),Settings->GetMasterVolume(),.35f);
    Settings->SetMasterVolume(0);
    TestEqual(TEXT("Zero stores master mute"),Settings->GetMasterVolume(),0.f);
    Settings->SetMasterVolume(2);
    TestEqual(TEXT("Volume clamps to full"),Settings->GetMasterVolume(),1.f);
    Settings->SetMasterVolume(-1);
    TestEqual(TEXT("Volume cannot be negative"),Settings->GetMasterVolume(),0.f);
    int32 ReloadNotifications=0;
    Reloaded->OnAudioVolumesChanged.AddLambda([&ReloadNotifications]{++ReloadNotifications;});
    Reloaded->ApplyNonResolutionSettings();
    TestEqual(TEXT("Applying saved settings notifies the game mix"),ReloadNotifications,1);
    TestEqual(TEXT("Applying settings does not replace application gain"),FApp::GetVolumeMultiplier(),PreviousVolume);
    Settings->OnAudioVolumesChanged.Clear();Reloaded->OnAudioVolumesChanged.Clear();
    return true;
}

// Exercise the actual audio-device class properties and world/settings subscription,
// rather than only checking a stored setting or duplicating the gain formula.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUserVolumeMixTest,"HeavensDivide.Audio.UserVolumeMix",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FUserVolumeMixTest::RunTest(const FString&)
{
    auto* Manager=GEngine->GetAudioDeviceManager();
    if (!TestNotNull(TEXT("Audio test requires audio enabled (omit -nosound)"),Manager)) return false;
    const FString TestIni=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Automation/UserVolumeMix.ini"));
    TGuardValue<FString> UserIni(GGameUserSettingsIni,TestIni);
    TGuardValue<FString> EditorIni(GEditorSettingsIni,TestIni);
    auto* Settings=NewObject<UHeavensDivideGameUserSettings>();
    TGuardValue<TObjectPtr<UGameUserSettings>> SettingsGuard(GEngine->GameUserSettings,Settings);
    const float PreviousAppGain=FApp::GetVolumeMultiplier();
    // Reproduce the editor resetting its own gain after the player's slider changes.
    FApp::SetVolumeMultiplier(1.f);
    Settings->SetMasterVolume(.35f);Settings->SetMusicVolume(.2f);Settings->SetSoundEffectsVolume(.6f);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    FAudioDeviceParams Params;Params.Scope=EAudioDeviceScope::Unique;Params.bIsNonRealtime=true;Params.AssociatedWorld=World;
    auto Handle=Manager->RequestAudioDevice(Params);
    FAudioDevice* Device=Handle.GetAudioDevice();
    if (!TestNotNull(TEXT("Isolated offline audio mixer initializes"),Device))
    { World->DestroyWorld(false);GEngine->DestroyWorldContext(World);FApp::SetVolumeMultiplier(PreviousAppGain);return false; }
    World->SetAudioDevice(Handle);
    auto* Audio=World->GetSubsystem<ULevelAudioSubsystem>();
    Audio->OnWorldBeginPlay(*World);
    TestTrue(TEXT("Ambience routes through the effects class"),Audio->LevelOneAmbience && Audio->LevelOneAmbience->SoundClassObject==Audio->SoundEffectsClass);
    Audio->Ambience=UGameplayStatics::SpawnSound2D(World,Audio->LevelOneAmbience,1.f,1.f,0.f,nullptr,false,false);
    TestNotNull(TEXT("Existing ambience loop starts on the game mixer"),Audio->Ambience.Get());
    auto ExpectMix=[&](const TCHAR* Label,float Effects,float Music)
    {
        float ActualEffects=-1,ActualMusic=-1;
        const double Deadline=FPlatformTime::Seconds()+3;
        do
        {
            Device->Update(true);
            FAudioThread::RunCommandOnAudioThread([&]
            {
                if (const auto* P=Device->GetSoundClassCurrentProperties(Audio->SoundEffectsClass)) ActualEffects=P->Volume;
                if (const auto* P=Device->GetSoundClassCurrentProperties(Audio->MusicClass)) ActualMusic=P->Volume;
            });
            FAudioCommandFence Fence;Fence.BeginFence();Fence.Wait();
            if (FMath::IsNearlyEqual(ActualEffects,Effects,.001f) && FMath::IsNearlyEqual(ActualMusic,Music,.001f)) break;
            FPlatformProcess::Sleep(.01f);
        } while(FPlatformTime::Seconds()<Deadline);
        AddInfo(FString::Printf(TEXT("%s: effects=%.4f music=%.4f"),Label,ActualEffects,ActualMusic));
        TestTrue(FString(Label)+TEXT(" reaches the ambience/effects bus"),FMath::IsNearlyEqual(ActualEffects,Effects,.001f));
        TestTrue(FString(Label)+TEXT(" reaches the music bus"),FMath::IsNearlyEqual(ActualMusic,Music,.001f));
    };
    ExpectMix(TEXT("Saved gains at world start"),.21f,.07f);
    Settings->SetMasterVolume(0);FApp::SetVolumeMultiplier(1.f);
    ExpectMix(TEXT("Master mute survives the editor resetting app gain"),0,0);
    bool bMutedLoopAlive=false;
    FAudioThread::RunCommandOnAudioThread([&]
    {
        for (const FActiveSound* Sound:Device->GetActiveSounds())
            if (Sound->GetSound()==Audio->LevelOneAmbience && !Sound->bIsStopping)
                bMutedLoopAlive=Sound->IsPlayWhenSilent() && Sound->GetSoundClass()==Audio->SoundEffectsClass;
    });
    { FAudioCommandFence Fence;Fence.BeginFence();Fence.Wait(); }
    TestTrue(TEXT("Muted ambience remains alive on the controlled effects bus"),bMutedLoopAlive);
    Settings->SetMasterVolume(1);
    ExpectMix(TEXT("Unmuting restores independent category preferences"),.6f,.2f);
    Settings->SetSoundEffectsVolume(0);
    ExpectMix(TEXT("Effects mute preserves music"),0,.2f);
    Settings->SetSoundEffectsVolume(.6f);Settings->SetMasterVolume(.4f);
    ExpectMix(TEXT("Changing master during the loop"),.24f,.08f);
    World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
    auto* Reloaded=NewObject<UHeavensDivideGameUserSettings>();Reloaded->LoadConfig(nullptr,*TestIni);
    GEngine->GameUserSettings=Reloaded;
    World=UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    Params.AssociatedWorld=World;
    Handle=Manager->RequestAudioDevice(Params);Device=Handle.GetAudioDevice();
    if (!TestNotNull(TEXT("New world's isolated audio mixer initializes"),Device))
    { World->DestroyWorld(false);GEngine->DestroyWorldContext(World);FApp::SetVolumeMultiplier(PreviousAppGain);return false; }
    World->SetAudioDevice(Handle);
    Audio=World->GetSubsystem<ULevelAudioSubsystem>();Audio->OnWorldBeginPlay(*World);
    ExpectMix(TEXT("Reloaded settings and world travel apply master once"),.24f,.08f);
    World->DestroyWorld(false);GEngine->DestroyWorldContext(World);
    FAudioCommandFence Fence;Fence.BeginFence();Fence.Wait();
    FApp::SetVolumeMultiplier(PreviousAppGain);
    return !HasAnyErrors();
}
#endif
