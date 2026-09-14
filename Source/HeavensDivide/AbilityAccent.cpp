#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "CharacterStatsComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnemyStatusEffectComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NinjaCharacter.h"
#include "PlayerUpgradeComponent.h"
#include "SamuraiCharacter.h"
#include "SharedPlayerStatsComponent.h"
#include "SurvivorAbilityComponent.h"
#include "SurvivorPlayerController.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AAbilityAccent::AAbilityAccent()
{
    PrimaryActorTick.bCanEverTick = true;
    Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Accent"));
    SetRootComponent(Visual);
    Niagara = CreateDefaultSubobject<UNiagaraComponent>(TEXT("UpgradeNiagara"));
    Niagara->SetupAttachment(Visual);
    Niagara->SetAutoActivate(false);
    Niagara->SetAutoDestroy(false);
    Niagara->SetAbsolute(false, false, true);
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Visual->SetCastShadow(false);
    // These tiny, short-lived primitives do not benefit from Nanite.
    Visual->SetForceDisableNanite(true);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
    Visual->SetStaticMesh(Plane.Object);
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Ring(
        TEXT("/Game/HeavensDivide/Materials/M_AbilityRing"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Beam(
        TEXT("/Game/HeavensDivide/Materials/M_AbilityStreak"));
    RingMaterial = Ring.Object;
    BeamMaterial = Beam.Object;
}

void AAbilityAccent::Initialize(FVector End, float Radius, FLinearColor Color, float Duration, bool bBeam,
                                const FUpgradePresentation *Settings, int32 Stage)
{
    const FVector Start = GetActorLocation();
    if (Settings)
        End += Settings->EndOffset;
    if (Settings)
        Duration = (Settings->LifetimeOverride > 0 ? Settings->LifetimeOverride : Duration) *
                   FMath::Max(0.01f, Settings->LifetimeMultiplier);
    Lifetime = FMath::Max(0.05f, Duration);
    bIsBeam = bBeam;
    if (bBeam)
    {
        Visual->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
        const FVector Delta = End - GetActorLocation();
        SetActorLocation(GetActorLocation() + Delta * 0.5f);
        SetActorRotation(Delta.Rotation());
        InitialScale = FVector(FMath::Max(1.0f, Delta.Size()) / 100.0f, 0.035f, 0.035f);
    }
    else
        InitialScale = FVector(Radius / 50.0f, Radius / 50.0f, 1);
    SetActorScale3D(InitialScale);
    if (Settings)
    {
        FallbackIntensityParameter = Settings->FallbackIntensityParameter;
        bFadeFallback = Settings->bFadeFallback;
        if (Settings->bOverrideColor)
            Color = Settings->Color;
        if (Settings->FallbackRingMaterial)
            RingMaterial = Settings->FallbackRingMaterial;
        if (Settings->FallbackLineMaterial)
            BeamMaterial = Settings->FallbackLineMaterial;
        if (bBeam)
        {
            InitialScale.Y = InitialScale.Z = FMath::Max(0.001f, Settings->FallbackLineThickness / 100.0f);
            SetActorScale3D(InitialScale);
        }
        InitialScale *= Settings->Scale;
        SetActorScale3D(InitialScale);
        AddActorLocalRotation(Settings->RotationOffset);
        UNiagaraSystem *System = Stage == 1   ? Settings->WarningSystem
                                 : Stage == 2 ? Settings->ImpactSystem
                                 : Stage == 3 ? Settings->DetonationSystem
                                 : bBeam      ? Settings->LineSystem
                                              : Settings->PulseSystem;
        if (!System && Stage == 3)
            System = Settings->PulseSystem;
        Visual->SetVisibility(System ? Settings->bShowFallbackWithNiagara : Settings->bShowFallbackWithoutNiagara,
                              false);
        PresentationOffset = Settings->LocationOffset;
        AddActorWorldOffset(PresentationOffset);
        if (System)
        {
            Niagara->SetAsset(System);
            Niagara->SetWorldLocation(Start + Settings->LocationOffset);
            Niagara->SetWorldRotation((bBeam ? (End - Start).Rotation() : FRotator::ZeroRotator) +
                                      Settings->RotationOffset);
            const float AreaScale =
                Settings->bScaleSystemToRadius && Radius > 0 ? Radius / FMath::Max(1.0f, Settings->AuthoredRadius) : 1;
            Niagara->SetWorldScale3D(Settings->Scale * AreaScale);
            if (!Settings->RadiusParameter.IsNone())
                Niagara->SetVariableFloat(Settings->RadiusParameter, Radius);
            if (!Settings->DurationParameter.IsNone())
                Niagara->SetVariableFloat(Settings->DurationParameter, Lifetime);
            if (!Settings->StartParameter.IsNone())
                Niagara->SetVariableVec3(Settings->StartParameter, Start + Settings->LocationOffset);
            if (!Settings->EndParameter.IsNone())
                Niagara->SetVariableVec3(Settings->EndParameter, End + Settings->LocationOffset);
            if (!Settings->ColorParameter.IsNone())
                Niagara->SetVariableLinearColor(Settings->ColorParameter, Color);
            NiagaraStartParameter = Settings->StartParameter;
            NiagaraEndParameter = Settings->EndParameter;
            NiagaraLocalEnd = Niagara->GetComponentQuat().UnrotateVector(End - Start);
            Niagara->Activate(true);
        }
        if (Settings->Sound)
            UGameplayStatics::PlaySoundAtLocation(this, Settings->Sound, Start, FRotator::ZeroRotator,
                                                  Settings->SoundVolume, FMath::Max(0.01f, Settings->SoundPitch), 0,
                                                  nullptr, Settings->SoundConcurrency);
    }
    Material = UMaterialInstanceDynamic::Create(bBeam ? BeamMaterial : RingMaterial, this);
    if (Material)
    {
        Material->SetVectorParameterValue(Settings ? Settings->FallbackColorParameter : FName(TEXT("Tint")), Color);
        Visual->SetMaterial(0, Material);
    }
    SetLifeSpan(Lifetime + 0.1f);
}

void AAbilityAccent::Tick(float Delta)
{
    Super::Tick(Delta);
    Age += Delta;
    if (Niagara && Niagara->GetAsset())
    {
        const FVector Start = Niagara->GetComponentLocation();
        if (!NiagaraStartParameter.IsNone())
            Niagara->SetVariableVec3(NiagaraStartParameter, Start);
        if (!NiagaraEndParameter.IsNone())
            Niagara->SetVariableVec3(NiagaraEndParameter,
                                     Start + Niagara->GetComponentQuat().RotateVector(NiagaraLocalEnd));
    }
    const float Alpha = FMath::Clamp(Age / Lifetime, 0.0f, 1.0f);
    if (Material)
        Material->SetScalarParameterValue(FallbackIntensityParameter, bFadeFallback ? 1 - Alpha : 1);
    // Keep rings aligned to their damage footprint; brightness provides the pulse.
    if (bIsBeam)
        SetActorScale3D(InitialScale * FVector(1, FMath::Max(0.1f, 1 - Alpha), FMath::Max(0.1f, 1 - Alpha)));
    if (Age >= Lifetime)
        Destroy();
}
