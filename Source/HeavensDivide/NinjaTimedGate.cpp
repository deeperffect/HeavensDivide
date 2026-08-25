#include "NinjaTimedGate.h"
#include "CharacterBase.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ANinjaTimedGate::ANinjaTimedGate()
{
	PrimaryActorTick.bCanEverTick=true;
	PrimaryActorTick.bStartWithTickEnabled=false;
	HazardBox=CreateDefaultSubobject<UBoxComponent>(TEXT("HazardBox"));
	HazardBox->SetupAttachment(SceneRoot);
	HazardBox->SetBoxExtent(FVector(650,75,150));
	HazardBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	HazardBox->SetCollisionResponseToChannel(ECC_Pawn,ECR_Overlap);
	BarrierVisual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BarrierVisual"));
	BarrierVisual->SetupAttachment(HazardBox);
	BarrierVisual->SetRelativeScale3D(FVector(13,1.5f,3));
	BarrierVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Red(TEXT("/Game/HeavensDivide/Materials/M_SamuraiLaneIndicator.M_SamuraiLaneIndicator"));
	if(Cube.Succeeded())BarrierVisual->SetStaticMesh(Cube.Object);
	if(Red.Succeeded())BarrierVisual->SetMaterial(0,Red.Object);
	HazardBox->OnComponentBeginOverlap.AddDynamic(this,&ANinjaTimedGate::HandleGateOverlap);
}

void ANinjaTimedGate::BeginPlay()
{
	Super::BeginPlay();
	if(UMaterialInstanceDynamic* Material=BarrierVisual->CreateAndSetMaterialInstanceDynamic(0))
	{
		Material->SetScalarParameterValue(TEXT("FillAmount"),1.0f);
		Material->SetVectorParameterValue(TEXT("FillColor"),FLinearColor::Red);
	}
}

void ANinjaTimedGate::SetTrapActive(bool bActive)
{
	Super::SetTrapActive(bActive);
	SetActorTickEnabled(bActive);
	HazardBox->SetCollisionEnabled(bActive?ECollisionEnabled::QueryOnly:ECollisionEnabled::NoCollision);
	BarrierVisual->SetVisibility(bActive);
}

void ANinjaTimedGate::ResetTrap()
{
	StateElapsed=0.0f;bHitThisClosure=false;
	GateState=bTrapActive?ENinjaGateState::InitialDelay:ENinjaGateState::Inactive;
	SetGateAlpha(0.0f);
}

void ANinjaTimedGate::SetGateAlpha(float ClosedAlpha)
{
	HazardBox->SetRelativeLocation(FMath::Lerp(OpenOffset,FVector::ZeroVector,FMath::Clamp(ClosedAlpha,0.0f,1.0f)));
}

void ANinjaTimedGate::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);if(!bTrapActive)return;StateElapsed+=DeltaSeconds;
	if(GateState==ENinjaGateState::InitialDelay&&StateElapsed>=InitialDelay){GateState=ENinjaGateState::Open;StateElapsed=0;OnGateOpened.Broadcast();return;}
	if(GateState==ENinjaGateState::Open&&StateElapsed>=OpenDuration){GateState=ENinjaGateState::Closing;StateElapsed=0;bHitThisClosure=false;OnGateClosing.Broadcast();return;}
	if(GateState==ENinjaGateState::Closing)
	{
		const float A=FMath::Clamp(StateElapsed/FMath::Max(.01f,TransitionDuration),0.0f,1.0f);SetGateAlpha(A);
		if(A>=1)
		{
			GateState=ENinjaGateState::Closed;StateElapsed=0;OnGateClosed.Broadcast();
			TArray<AActor*> Overlapping;
			HazardBox->GetOverlappingActors(Overlapping,ACharacterBase::StaticClass());
			if(!bHitThisClosure&&Overlapping.Num()>0)bHitThisClosure=DamageTrialPlayer();
		}
		return;
	}
	if(GateState==ENinjaGateState::Closed&&StateElapsed>=ClosedDuration){GateState=ENinjaGateState::Opening;StateElapsed=0;return;}
	if(GateState==ENinjaGateState::Opening)
	{
		const float A=FMath::Clamp(StateElapsed/FMath::Max(.01f,TransitionDuration),0.0f,1.0f);SetGateAlpha(1.0f-A);
		if(A>=1){GateState=ENinjaGateState::Open;StateElapsed=0;OnGateOpened.Broadcast();}
	}
}

void ANinjaTimedGate::HandleGateOverlap(UPrimitiveComponent*,AActor* OtherActor,UPrimitiveComponent*,int32,bool,const FHitResult&)
{
	if(bHitThisClosure||!Cast<ACharacterBase>(OtherActor))return;
	if(GateState!=ENinjaGateState::Closing&&GateState!=ENinjaGateState::Closed)return;
	bHitThisClosure=DamageTrialPlayer();
}
