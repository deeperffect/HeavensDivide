#include "NinjaSweepingTrap.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
ANinjaSweepingTrap::ANinjaSweepingTrap()
{
	PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.bStartWithTickEnabled=false;
	USceneComponent* Root=CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));SetRootComponent(Root);
	HazardBox=CreateDefaultSubobject<UBoxComponent>(TEXT("HazardBox"));HazardBox->SetupAttachment(Root);HazardBox->SetBoxExtent(FVector(60,500,100));HazardBox->SetGenerateOverlapEvents(true);HazardBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);HazardBox->SetCollisionResponseToAllChannels(ECR_Ignore);HazardBox->SetCollisionResponseToChannel(ECC_Pawn,ECR_Overlap);
	Visual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));Visual->SetupAttachment(HazardBox);Visual->SetRelativeScale3D(FVector(1.2f,10,2));Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);Visual->SetVisibility(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));if(Cube.Succeeded())Visual->SetStaticMesh(Cube.Object);
	HazardBox->OnComponentBeginOverlap.AddDynamic(this,&ANinjaSweepingTrap::HandleHazardOverlap);
}
void ANinjaSweepingTrap::BeginPlay(){Super::BeginPlay();StartHazardRelativeLocation=HazardBox->GetRelativeLocation();}
void ANinjaSweepingTrap::HandleActivationChanged(bool bActive)
{
	HazardBox->SetRelativeLocation(StartHazardRelativeLocation);PhaseElapsed=0;bSweeping=false;bHitThisSweep=false;bWaitingForFirstSweep=true;
	SetActorTickEnabled(bActive);HazardBox->SetCollisionEnabled(bActive?ECollisionEnabled::QueryOnly:ECollisionEnabled::NoCollision);Visual->SetVisibility(bActive);
}
void ANinjaSweepingTrap::Tick(float D)
{
	Super::Tick(D);if(!bTrapActive)return;PhaseElapsed+=D;
	if(!bSweeping){const float Wait=bWaitingForFirstSweep?InitialDelay:DelayBetweenSweeps;if(PhaseElapsed<Wait)return;PhaseElapsed=0;bSweeping=true;bHitThisSweep=false;bWaitingForFirstSweep=false;OnSweepStarted.Broadcast();}
	const float A=FMath::Clamp(PhaseElapsed/FMath::Max(.01f,SweepDuration),0.f,1.f);HazardBox->SetRelativeLocation(StartHazardRelativeLocation+MovementDirection.GetSafeNormal()*MovementDistance*A);
	if(A>=1){bSweeping=false;PhaseElapsed=0;HazardBox->SetRelativeLocation(StartHazardRelativeLocation);OnSweepReset.Broadcast();if(!bLooping)DeactivateTrap();}
}
void ANinjaSweepingTrap::HandleHazardOverlap(UPrimitiveComponent*,AActor* Other,UPrimitiveComponent*,int32,bool,const FHitResult&){if(bSweeping&&!bHitThisSweep)bHitThisSweep=DamageTrialPlayer(Other);}
