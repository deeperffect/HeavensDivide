#include "IndicatorAttackVFX.h"
#include "CombatGround.h"
#include "EnemyBase.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "TacticalEnemy.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "EngineUtils.h"

AIndicatorAttackVFX::AIndicatorAttackVFX()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    SetActorEnableCollision(false);
    // CDO references make the copied systems and their dependencies cook with the game.
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> Impact(TEXT("/Game/HeavensDivide/VFX/ChargedAttacks/NS_ChargedImpact"));
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> Slash(TEXT("/Game/HeavensDivide/VFX/ChargedAttacks/NS_ChargedSlash"));
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> Fire(TEXT("/Game/HeavensDivide/VFX/ChargedAttacks/NS_ChargedFire"));
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> Frost(TEXT("/Game/HeavensDivide/VFX/ChargedAttacks/NS_ChargedFrost"));
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> Poison(TEXT("/Game/HeavensDivide/VFX/ChargedAttacks/NS_ChargedPoison"));
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> Flash(TEXT("/Game/HeavensDivide/VFX/ChargedAttacks/NS_ChargedFlash"));
    Systems = { Impact.Object, Slash.Object, Fire.Object, Frost.Object, Poison.Object, Flash.Object };
}

EIndicatorVFX AIndicatorAttackVFX::StyleFor(const AActor* Source)
{
    if (auto* Enemy = Cast<ATacticalEnemy>(Source))
        switch (Enemy->TacticalRole)
        {
        case ETacticalEnemyRole::FrostOracle: return EIndicatorVFX::Frost;
        case ETacticalEnemyRole::MireWeaver:
        case ETacticalEnemyRole::GraveCantor: return EIndicatorVFX::Poison;
        case ETacticalEnemyRole::StormGorilla: return EIndicatorVFX::Frost;
        case ETacticalEnemyRole::HexSniper: return EIndicatorVFX::Slash;
        case ETacticalEnemyRole::AshSeer: return EIndicatorVFX::Fire;
        default: return EIndicatorVFX::Impact;
        }
    return EIndicatorVFX::Fire;
}

TArray<AIndicatorAttackVFX::FSample> AIndicatorAttackVFX::Samples(EEncounterShape Shape, float Size, float Width, FVector ViewOffset)
{
    TArray<FSample> Result;
    Size = FMath::Max(1.f, Size);
    if (Shape == EEncounterShape::Circle)
        Result.Add({ FVector::ZeroVector, FVector(Size / 200, Size / 200, FMath::Min(1.f, Size / 200)), Size });
    else if (Shape == EEncounterShape::Lane)
    {
        // Uniform bursts avoid billboard sprites widening a narrow lane when
        // its long axis is stretched. Tile along the locked attack direction.
        const float Radius = FMath::Max(1.f, FMath::Min(Width, Size) * .4f);
        const int32 Count = FMath::Clamp(FMath::CeilToInt(Size / FMath::Max(1.f, Radius * 1.6f)), 1, 20);
        for (int32 I = 0; I < Count; ++I)
            Result.Add({ FVector(Count == 1 ? 0 : FMath::Lerp(-Size * .5f + Radius, Size * .5f - Radius, float(I) / (Count - 1)), 0, 0), FVector(Radius / 200), Radius });
    }
    else
    {
        const float Inner = FMath::Clamp(Width, 0.f, Size);
        const float Radius = FMath::Min(160.f, (Size - Inner) * .24f);
        if (Radius < 1) return Result;
        if (Size < 2200)
        {
            const float R = (Inner + Size) * .5f;
            const int32 Count = FMath::Clamp(FMath::CeilToInt(UE_TWO_PI * R / (Radius * 1.6f)), 8, 48);
            for (int32 I = 0; I < Count; ++I)
            {
                const float Angle = UE_TWO_PI * I / Count;
                Result.Add({ FVector(FMath::Cos(Angle) * R, FMath::Sin(Angle) * R, 0), FVector(Radius / 200), Radius });
            }
        }
        else
        {
            // Arena-wide annuli: show the part around the camera rather than allocating
            // hundreds of invisible systems around the whole map. Every burst stays
            // inside the damaging annulus, including its radius.
            const float Step = Radius * 2.5f;
            for (int32 X = -3; X <= 3; ++X)
            for (int32 Y = -3; Y <= 3; ++Y)
            {
                FVector Point = ViewOffset + FVector(X * Step, Y * Step, 0); Point.Z = 0;
                const float R = Point.Size2D();
                if (R >= Inner + Radius && R <= Size - Radius)
                    Result.Add({ Point, FVector(Radius / 200), Radius });
            }
        }
    }
    return Result;
}

AIndicatorAttackVFX* AIndicatorAttackVFX::Create(AActor* Source, FVector Center, float Yaw)
{
    if (!IsValid(Source) || !Source->GetWorld() || Source->GetWorld()->bIsTearingDown || Source->GetNetMode() == NM_DedicatedServer) return nullptr;
    int32 LiveBursts = 0;
    for (TActorIterator<AIndicatorAttackVFX> It(Source->GetWorld()); It; ++It)
        if (!It->IsActorBeingDestroyed()) LiveBursts += It->Bursts.Num();
    if (LiveBursts >= 512) return nullptr;
    FActorSpawnParameters Params; Params.Owner = Source;
    auto* FX = Source->GetWorld()->SpawnActor<AIndicatorAttackVFX>(Center, FRotator(0, Yaw, 0), Params);
    if (FX) FX->SetLifeSpan(3.f);
    return FX;
}

void AIndicatorAttackVFX::AddBurst(FVector Location, FVector Scale, EIndicatorVFX Style, bool bGround)
{
    auto* System = Systems[static_cast<uint8>(Style)].Get();
    if (!System) return;
    if (bGround)
    {
        FVector Surface;
        if (CombatGround::FindSurface(GetWorld(), Location, GetOwner(), Surface)) Location.Z = Surface.Z;
        else if (const auto* Character = Cast<ACharacter>(GetOwner()))
            if (FMath::Abs(Location.Z - Character->GetActorLocation().Z) < 10)
                Location.Z -= Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        Location.Z += 12;
    }
    // Components belong to this independent actor: warning cleanup cannot cut off particles.
    auto* FX = NewObject<UNiagaraComponent>(this);
    // Vendor distance/instance limits can drop all segments of a ring. This
    // bounded execution layer owns its budget instead of sharing those limits.
    FX->SetAllowScalability(false);
    // Ground indicators are translucent too. Without an explicit order, a
    // segment away from their centre sorts behind the entire warning plane.
    FX->SetTranslucentSortPriority(30);
    FX->SetAutoActivate(false); FX->SetAutoDestroy(false); FX->SetAsset(System);
    AddInstanceComponent(FX); FX->RegisterComponent();
    FX->SetWorldLocationAndRotation(Location, GetActorRotation()); FX->SetWorldScale3D(Scale);
    FX->SetRandomSeedOffset(static_cast<int32>(FPlatformTime::Cycles()));
    if (Style == EIndicatorVFX::Frost || Style == EIndicatorVFX::Poison || Style == EIndicatorVFX::Slash)
    {
        const FLinearColor Color = Style == EIndicatorVFX::Frost ? FLinearColor(.15f, 1.f, 3.f)
            : Style == EIndicatorVFX::Poison ? FLinearColor(.15f, 2.f, .4f) : FLinearColor(.3f, 1.5f, 3.f);
        TArray<FNiagaraVariable> Parameters; System->GetExposedParameters().GetParameters(Parameters);
        for (const auto& Parameter : Parameters)
            if (Parameter.GetType() == FNiagaraTypeDefinition::GetColorDef())
            {
                const FLinearColor Original = System->GetExposedParameters().GetParameterValue<FLinearColor>(Parameter);
                FX->SetVariableLinearColor(Parameter.GetName(), FLinearColor(Color.R, Color.G, Color.B, Original.A));
            }
    }
    Bursts.Add(FX);
    FX->Activate(true);
}

AIndicatorAttackVFX* AIndicatorAttackVFX::Spawn(AActor* Source, FVector Center, EEncounterShape Shape, float Size, float Width, float Yaw, EIndicatorVFX Style)
{
    auto* FX = Create(Source, Center, Yaw);
    if (!FX) return nullptr;
    FVector ViewOffset = FVector::ZeroVector;
    if (auto* Pawn = UGameplayStatics::GetPlayerPawn(Source, 0))
        ViewOffset = FX->GetActorRotation().UnrotateVector(Pawn->GetActorLocation() - Center);
    for (const auto& Sample : Samples(Shape, Size, Width, ViewOffset))
        FX->AddBurst(Center + FX->GetActorRotation().RotateVector(Sample.Offset), Sample.Scale, Style);
    if (FX->Bursts.IsEmpty()) { FX->Destroy(); return nullptr; }
    return FX;
}

AIndicatorAttackVFX* AIndicatorAttackVFX::Release(AActor* Source, FVector Location)
{
    auto* FX = Create(Source, Location, Source ? Source->GetActorRotation().Yaw : 0);
    if (FX) FX->AddBurst(Location, FVector(.3f), EIndicatorVFX::Flash, false);
    return FX;
}

AIndicatorAttackVFX* AIndicatorAttackVFX::BeginCharge(AActor* Source, float HitWidth, float Seconds)
{
    if (!IsValid(Source)) return nullptr;
    auto* FX = Create(Source, Source->GetActorLocation(), Source->GetActorRotation().Yaw);
    if (!FX) return nullptr;
    FX->ChargeSource = Source; FX->ChargeRadius = FMath::Max(10.f, HitWidth * .5f);
    FX->ChargeTime = FMath::Max(.01f, Seconds); FX->SinceBurst = .12f;
    FX->SetLifeSpan(FX->ChargeTime + 3); FX->SetActorTickEnabled(true);
    FX->Tick(0);
    return FX;
}

void AIndicatorAttackVFX::StopCharge()
{
    ChargeSource.Reset(); SetActorTickEnabled(false);
    // These are one-shot systems. Deactivate would kill their tails for vendor
    // emitters whose inactive response is Kill; simply stop creating new bursts.
    SetLifeSpan(3.f);
}

void AIndicatorAttackVFX::Tick(float Delta)
{
    Super::Tick(Delta);
    auto* Source = ChargeSource.Get();
    auto* Enemy = Cast<AEnemyBase>(Source);
    if (!Source || (Enemy && (Enemy->IsDead() || Enemy->IsGameplaySuspended())) || (ChargeTime -= Delta) <= 0)
    { StopCharge(); return; }
    // Retire completed bursts during long charges instead of growing the component list.
    for (int32 I = Bursts.Num() - 1; I >= 0; --I)
        if (!IsValid(Bursts[I]) || Bursts[I]->IsComplete())
        { if (IsValid(Bursts[I])) Bursts[I]->DestroyComponent(); Bursts.RemoveAt(I); }
    SinceBurst += Delta;
    if (SinceBurst >= .12f && Bursts.Num() < 16)
    {
        SinceBurst = 0;
        SetActorRotation(Source->GetActorRotation());
        AddBurst(Source->GetActorLocation(), FVector(ChargeRadius / 200), EIndicatorVFX::Impact);
    }
}
