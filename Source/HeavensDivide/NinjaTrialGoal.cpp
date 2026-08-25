#include "NinjaTrialGoal.h"
#include "NinjaTechniqueTrial.h"
#include "CharacterBase.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
ANinjaTrialGoal::ANinjaTrialGoal()
{
	GoalTrigger=CreateDefaultSubobject<UBoxComponent>(TEXT("GoalTrigger"));SetRootComponent(GoalTrigger);
	GoalTrigger->SetBoxExtent(FVector(500,150,200));GoalTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);GoalTrigger->SetCollisionResponseToChannel(ECC_Pawn,ECR_Overlap);
	GoalVisual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GoalVisual"));GoalVisual->SetupAttachment(GoalTrigger);GoalVisual->SetRelativeScale3D(FVector(2,2,4));GoalVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));if(Mesh.Succeeded())GoalVisual->SetStaticMesh(Mesh.Object);
	GoalTrigger->OnComponentBeginOverlap.AddDynamic(this,&ANinjaTrialGoal::HandleOverlap);
}
void ANinjaTrialGoal::HandleOverlap(UPrimitiveComponent*,AActor* Other,UPrimitiveComponent*,int32,bool,const FHitResult&)
{
	if(bCompleted||!OwningTrial||!OwningTrial->IsTrialRunning()||!Cast<ACharacterBase>(Other))return;
	bCompleted=true;OnGoalReached.Broadcast();OwningTrial->CompleteCourse();
}
