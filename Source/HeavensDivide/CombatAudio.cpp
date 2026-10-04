#include "CombatAudio.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

bool UCombatAudioSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const auto* World = Cast<UWorld>(Outer);
    return Super::ShouldCreateSubsystem(Outer) && World && World->IsGameWorld() && !IsRunningDedicatedServer();
}
void UCombatAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Palette = LoadObject<UCombatAudioPalette>(nullptr, TEXT("/Game/HeavensDivide/Audio/DA_CombatAudio.DA_CombatAudio"));
}
USoundBase* UCombatAudioSubsystem::FindSound(FName Event) const
{
    const auto* Sound = Palette ? Palette->Sounds.Find(Event) : nullptr;
    return Sound ? Sound->Get() : nullptr;
}
UAudioComponent* UCombatAudioLibrary::PlayEvent(const UObject* Context, FName Event, FVector Location, bool bUI, float Volume)
{
    UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(Context, EGetWorldErrorMode::ReturnNull) : nullptr;
    if (!World || World->GetNetMode() == NM_DedicatedServer || !FMath::IsFinite(Volume) || Volume <= 0.f) return nullptr;
    auto* Audio = World->GetSubsystem<UCombatAudioSubsystem>();
    USoundBase* Sound = Audio ? Audio->FindSound(Event) : nullptr;
    if (!Sound) return nullptr;
    return bUI ? UGameplayStatics::SpawnSound2D(Context, Sound, Volume)
        : UGameplayStatics::SpawnSoundAtLocation(Context, Sound, Location, FRotator::ZeroRotator, Volume);
}
