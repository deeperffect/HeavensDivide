#include "NinjaFloorTrap.h"
#include "CharacterBase.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ANinjaFloorTrap::ANinjaFloorTrap()
{
	PrimaryActorTick.bCanEverTick=true;
	PrimaryActorTick.bStartWithTickEnabled=false;
	HazardArea=CreateDefaultSubobject<UBoxComponent>(TEXT("HazardArea"));
	HazardArea->SetupAttachment(SceneRoot);
	HazardArea->SetBoxExtent(FVector(300,300,100));
	HazardArea->SetCollisionResponseToAllChannels(ECR_Ignore);
	HazardArea->SetCollisionResponseToChannel(ECC_Pawn,ECR_Overlap);
	TelegraphVisual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TelegraphVisual"));
	TelegraphVisual->SetupAttachment(SceneRoot);
	TelegraphVisual->SetRelativeLocation(FVector(0,0,3));
	TelegraphVisual->SetRelativeScale3D(FVector(6,6,.03f));
	TelegraphVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Game/HeavensDivide/Materials/M_SamuraiLaneIndicator.M_SamuraiLaneIndicator"));
	if(Cube.Succeeded())TelegraphVisual->SetStaticMesh(Cube.Object);
	if(Material.Succeeded())TelegraphVisual->SetMaterial(0,Material.Object);
}

void ANinjaFloorTrap::BeginPlay()
{
	Super::BeginPlay();
	TelegraphMaterial=TelegraphVisual->CreateAndSetMaterialInstanceDynamic(0);
	if(TelegraphMaterial)TelegraphMaterial->SetVectorParameterValue(TEXT("FillColor"),FLinearColor::Red);
}

void ANinjaFloorTrap::SetTrapActive(bool bActive)
{
	Super::SetTrapActive(bActive);
	SetActorTickEnabled(bActive);
	HazardArea->SetCollisionEnabled(bActive?ECollisionEnabled::QueryOnly:ECollisionEnabled::NoCollision);
}

void ANinjaFloorTrap::ResetTrap()
{
	StateElapsed=0.0f;
	FloorState=bTrapActive?ENinjaFloorTrapState::Waiting:ENinjaFloorTrapState::Inactive;
	TelegraphVisual->SetVisibility(false);
	if(TelegraphMaterial)TelegraphMaterial->SetScalarParameterValue(TEXT("FillAmount"),0.0f);
}

void ANinjaFloorTrap::BeginTelegraph()
{
	FloorState=ENinjaFloorTrapState::Telegraphing;StateElapsed=0.0f;
	TelegraphVisual->SetVisibility(true);
	if(TelegraphMaterial)TelegraphMaterial->SetScalarParameterValue(TEXT("FillAmount"),0.0f);
	OnTelegraphStarted.Broadcast();
}

void ANinjaFloorTrap::ResolveDamage()
{
	TArray<AActor*> Overlapping;
	HazardArea->GetOverlappingActors(Overlapping,ACharacterBase::StaticClass());
	if(Overlapping.Num()>0)DamageTrialPlayer();
	FloorState=ENinjaFloorTrapState::Active;StateElapsed=0.0f;
	OnDamageResolved.Broadcast();
}

void ANinjaFloorTrap::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if(!bTrapActive)return;
	StateElapsed+=DeltaSeconds;
	if(FloorState==ENinjaFloorTrapState::Waiting&&StateElapsed>=InitialDelay){BeginTelegraph();return;}
	if(FloorState==ENinjaFloorTrapState::Telegraphing)
	{
		const float Alpha=FMath::Clamp(StateElapsed/FMath::Max(0.01f,TelegraphDuration),0.0f,1.0f);
		if(TelegraphMaterial)TelegraphMaterial->SetScalarParameterValue(TEXT("FillAmount"),Alpha);
		if(Alpha>=1.0f)ResolveDamage();
		return;
	}
	if(FloorState==ENinjaFloorTrapState::Active&&StateElapsed>=ActiveDuration)
	{
		FloorState=ENinjaFloorTrapState::Cooldown;StateElapsed=0.0f;TelegraphVisual->SetVisibility(false);return;
	}
	if(FloorState==ENinjaFloorTrapState::Cooldown&&StateElapsed>=CooldownDuration)
	{
		OnTrapReset.Broadcast();
		if(bLooping)BeginTelegraph();else SetTrapActive(false);
	}
}
