#include "NinjaTrialGoal.h"

#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "NinjaTechniqueTrial.h"
#include "UObject/ConstructorHelpers.h"

ANinjaTrialGoal::ANinjaTrialGoal()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	GoalVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GoalVisual"));
	GoalVisual->SetupAttachment(SceneRoot);
	GoalVisual->SetRelativeScale3D(FVector(1.0, 1.0, 2.0));
	GoalVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded()) GoalVisual->SetStaticMesh(CubeMesh.Object);

	GoalTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("GoalTrigger"));
	GoalTrigger->SetupAttachment(SceneRoot);
	GoalTrigger->SetSphereRadius(150.0f);
	GoalTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	GoalTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	GoalTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	GoalTrigger->OnComponentBeginOverlap.AddDynamic(this, &ANinjaTrialGoal::HandleGoalOverlap);
}

void ANinjaTrialGoal::InitializeForTrial(ANinjaTechniqueTrial* InOwningTrial)
{
	OwningTrial = InOwningTrial;
}

void ANinjaTrialGoal::HandleGoalOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (OwningTrial) OwningTrial->NotifyGoalReached(OtherActor);
}
