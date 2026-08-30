#include "NinjaFloorTrap.h"
#include "CharacterBase.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ANinjaFloorTrap::ANinjaFloorTrap()
{
	PrimaryActorTick.bCanEverTick = true; PrimaryActorTick.bStartWithTickEnabled = false;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")); SetRootComponent(Root);
	HazardArea = CreateDefaultSubobject<UBoxComponent>(TEXT("HazardArea")); HazardArea->SetupAttachment(Root);
	HazardArea->SetBoxExtent(FVector(300,300,100)); HazardArea->SetGenerateOverlapEvents(true);
	HazardArea->SetCollisionEnabled(ECollisionEnabled::NoCollision); HazardArea->SetCollisionResponseToAllChannels(ECR_Ignore); HazardArea->SetCollisionResponseToChannel(ECC_Pawn,ECR_Overlap);
	TelegraphVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TelegraphVisual")); TelegraphVisual->SetupAttachment(Root);
	TelegraphVisual->SetRelativeLocation(FVector(0,0,3)); TelegraphVisual->SetRelativeScale3D(FVector(6,6,.03f)); TelegraphVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision); TelegraphVisual->SetVisibility(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube")); if(Cube.Succeeded()) TelegraphVisual->SetStaticMesh(Cube.Object);
}

void ANinjaFloorTrap::BeginPlay(){ Super::BeginPlay(); TelegraphMaterial=TelegraphVisual->CreateAndSetMaterialInstanceDynamic(0); }
void ANinjaFloorTrap::HandleActivationChanged(bool bActive)
{
	StateElapsed=0; FloorState=bActive?ENinjaFloorTrapState::Waiting:ENinjaFloorTrapState::Inactive;
	SetActorTickEnabled(bActive); HazardArea->SetCollisionEnabled(bActive?ECollisionEnabled::QueryOnly:ECollisionEnabled::NoCollision); TelegraphVisual->SetVisibility(false);
	if(TelegraphMaterial)TelegraphMaterial->SetScalarParameterValue(TEXT("FillAmount"),0);
}
void ANinjaFloorTrap::BeginTelegraph(){FloorState=ENinjaFloorTrapState::Telegraphing;StateElapsed=0;TelegraphVisual->SetVisibility(true);OnTelegraphStarted.Broadcast();}
void ANinjaFloorTrap::ResolveDamage()
{
	TArray<AActor*> Actors; HazardArea->GetOverlappingActors(Actors,ACharacterBase::StaticClass());
	for(AActor* Actor:Actors)if(DamageTrialPlayer(Actor))break;
	FloorState=ENinjaFloorTrapState::Active;StateElapsed=0;OnDamageResolved.Broadcast();
}
void ANinjaFloorTrap::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);if(!bTrapActive)return;StateElapsed+=DeltaSeconds;
	if(FloorState==ENinjaFloorTrapState::Waiting&&StateElapsed>=InitialDelay){BeginTelegraph();return;}
	if(FloorState==ENinjaFloorTrapState::Telegraphing){const float A=FMath::Clamp(StateElapsed/FMath::Max(.01f,TelegraphDuration),0.f,1.f);if(TelegraphMaterial)TelegraphMaterial->SetScalarParameterValue(TEXT("FillAmount"),A);if(A>=1)ResolveDamage();return;}
	if(FloorState==ENinjaFloorTrapState::Active&&StateElapsed>=ActiveDuration){FloorState=ENinjaFloorTrapState::Cooldown;StateElapsed=0;TelegraphVisual->SetVisibility(false);return;}
	if(FloorState==ENinjaFloorTrapState::Cooldown&&StateElapsed>=CooldownDuration){if(bLooping)BeginTelegraph();else DeactivateTrap();}
}
