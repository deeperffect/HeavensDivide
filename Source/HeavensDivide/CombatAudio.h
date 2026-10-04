#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Subsystems/WorldSubsystem.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CombatAudio.generated.h"

class USoundBase;
class UAudioComponent;

/** Editable mapping for combat events that do not already have an asset sound slot. */
UCLASS(BlueprintType)
class HEAVENSDIVIDE_API UCombatAudioPalette : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio") TMap<FName, TObjectPtr<USoundBase>> Sounds;
};

/** Hold the small combat palette for the run; no disk loading in attack callbacks. */
UCLASS()
class HEAVENSDIVIDE_API UCombatAudioSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    USoundBase* FindSound(FName Event) const;
private:
    UPROPERTY() TObjectPtr<UCombatAudioPalette> Palette;
};

UCLASS()
class HEAVENSDIVIDE_API UCombatAudioLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Audio|Combat", meta=(WorldContext="WorldContextObject"))
    static UAudioComponent* PlayEvent(const UObject* WorldContextObject, FName Event, FVector Location,
        bool bUI = false, float Volume = 1.f);
};
