#include "EncounterHazard.h"
#include "EnemyBase.h"
#include "CharacterBase.h"
#include "SurvivorPlayerController.h"
#include "Components/DecalComponent.h"
#include "Components/CapsuleComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "EngineUtils.h"
#include "CombatAudio.h"

AEncounterHazard::AEncounterHazard()
{
    PrimaryActorTick.bCanEverTick = true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    Visual = CreateDefaultSubobject<UDecalComponent>(TEXT("GroundWarning"));
    Visual->SetupAttachment(RootComponent);
    Visual->SetRelativeRotation(FRotator(-90,0,0));
    Visual->SetComponentTickEnabled(false);
    Visual->SetFadeScreenSize(.001f);
    Visual->SortOrder=20;
}

AEncounterHazard* AEncounterHazard::Spawn(AActor* Source, FVector Center, EEncounterShape Shape, float Size,
    float Width, float Warning, float Damage, float Active, float Yaw, float Delay)
{
    if (!Source || !Source->GetWorld()) return nullptr;
    int32 Count = 0;
    for (TActorIterator<AEncounterHazard> It(Source->GetWorld()); It; ++It) if (++Count >= 36) return nullptr;
    FHitResult Ground;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(EncounterWarningGround), false, Source);
    if (Source->GetWorld()->LineTraceSingleByChannel(Ground, Center + FVector(0,0,600), Center - FVector(0,0,1600), ECC_GameTraceChannel2, Query))
        Center.Z = Ground.ImpactPoint.Z + 4;
    FActorSpawnParameters Params; Params.Owner = Source;
    auto* Hazard = Source->GetWorld()->SpawnActor<AEncounterHazard>(Center, FRotator(0,Yaw,0), Params);
    if (Hazard) Hazard->Initialize(Source, Shape, Size, Width, Warning, Damage, Active, Delay);
    return Hazard;
}

void AEncounterHazard::Initialize(AActor* Source, EEncounterShape Shape, float InSize, float InWidth,
    float InWarning, float InDamage, float InActive, float InDelay)
{
    DamageSource = Source; Footprint = Shape; Size = FMath::Max(10.f,InSize); Width = FMath::Max(0.f,InWidth);
    Warning = FMath::Max(.6f,InWarning); Damage = FMath::Max(0.f,InDamage);
    ActiveTime = FMath::Max(.1f,InActive); Delay = FMath::Max(0.f,InDelay); Age = 0; NextDamage = 0; bImpacted = false;
    Visual->DecalSize=Shape == EEncounterShape::Lane ? FVector(180,Width*.5f,Size*.5f) : FVector(180,Size,Size);
    auto* Base = LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/HeavensDivide/Materials/M_EncounterWarning"));
    Material = Base ? UMaterialInstanceDynamic::Create(Base,this) : nullptr;
    if (Material)
    {
        Visual->SetDecalMaterial(Material);
        Material->SetScalarParameterValue(TEXT("Shape"),float(Shape));
        Material->SetScalarParameterValue(TEXT("Inner"),Width/Size);
        Material->SetScalarParameterValue(TEXT("Fill"),0);
        Material->SetScalarParameterValue(TEXT("Impact"),0);
    }
    SetLifeSpan(Delay+Warning+ActiveTime+1);
    Visual->SetVisibility(Delay<=0);
}

bool AEncounterHazard::ContainsPoint(FVector Point, float Padding) const
{
    if (FMath::Abs(Point.Z-GetActorLocation().Z)>240) return false;
    const FVector Local = GetActorRotation().UnrotateVector(Point-GetActorLocation());
    if (Footprint == EEncounterShape::Lane)
        return FMath::Abs(Local.X)<=Size*.5f+Padding && FMath::Abs(Local.Y)<=Width*.5f+Padding;
    const float R = Local.Size2D();
    return R <= Size+Padding && (Footprint != EEncounterShape::Ring || R >= FMath::Max(0.f,Width-Padding));
}

void AEncounterHazard::Tick(float Delta)
{
    Super::Tick(Delta);
    if (!DamageSource.IsValid() || (Cast<AEnemyBase>(DamageSource.Get()) && Cast<AEnemyBase>(DamageSource.Get())->IsDead())) { Destroy(); return; }
    Age += FMath::Max(0.f,Delta);
    Visual->SetVisibility(Age >= Delay);
    const float Time = Age-Delay;
    if (Material) Material->SetScalarParameterValue(TEXT("Fill"),FMath::Clamp(Time/Warning,0.f,1.f));
    if (Time < Warning) return;
    if (!bImpacted)
    {
        bImpacted = true;
        if (Material) Material->SetScalarParameterValue(TEXT("Impact"),1);
        if (Damage>0) UCombatAudioLibrary::PlayEvent(this,TEXT("BossGroundHit"),GetActorLocation(),false,.45f);
    }
    // One strike for bursts; persistent fields tick slowly and never catch up missed damage after a hitch.
    if (Time >= NextDamage)
    {
        if (auto* PC = Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(this,0)))
            if (auto* Player = Cast<ACharacterBase>(PC->GetPawn()))
                if (ContainsPoint(Player->GetActorLocation(),Player->GetCapsuleComponent()->GetScaledCapsuleRadius()*.5f))
                    PC->ApplyDamageToPlayer(Damage);
        NextDamage = Time + .8f;
    }
    if (Time >= Warning+ActiveTime) Destroy();
}
