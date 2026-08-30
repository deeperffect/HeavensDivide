#include "NinjaTrialTrapBase.h"

#include "NinjaTechniqueTrial.h"

ANinjaTrialTrapBase::ANinjaTrialTrapBase()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ANinjaTrialTrapBase::InitializeForTrial(ANinjaTechniqueTrial* InOwningTrial)
{
	if (!IsValid(InOwningTrial) || OwningTrial == InOwningTrial) return;
	if (bTrapActive) DeactivateTrap();
	OwningTrial = InOwningTrial;
	ReceiveInitializedForTrial();
	UE_LOG(LogTemp, Log, TEXT("[NinjaTrialTraps] Initialized trap=%s OwningTrial=%s Valid=%s"), *GetName(), *GetNameSafe(OwningTrial), IsValid(OwningTrial) ? TEXT("true") : TEXT("false"));
}

void ANinjaTrialTrapBase::ActivateTrap()
{
	if (!IsValid(OwningTrial) || bTrapActive) return;
	bTrapActive = true;
	HandleActivationChanged(true);
	ReceiveTrapActivated();
	OnTrapActivated.Broadcast();
}

void ANinjaTrialTrapBase::DeactivateTrap()
{
	if (!bTrapActive) return;
	bTrapActive = false;
	HandleActivationChanged(false);
	ReceiveTrapDeactivated();
	OnTrapDeactivated.Broadcast();
}

bool ANinjaTrialTrapBase::DamageTrialPlayer(AActor* DamageTarget)
{
	if (!bTrapActive || !IsValid(OwningTrial) || !OwningTrial->IsTrialRunning() || !OwningTrial->IsActivePlayerCharacter(DamageTarget)) return false;
	if (!OwningTrial->ApplyTrialHazardDamage(Damage)) return false;
	OnTrapDamagedPlayer.Broadcast();
	return true;
}
