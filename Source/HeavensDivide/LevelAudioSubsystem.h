#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LevelAudioSubsystem.generated.h"
class USoundClass;
class USoundMix;
class USoundWave;
class UAudioComponent;
class UHeavensDivideGameUserSettings;
/** World-scoped audio mix and Level 1 ambience; travel stops the old loop. */
UCLASS()
class HEAVENSDIVIDE_API ULevelAudioSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    ULevelAudioSubsystem();
    virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;
    virtual void OnWorldBeginPlay(UWorld& World) override;
    virtual void Deinitialize() override;
    void ApplyVolumes();
    UPROPERTY() TObjectPtr<USoundClass> SoundEffectsClass;
    UPROPERTY() TObjectPtr<USoundClass> MusicClass;
    UPROPERTY() TObjectPtr<USoundMix> VolumeMix;
    UPROPERTY() TObjectPtr<USoundWave> LevelOneAmbience;
    UPROPERTY() TObjectPtr<UAudioComponent> Ambience;
private:
    TWeakObjectPtr<UHeavensDivideGameUserSettings> Settings;
    bool bMixPushed=false;
};
