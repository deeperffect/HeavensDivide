#include "LevelAudioSubsystem.h"
#include "HeavensDivideGameUserSettings.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundWave.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
ULevelAudioSubsystem::ULevelAudioSubsystem()
{
    static ConstructorHelpers::FObjectFinder<USoundClass> SFX(TEXT("/Game/HeavensDivide/Audio/Mix/SC_SoundEffects.SC_SoundEffects"));
    static ConstructorHelpers::FObjectFinder<USoundClass> Music(TEXT("/Game/HeavensDivide/Audio/Mix/SC_Music.SC_Music"));
    static ConstructorHelpers::FObjectFinder<USoundMix> Mix(TEXT("/Game/HeavensDivide/Audio/Mix/SM_UserVolumes.SM_UserVolumes"));
    static ConstructorHelpers::FObjectFinder<USoundWave> Loop(TEXT("/Game/HeavensDivide/Audio/Ambience/W_Ambience_Lvl1.W_Ambience_Lvl1"));
    SoundEffectsClass=SFX.Object; MusicClass=Music.Object; VolumeMix=Mix.Object; LevelOneAmbience=Loop.Object;
}
bool ULevelAudioSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type==EWorldType::Game || Type==EWorldType::PIE; }
void ULevelAudioSubsystem::OnWorldBeginPlay(UWorld& World)
{
    Super::OnWorldBeginPlay(World);
    Settings=UHeavensDivideGameUserSettings::GetHeavensDivideGameUserSettings();
    if (Settings.IsValid()) Settings->OnAudioVolumesChanged.AddUObject(this,&ULevelAudioSubsystem::ApplyVolumes);
    if (VolumeMix) { UGameplayStatics::PushSoundMixModifier(this,VolumeMix); bMixPushed=true; }
    ApplyVolumes();
    const FString Map=UGameplayStatics::GetCurrentLevelName(this,true);
    if (Map==TEXT("Lvl_B1_Lvl1") && LevelOneAmbience)
        Ambience=UGameplayStatics::SpawnSound2D(this,LevelOneAmbience,1.f,1.f,0.f,nullptr,false,false);
}
void ULevelAudioSubsystem::ApplyVolumes()
{
    if (!VolumeMix) return;
    // FApp's gain belongs to editor focus/background audio and is reset each PIE frame.
    // Keep player preferences in the world mix, applying master exactly once per bus.
    const float Master=Settings.IsValid()?Settings->GetMasterVolume():1.f;
    UGameplayStatics::SetSoundMixClassOverride(this,VolumeMix,SoundEffectsClass,Master*(Settings.IsValid()?Settings->GetSoundEffectsVolume():1.f),1.f,.1f,true);
    UGameplayStatics::SetSoundMixClassOverride(this,VolumeMix,MusicClass,Master*(Settings.IsValid()?Settings->GetMusicVolume():1.f),1.f,.1f,true);
}
void ULevelAudioSubsystem::Deinitialize()
{
    if (Settings.IsValid()) Settings->OnAudioVolumesChanged.RemoveAll(this);
    if (Ambience) { Ambience->Stop(); Ambience->DestroyComponent(); Ambience=nullptr; }
    if (bMixPushed) UGameplayStatics::PopSoundMixModifier(this,VolumeMix);
    bMixPushed=false; Super::Deinitialize();
}
