#include "SwapPortal.h"
#include "NiagaraComponent.h"
#include "Misc/App.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

ASwapPortal::ASwapPortal()
{
    PrimaryActorTick.bCanEverTick = true;
    Effect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Portal"));
    SetRootComponent(Effect);
    Effect->SetAutoActivate(false);
    SetActorEnableCollision(false);
}

void ASwapPortal::Initialize(UNiagaraSystem* System, float Duration, float OpenDuration,
    float CloseDuration, EAxis::Type SqueezeAxis, USoundBase* Sound, float Volume)
{
    if (!System) { Destroy(); return; }
    HoldDuration = FMath::Max(.01f, Duration);
    OpeningDuration = FMath::Min(FMath::Max(0.f, OpenDuration), HoldDuration);
    ClosingDuration = FMath::Max(0.f, CloseDuration);
    WidthAxis = SqueezeAxis;
    FullScale = GetActorScale3D();
    Age = 0;
    AdvancePresentation(0);
    Effect->SetAsset(System);
    // Use the normal Niagara tick, including GPU emitters, at presentation speed.
    Effect->SetForceSolo(true);
    Effect->AddTickPrerequisiteActor(this);
    Effect->Activate(true);
    PendingSound = Sound;
    SoundVolume = FMath::Max(0.f, Volume);
    if (SoundDelaySeconds <= 0) PlayPendingSound();
}

void ASwapPortal::ConfigurePolish(float Expansion, UNiagaraSystem* Burst, float SoundDelay, float BurstTimeOffset)
{
    CloseExpansion = FMath::Clamp(Expansion, 0.f, 1.f);
    CloseBurst = Burst;
    SoundDelaySeconds = FMath::Max(0.f, SoundDelay);
    BurstOffset = BurstTimeOffset;
}

void ASwapPortal::PlayPendingSound()
{
    if (PendingSound) UGameplayStatics::PlaySoundAtLocation(this, PendingSound, GetActorLocation(), SoundVolume);
    PendingSound = nullptr;
}

void ASwapPortal::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!GetOwner() || GetOwner()->IsActorBeingDestroyed()) { Destroy(); return; }
    const float Delta = FMath::Max(0.f, static_cast<float>(FApp::GetDeltaTime()));
    AdvancePresentation(Delta);
    if (IsActorBeingDestroyed()) return;
    const float WorldDelta = GetWorld()->GetDeltaSeconds();
    Effect->SetCustomTimeDilation(WorldDelta > SMALL_NUMBER ? Delta / WorldDelta : 1.f);
}

void ASwapPortal::AdvancePresentation(float Delta)
{
    Age += FMath::Max(0.f, Delta);
    if (Age >= SoundDelaySeconds) PlayPendingSound();
    const float BurstTime = FMath::Clamp(HoldDuration + BurstOffset,0.f,HoldDuration + ClosingDuration);
    if (Age >= BurstTime && !bCloseBurstPlayed)
    {
        bCloseBurstPlayed = true;
        if (CloseBurst && GetWorld())
        {
            FActorSpawnParameters Params; Params.Owner = GetOwner();
            const FTransform Transform(GetActorQuat(), GetActorLocation(), FullScale);
            if (auto* Burst = GetWorld()->SpawnActor<ASwapPortal>(StaticClass(), Transform, Params))
                Burst->Initialize(CloseBurst, .6f, 0, 0);
        }
    }
    if (Age >= HoldDuration + ClosingDuration) { Destroy(); return; }
    float Width = OpeningDuration > 0 ? FMath::Clamp(Age / OpeningDuration, 0.f, 1.f) : 1.f;
    if (Age > HoldDuration && ClosingDuration > 0)
        Width = 1.f - FMath::Clamp((Age - HoldDuration) / ClosingDuration, 0.f, 1.f);
    Width = Width * Width * (3.f - 2.f * Width);
    if (Age > HoldDuration && ClosingDuration > 0)
    {
        const float CloseAlpha = FMath::Clamp((Age - HoldDuration) / ClosingDuration, 0.f, 1.f);
        if (CloseExpansion > 0)
        {
            // Expand first, then squeeze from the expanded width. Do not shrink
            // underneath the expansion pulse, which previously hid most of it.
            const float Phase = CloseAlpha < .4f ? CloseAlpha / .4f : (CloseAlpha - .4f) / .6f;
            const float Ease = Phase * Phase * (3.f - 2.f * Phase);
            Width = CloseAlpha < .4f ? FMath::Lerp(1.f, 1.f + CloseExpansion, Ease)
                : FMath::Lerp(1.f + CloseExpansion, 0.f, Ease);
        }
    }
    FVector Scale = FullScale;
    const int32 Axis = WidthAxis == EAxis::X ? 0 : WidthAxis == EAxis::Z ? 2 : 1;
    Scale[Axis] *= FMath::Max(.001f, Width);
    SetActorScale3D(Scale);
}
