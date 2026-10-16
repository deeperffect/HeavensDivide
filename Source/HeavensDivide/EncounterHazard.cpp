#include "EncounterHazard.h"
#include "IndicatorAttackVFX.h"
#include "EnemyBase.h"
#include "CharacterBase.h"
#include "SurvivorPlayerController.h"
#include "Components/DecalComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "EngineUtils.h"
#include "CombatAudio.h"
#include "BloodshiftBalanceData.h"
#include "CombatGround.h"
#include "CinderOracleBoss.h"

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
    // Surface warnings remain visible on meshes/landscapes that reject decals.
    Visual->SetVisibility(false);
    GroundSurface=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WarningSurface"));
    GroundSurface->SetupAttachment(RootComponent);
    ImpactColumn=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ImpactColumn"));
    ImpactColumn->SetupAttachment(RootComponent);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Surface(TEXT("/Game/HeavensDivide/Materials/M_AttackIndicatorCircle"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Lane(TEXT("/Game/HeavensDivide/Materials/M_AttackIndicatorRectangle"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Energy(TEXT("/Game/HeavensDivide/Materials/M_EncounterEnergy"));
    SurfaceTemplate=Surface.Object; LaneTemplate=Lane.Object; ImpactTemplate=Energy.Object;
    GroundSurface->SetStaticMesh(Plane.Object); ImpactColumn->SetStaticMesh(Cylinder.Object);
    for(auto* Mesh : {GroundSurface.Get(),ImpactColumn.Get()})
    { Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->SetCastShadow(false); Mesh->SetReceivesDecals(false); }
    ImpactColumn->SetVisibility(false);
}

AEncounterHazard* AEncounterHazard::Spawn(AActor* Source, FVector Center, EEncounterShape Shape, float Size,
    float Width, float Warning, float Damage, float Active, float Yaw, float Delay, bool bConstrainToArena)
{
    if (!Source || !Source->GetWorld()) return nullptr;
    int32 Count = 0;
    for (TActorIterator<AEncounterHazard> It(Source->GetWorld()); It; ++It)
        if (!It->IsActorBeingDestroyed()) ++Count;
    // Screen-filling boss patterns reserve capacity for their guardian casts too.
    const ACinderOracleBoss* Boss = Cast<ACinderOracleBoss>(Source);
    if (!Boss) Boss = Cast<ACinderOracleBoss>(Source->GetOwner());
    const int32 Limit = Boss ? FMath::Max(UBloodshiftBalanceData::Get()->MaxActiveHazards, Boss->MaxActiveSpellHazards)
                            : UBloodshiftBalanceData::Get()->MaxActiveHazards;
    if (Count >= FMath::Clamp(Limit,1,256)) return nullptr;
    const float Footprint=Shape==EEncounterShape::Lane ? .5f*FMath::Sqrt(Size*Size+Width*Width) : Size;
    if (bConstrainToArena)
    {
        Center=CombatGround::ClampInsideArena(Source->GetWorld(),Center,Footprint);
        if (!CombatGround::IsInsideArena(Source->GetWorld(),Center,Footprint)) return nullptr;
    }
    FVector Ground;
    if (CombatGround::FindSurface(Source->GetWorld(),Center,Source,Ground))
        Center.Z = Ground.Z + 4;
    FActorSpawnParameters Params; Params.Owner = Source;
    auto* Hazard = Source->GetWorld()->SpawnActor<AEncounterHazard>(Center, FRotator(0,Yaw,0), Params);
    if (Hazard)
    {
        Hazard->Initialize(Source, Shape, Size, Width, Warning, Damage, Active, Delay);
        if (!Hazard->HasVisibleWarning()) { Hazard->Destroy(); return nullptr; }
    }
    return Hazard;
}

void AEncounterHazard::Initialize(AActor* Source, EEncounterShape Shape, float InSize, float InWidth,
    float InWarning, float InDamage, float InActive, float InDelay)
{
    DamageSource = Source; Footprint = Shape; Size = FMath::Max(10.f,InSize); Width = FMath::Max(0.f,InWidth);
    Warning = FMath::Max(FMath::Max(.01f,UBloodshiftBalanceData::Get()->MinimumWarning),InWarning); Damage = FMath::Max(0.f,InDamage);
    ActiveTime = FMath::Max(.1f,InActive); Delay = FMath::Max(0.f,InDelay); Age = 0; NextDamage = 0; bImpacted = false;
    Visual->DecalSize=Shape == EEncounterShape::Lane ? FVector(180,Width*.5f,Size*.5f) : FVector(180,Size,Size);
    UMaterialInterface* Template = Shape==EEncounterShape::Lane ? LaneTemplate.Get() : SurfaceTemplate.Get();
    Material = Template ? UMaterialInstanceDynamic::Create(Template,this) : nullptr;
    if (Material)
    {
        GroundSurface->SetMaterial(0,Material);
        Material->SetScalarParameterValue(TEXT("InnerRadius"),Shape==EEncounterShape::Ring ? Width/Size : 0.f);
        Material->SetScalarParameterValue(TEXT("LaneAspect"),Width/Size);
        Material->SetScalarParameterValue(TEXT("FillAmount"),0);
        Material->SetScalarParameterValue(TEXT("Impact"),0);
        Material->SetScalarParameterValue(TEXT("Fade"),1);
        // Inherit the elite materials' authored colors, fill and outline ripple.
    }
    GroundSurface->SetWorldScale3D(Shape==EEncounterShape::Lane ? FVector(Size/100,Width/100,1) : FVector(Size/50,Size/50,1));
    ImpactMaterial=ImpactTemplate ? UMaterialInstanceDynamic::Create(ImpactTemplate,this) : nullptr;
    if (ImpactMaterial)
    {
        ImpactMaterial->SetVectorParameterValue(TEXT("Tint"),Material ? Material->K2_GetVectorParameterValue(TEXT("FillColor")) : FLinearColor::Red);
        ImpactColumn->SetMaterial(0,ImpactMaterial);
    }
    if (Shape==EEncounterShape::Lane)
        ImpactColumn->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    SetLifeSpan(Delay+Warning+ActiveTime+1);
    GroundSurface->SetVisibility(Delay<=0);
}

bool AEncounterHazard::HasVisibleWarning() const
{ return Material && GroundSurface && GroundSurface->GetStaticMesh(); }

void AEncounterHazard::ActivateAsTrail()
{
    Warning=0; Delay=0; bImpacted=true; ActiveTime=3.f;
    if(Material) { Material->SetScalarParameterValue(TEXT("Impact"),1); Material->SetScalarParameterValue(TEXT("FillAmount"),1); }
    ImpactColumn->SetVisibility(false); SetLifeSpan(3.f);
}

bool AEncounterHazard::ContainsPoint(FVector Point, float Padding) const
{
    if (FMath::Abs(Point.Z-GetActorLocation().Z)>UBloodshiftBalanceData::Get()->HazardVerticalTolerance) return false;
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
    if (auto* Enemy=Cast<AEnemyBase>(DamageSource.Get())) if(Enemy->IsGameplaySuspended()) { Destroy(); return; }
    Age += FMath::Max(0.f,Delta);
    GroundSurface->SetVisibility(Age >= Delay);
    const float Time = Age-Delay;
    if (Material) Material->SetScalarParameterValue(TEXT("FillAmount"),Warning>0?FMath::Clamp(Time/Warning,0.f,1.f):1.f);
    if (Time < Warning || !HasVisibleWarning()) return;
    // An attack whose entire active window elapsed during a hitch must not hit
    // on a frame in which the player never saw its impact.
    if (!bImpacted && Time >= Warning+ActiveTime) { Destroy(); return; }
    if (!bImpacted)
    {
        bImpacted = true;
        if (Material) Material->SetScalarParameterValue(TEXT("Impact"),1);
        // A solid cylinder would incorrectly cover a ring attack's safe center.
        auto* FX = Damage > 0 ? AIndicatorAttackVFX::Spawn(DamageSource.Get(), GetActorLocation(), Footprint,
            Size, Width, GetActorRotation().Yaw, AIndicatorAttackVFX::StyleFor(DamageSource.Get())) : nullptr;
        ImpactColumn->SetVisibility(!FX && Damage>0 && ImpactMaterial && Footprint!=EEncounterShape::Ring);
        if (Damage>0) UCombatAudioLibrary::PlayEvent(this,TEXT("BossGroundHit"),GetActorLocation(),false,.45f);
    }
    // One strike for bursts; persistent fields tick slowly and never catch up missed damage after a hitch.
    ImpactAge=Time-Warning;
    const float Tail=FMath::Max(ActiveTime,.55f);
    const float Fade=1-FMath::SmoothStep(FMath::Max(0.f,Tail-.3f),Tail,ImpactAge);
    if (Material) Material->SetScalarParameterValue(TEXT("Fade"),Fade);
    if (ImpactMaterial) ImpactMaterial->SetScalarParameterValue(TEXT("Fade"),Fade);
    const float Height=FMath::Lerp(150.f,25.f,FMath::Clamp(ImpactAge/.5f,0.f,1.f));
    ImpactColumn->SetRelativeLocation(FVector(0,0,Height*.5f));
    ImpactColumn->SetWorldScale3D(Footprint==EEncounterShape::Lane ? FVector(Size/100,Width/100,Height/100) : FVector(Size/50,Size/50,Height/100));
    if (Damage>0 && Time < Warning+ActiveTime && Time >= NextDamage)
    {
        if (auto* PC = Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(this,0)))
            if (auto* Player = Cast<ACharacterBase>(PC->GetPawn()))
                if (ContainsPoint(Player->GetActorLocation(),Player->GetCapsuleComponent()->GetScaledCapsuleRadius()*FMath::Clamp(UBloodshiftBalanceData::Get()->PlayerCollisionPaddingFraction,0.f,1.f)))
                    PC->ApplyDamageToPlayer(Damage);
        NextDamage = Time + FMath::Max(.01f,UBloodshiftBalanceData::Get()->PersistentDamageInterval);
    }
    if (ImpactAge >= Tail) Destroy();
}
