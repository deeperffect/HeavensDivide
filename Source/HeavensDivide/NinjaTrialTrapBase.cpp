#include "NinjaTrialTrapBase.h"
#include "NinjaTechniqueTrial.h"
#include "Components/SceneComponent.h"

ANinjaTrialTrapBase::ANinjaTrialTrapBase()
{
	PrimaryActorTick.bCanEverTick=false;
	SceneRoot=CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void ANinjaTrialTrapBase::BeginPlay()
{
	Super::BeginPlay();
	if(OwningTrial)OwningTrial->RegisterTrap(this);
	SetTrapActive(false);
}

void ANinjaTrialTrapBase::SetTrapActive(bool bActive)
{
	if(bTrapActive==bActive&&bActive)return;
	bTrapActive=bActive;
	ResetTrap();
	if(bActive)OnTrapActivated.Broadcast();else OnTrapDeactivated.Broadcast();
}

bool ANinjaTrialTrapBase::DamageTrialPlayer()
{
	if(!bTrapActive||!OwningTrial||!OwningTrial->ApplyTrialHazardDamage(Damage))return false;
	OnTrapDamagedPlayer.Broadcast();
	return true;
}
