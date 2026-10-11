#include "MagnetPickup.h"
#include "ExperiencePickup.h"
#include "SurvivorPlayerController.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "EngineUtils.h"
#include "CombatAudio.h"
#include "MinimapMarkerComponent.h"

AMagnetPickup::AMagnetPickup()
{
    PrimaryActorTick.bCanEverTick=true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    MinimapMarker=CreateDefaultSubobject<UMinimapMarkerComponent>(TEXT("MinimapMarker"));
    MinimapMarker->MarkerType=EMinimapMarkerType::MagnetPickup;
    MinimapMarker->DisplayName=FText::FromString(TEXT("Magnet"));
    MinimapMarker->Priority=5;
    Symbol=CreateDefaultSubobject<USceneComponent>(TEXT("MagnetSymbol")); Symbol->SetupAttachment(RootComponent);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Energy(TEXT("/Game/HeavensDivide/Materials/M_EncounterEnergy"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> RingMat(TEXT("/Game/HeavensDivide/Materials/M_EnemyRoleRing"));
    EnergyMaterial=Energy.Object;
    for (int32 I=0;I<5;++I)
    {
        auto* Piece=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("MagnetPart%d"),I));
        Piece->SetupAttachment(Symbol); Piece->SetStaticMesh(Cube.Object); Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision); Piece->SetCastShadow(false);
        Piece->SetRelativeLocation(I==2 ? FVector(0,-18,0) : FVector(I%2==0?-16:16,I<2?0:20,0));
        Piece->SetRelativeScale3D(I==2 ? FVector(.42f,.12f,.14f) : I<2 ? FVector(.12f,.35f,.14f) : FVector(.12f,.10f,.14f));
        Pieces.Add(Piece);
    }
    Ring=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MagnetHalo")); Ring->SetupAttachment(RootComponent);
    Ring->SetStaticMesh(Plane.Object); Ring->SetMaterial(0,RingMat.Object); Ring->SetRelativeScale3D(FVector(1.25f));
    Ring->SetCollisionEnabled(ECollisionEnabled::NoCollision); Ring->SetCastShadow(false);
}
void AMagnetPickup::BeginPlay()
{
    Super::BeginPlay(); SetLifeSpan(90);
    for (int32 I=0;I<Pieces.Num();++I)
        if (EnergyMaterial)
        {
            auto* MID=UMaterialInstanceDynamic::Create(EnergyMaterial,this);
            MID->SetVectorParameterValue(TEXT("Tint"),I<3 ? FLinearColor(.03f,.7f,1) : FLinearColor(.7f,1,1));
            MID->SetScalarParameterValue(TEXT("Solid"),1); Pieces[I]->SetMaterial(0,MID);
        }
    if (auto* MID=Ring->CreateAndSetMaterialInstanceDynamic(0)) MID->SetVectorParameterValue(TEXT("Tint"),FLinearColor(.05f,.8f,1));
}
void AMagnetPickup::Tick(float Delta)
{
    Super::Tick(Delta); if (bConsumed) return;
    Age+=Delta; Symbol->SetRelativeLocation(FVector(0,0,12+FMath::Sin(Age*3)*7));
    Symbol->SetRelativeRotation(FRotator(0,Age*55,0));
    auto* PC=Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(this,0));
    if (!PC || !PC->GetPawn() || PC->IsPlayerDead()) return;
    const FVector D=PC->GetPawn()->GetActorLocation()-GetActorLocation();
    if (D.SizeSquared2D()<=FMath::Square(CollectRadius) && FMath::Abs(D.Z)<180) Collect(PC);
}
int32 AMagnetPickup::Collect(ASurvivorPlayerController* Player)
{
    if (bConsumed || !Player || !Player->GetPawn() || Player->IsPlayerDead()) return 0;
    bConsumed=true; int32 Count=0;
    for (TActorIterator<AExperiencePickup> It(GetWorld());It;++It) Count+=It->AttractByMagnet()?1:0;
    UCombatAudioLibrary::PlayEvent(this,TEXT("FireballCast"),GetActorLocation(),false,.65f);
    Destroy(); return Count;
}
