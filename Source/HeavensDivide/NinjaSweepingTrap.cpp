#include "NinjaSweepingTrap.h"
#include "CharacterBase.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ANinjaSweepingTrap::ANinjaSweepingTrap()
{
	PrimaryActorTick.bCanEverTick=true;
	PrimaryActorTick.bStartWithTickEnabled=false;
	HazardBox=CreateDefaultSubobject<UBoxComponent>(TEXT("HazardBox"));
	HazardBox->SetupAttachment(SceneRoot);
	HazardBox->SetBoxExtent(FVector(60,500,100));
	HazardBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	HazardBox->SetCollisionResponseToChannel(ECC_Pawn,ECR_Overlap);
	Visual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(HazardBox);
	Visual->SetRelativeScale3D(FVector(1.2f,10.0f,2.0f));
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Red(TEXT("/Game/HeavensDivide/Materials/M_SamuraiLaneIndicator.M_SamuraiLaneIndicator"));
	if(Cube.Succeeded())Visual->SetStaticMesh(Cube.Object);
	if(Red.Succeeded())Visual->SetMaterial(0,Red.Object);
	HazardBox->OnComponentBeginOverlap.AddDynamic(this,&ANinjaSweepingTrap::HandleHazardOverlap);
}

void ANinjaSweepingTrap::BeginPlay()
{
	Super::BeginPlay();
	if(UMaterialInstanceDynamic* Material=Visual->CreateAndSetMaterialInstanceDynamic(0))
	{
		Material->SetScalarParameterValue(TEXT("FillAmount"),1.0f);
		Material->SetVectorParameterValue(TEXT("FillColor"),FLinearColor::Red);
	}
}

void ANinjaSweepingTrap::SetTrapActive(bool bActive)
{
	Super::SetTrapActive(bActive);
	SetActorTickEnabled(bActive);
	HazardBox->SetCollisionEnabled(bActive?ECollisionEnabled::QueryOnly:ECollisionEnabled::NoCollision);
	Visual->SetVisibility(bActive);
}

void ANinjaSweepingTrap::ResetTrap()
{
	if(StartLocation.IsNearlyZero())StartLocation=GetActorLocation();
	SetActorLocation(StartLocation);
	PhaseElapsed=0.0f;
	bSweeping=false;
	bHitThisSweep=false;
	bWaitingForFirstSweep=true;
}

void ANinjaSweepingTrap::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if(!bTrapActive)return;
	PhaseElapsed+=DeltaSeconds;
	if(!bSweeping)
	{
		const float WaitDuration=bWaitingForFirstSweep?InitialDelay:DelayBetweenSweeps;
		if(PhaseElapsed<WaitDuration)return;
		PhaseElapsed=0.0f;bSweeping=true;bHitThisSweep=false;bWaitingForFirstSweep=false;OnSweepStarted.Broadcast();
	}
	const float Alpha=FMath::Clamp(PhaseElapsed/FMath::Max(0.01f,SweepDuration),0.0f,1.0f);
	SetActorLocation(StartLocation+MovementDirection.GetSafeNormal()*MovementDistance*Alpha);
	if(Alpha>=1.0f)
	{
		bSweeping=false;PhaseElapsed=0.0f;SetActorLocation(StartLocation);OnSweepReset.Broadcast();
		if(!bLooping)SetTrapActive(false);
	}
}

void ANinjaSweepingTrap::HandleHazardOverlap(UPrimitiveComponent*,AActor* OtherActor,UPrimitiveComponent*,int32,bool,const FHitResult&)
{
	if(!bSweeping||bHitThisSweep||!Cast<ACharacterBase>(OtherActor))return;
	bHitThisSweep=DamageTrialPlayer();
}
