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
    if (Sound) UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation(), FMath::Max(0.f, Volume));
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
    if (Age >= HoldDuration + ClosingDuration) { Destroy(); return; }
    float Width = OpeningDuration > 0 ? FMath::Clamp(Age / OpeningDuration, 0.f, 1.f) : 1.f;
    if (Age > HoldDuration && ClosingDuration > 0)
        Width = 1.f - FMath::Clamp((Age - HoldDuration) / ClosingDuration, 0.f, 1.f);
    Width = Width * Width * (3.f - 2.f * Width);
    FVector Scale = FullScale;
    const int32 Axis = WidthAxis == EAxis::X ? 0 : WidthAxis == EAxis::Z ? 2 : 1;
    Scale[Axis] *= FMath::Max(.001f, Width);
    SetActorScale3D(Scale);
}
