#include "NinjaTimedGate.h"
#include "CharacterBase.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
ANinjaTimedGate::ANinjaTimedGate()
{
	PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.bStartWithTickEnabled=false;
	USceneComponent* Root=CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));SetRootComponent(Root);
	HazardBox=CreateDefaultSubobject<UBoxComponent>(TEXT("HazardBox"));HazardBox->SetupAttachment(Root);HazardBox->SetBoxExtent(FVector(650,75,150));HazardBox->SetGenerateOverlapEvents(true);HazardBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);HazardBox->SetCollisionResponseToAllChannels(ECR_Ignore);HazardBox->SetCollisionResponseToChannel(ECC_Pawn,ECR_Overlap);
	BarrierVisual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BarrierVisual"));BarrierVisual->SetupAttachment(HazardBox);BarrierVisual->SetRelativeScale3D(FVector(13,1.5f,3));BarrierVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);BarrierVisual->SetVisibility(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));if(Cube.Succeeded())BarrierVisual->SetStaticMesh(Cube.Object);
	HazardBox->OnComponentBeginOverlap.AddDynamic(this,&ANinjaTimedGate::HandleGateOverlap);
}
void ANinjaTimedGate::HandleActivationChanged(bool bActive){StateElapsed=0;bHitThisClosure=false;GateState=bActive?ENinjaGateState::InitialDelay:ENinjaGateState::Inactive;SetGateAlpha(0);SetActorTickEnabled(bActive);HazardBox->SetCollisionEnabled(bActive?ECollisionEnabled::QueryOnly:ECollisionEnabled::NoCollision);BarrierVisual->SetVisibility(bActive);}
void ANinjaTimedGate::SetGateAlpha(float A){HazardBox->SetRelativeLocation(FMath::Lerp(OpenOffset,FVector::ZeroVector,FMath::Clamp(A,0.f,1.f)));}
void ANinjaTimedGate::Tick(float D)
{
	Super::Tick(D);if(!bTrapActive)return;StateElapsed+=D;
	if(GateState==ENinjaGateState::InitialDelay&&StateElapsed>=InitialDelay){GateState=ENinjaGateState::Open;StateElapsed=0;OnGateOpened.Broadcast();return;}
	if(GateState==ENinjaGateState::Open&&StateElapsed>=OpenDuration){GateState=ENinjaGateState::Closing;StateElapsed=0;bHitThisClosure=false;OnGateClosing.Broadcast();return;}
	if(GateState==ENinjaGateState::Closing){const float A=FMath::Clamp(StateElapsed/FMath::Max(.01f,TransitionDuration),0.f,1.f);SetGateAlpha(A);if(A>=1){GateState=ENinjaGateState::Closed;StateElapsed=0;OnGateClosed.Broadcast();TArray<AActor*>Actors;HazardBox->GetOverlappingActors(Actors,ACharacterBase::StaticClass());for(AActor* Actor:Actors)if(!bHitThisClosure&&DamageTrialPlayer(Actor))bHitThisClosure=true;}return;}
	if(GateState==ENinjaGateState::Closed&&StateElapsed>=ClosedDuration){GateState=ENinjaGateState::Opening;StateElapsed=0;return;}
	if(GateState==ENinjaGateState::Opening){const float A=FMath::Clamp(StateElapsed/FMath::Max(.01f,TransitionDuration),0.f,1.f);SetGateAlpha(1-A);if(A>=1){GateState=ENinjaGateState::Open;StateElapsed=0;OnGateOpened.Broadcast();}}
}
void ANinjaTimedGate::HandleGateOverlap(UPrimitiveComponent*,AActor* Other,UPrimitiveComponent*,int32,bool,const FHitResult&){if(!bHitThisClosure&&(GateState==ENinjaGateState::Closing||GateState==ENinjaGateState::Closed))bHitThisClosure=DamageTrialPlayer(Other);}
