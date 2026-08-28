#include "NinjaTechniqueTrial.h"
#include "NinjaTrialTrapBase.h"
#include "NinjaFloorTrap.h"
#include "NinjaSweepingTrap.h"
#include "NinjaTimedGate.h"
#include "NinjaTrialGoal.h"
#include "ObjectiveInteractionComponent.h"
#include "Components/StaticMeshComponent.h"
#include "HealthComponent.h"
#include "SurvivorPlayerController.h"

ANinjaTechniqueTrial::ANinjaTechniqueTrial()
{
	bForceSamuraiOnEntry=false;
	bLockSwappingDuringTrial=true;
	bSuspendAutoAttacksDuringTrial=true;
	ObjectiveInteraction->Title=FText::FromString(TEXT("Ninja Trial"));
	TrialArenaOffset=FVector(0.0f,75000.0f,0.0f);
	TrialPlayerOffset=FVector(0.0f,-3000.0f,150.0f);
	TrialFloor->SetRelativeLocation(TrialArenaOffset+FVector(0.0f,0.0f,-50.0f));
	TrialFloor->SetRelativeScale3D(FVector(8.0f,35.0f,1.0f));
	TrialFloor->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	TrialFloor->SetCollisionResponseToAllChannels(ECR_Block);
	FloorTrapClass=ANinjaFloorTrap::StaticClass();
	SweepingTrapClass=ANinjaSweepingTrap::StaticClass();
	TimedGateClass=ANinjaTimedGate::StaticClass();
	GoalClass=ANinjaTrialGoal::StaticClass();
	const FVector WallLocations[]={{0,3500,60},{0,-3500,60},{800,0,60},{-800,0,60}};
	const FVector WallScales[]={{8,.2,.6},{8,.2,.6},{.2,35,.6},{.2,35,.6}};
	for(int32 Index=0;Index<TrialWalls.Num();++Index)
	{
		TrialWalls[Index]->SetRelativeLocation(TrialArenaOffset+WallLocations[Index]);
		TrialWalls[Index]->SetRelativeScale3D(WallScales[Index]);
		TrialWalls[Index]->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		TrialWalls[Index]->SetCollisionResponseToAllChannels(ECR_Block);
	}
}

void ANinjaTechniqueTrial::BeginPlay()
{
	Super::BeginPlay();
	if(!IsArenaReady())return;
	const float FloorTop=TrialFloor->Bounds.Origin.Z+TrialFloor->Bounds.BoxExtent.Z;
	const FTransform ArenaTransform=GetArenaWorldTransform();
	auto SpawnTrap=[this,FloorTop,ArenaTransform](TSubclassOf<ANinjaTrialTrapBase> Class,const FVector& Offset)
	{
		if(!Class)return;
		FActorSpawnParameters Params;Params.Owner=this;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		FVector Location=ArenaTransform.TransformPositionNoScale(Offset);Location.Z+=FloorTop-GetTrialOrigin().Z;
		if(ANinjaTrialTrapBase* Trap=GetWorld()->SpawnActor<ANinjaTrialTrapBase>(Class,Location,ArenaTransform.Rotator(),Params)){Trap->InitializeForTrial(this);OwnedArenaActors.Add(Trap);}
	};
	SpawnTrap(FloorTrapClass,FVector(0,-1600,5));
	SpawnTrap(FloorTrapClass,FVector(0,-400,5));
	SpawnTrap(SweepingTrapClass,FVector(0,700,100));
	SpawnTrap(TimedGateClass,FVector(0,1800,150));
	if(GoalClass)
	{
		FActorSpawnParameters Params;Params.Owner=this;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		FVector GoalLocation=ArenaTransform.TransformPositionNoScale(FVector(0,3000,0));GoalLocation.Z=FloorTop+100;
		if(ANinjaTrialGoal* Goal=GetWorld()->SpawnActor<ANinjaTrialGoal>(GoalClass,GoalLocation,ArenaTransform.Rotator(),Params)){Goal->InitializeForTrial(this);OwnedArenaActors.Add(Goal);}
	}
	UE_LOG(LogTemp,Log,TEXT("[TrialRuntime] NinjaArena Trial=%s Traps=%d Goal=%s"),*GetName(),RegisteredTraps.Num(),OwnedArenaActors.Num()>RegisteredTraps.Num()?TEXT("OK"):TEXT("Missing"));
}

void ANinjaTechniqueTrial::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for(AActor* Actor:OwnedArenaActors)if(IsValid(Actor))Actor->Destroy();
	OwnedArenaActors.Reset();RegisteredTraps.Reset();
	Super::EndPlay(EndPlayReason);
}

void ANinjaTechniqueTrial::RegisterTrap(ANinjaTrialTrapBase* Trap)
{
	if(Trap)RegisteredTraps.AddUnique(Trap);
}

bool ANinjaTechniqueTrial::BeginChallenge()
{
	if(!PlayerController)return false;
	NinjaState=ENinjaTrialState::Entering;
	bCompletionHandled=false;
	PlayerController->OnNinjaTrialRewardCompleted.AddUniqueDynamic(this,&ANinjaTechniqueTrial::HandleRewardCompleted);
	NinjaState=ENinjaTrialState::Running;
	SetAllTrapsActive(true);
	return true;
}

bool ANinjaTechniqueTrial::PrepareActiveCharacter()
{
	return PlayerController&&PlayerController->ForceNinjaActive();
}

void ANinjaTechniqueTrial::StopChallenge()
{
	SetAllTrapsActive(false);
	if(PlayerController)PlayerController->OnNinjaTrialRewardCompleted.RemoveDynamic(this,&ANinjaTechniqueTrial::HandleRewardCompleted);
	if(NinjaState!=ENinjaTrialState::Completed&&NinjaState!=ENinjaTrialState::Reward&&NinjaState!=ENinjaTrialState::Returning)NinjaState=ENinjaTrialState::Failed;
}

void ANinjaTechniqueTrial::SetAllTrapsActive(bool bActive)
{
	for(ANinjaTrialTrapBase* Trap:RegisteredTraps)if(Trap)Trap->SetTrapActive(bActive);
}

bool ANinjaTechniqueTrial::ApplyTrialHazardDamage(float DamageAmount)
{
	if(!IsTrialRunning()||!PlayerController||PlayerController->IsPlayerDead())return false;
	UHealthComponent* Health=PlayerController->GetPlayerHealthComponent();
	if(!Health||Health->IsDead())return false;
	Health->ApplyDamage(DamageAmount);
	return true;
}

void ANinjaTechniqueTrial::CompleteCourse()
{
	if(!IsTrialRunning()||bCompletionHandled||!PlayerController||PlayerController->IsPlayerDead())return;
	bCompletionHandled=true;
	NinjaState=ENinjaTrialState::Reward;
	SetAllTrapsActive(false);
	PlayerController->RequestNinjaTrialUpgradeReward(RewardChoiceCount);
}

void ANinjaTechniqueTrial::HandleRewardCompleted()
{
	if(NinjaState!=ENinjaTrialState::Reward)return;
	NinjaState=ENinjaTrialState::Returning;
	FinishChallenge();
	NinjaState=ENinjaTrialState::Completed;
}
