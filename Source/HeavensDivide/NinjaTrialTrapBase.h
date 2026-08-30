#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NinjaTrialTrapBase.generated.h"

class ANinjaTechniqueTrial;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNinjaTrapEvent);

UCLASS(Abstract, Blueprintable)
class HEAVENSDIVIDE_API ANinjaTrialTrapBase : public AActor
{
	GENERATED_BODY()

public:
	ANinjaTrialTrapBase();

	UFUNCTION(BlueprintCallable, Category="Ninja Trial|Trap")
	void InitializeForTrial(ANinjaTechniqueTrial* InOwningTrial);

	UFUNCTION(BlueprintCallable, Category="Ninja Trial|Trap")
	void ActivateTrap();

	UFUNCTION(BlueprintCallable, Category="Ninja Trial|Trap")
	void DeactivateTrap();

	UFUNCTION(BlueprintPure, Category="Ninja Trial|Trap")
	ANinjaTechniqueTrial* GetOwningTrial() const { return OwningTrial; }

	UFUNCTION(BlueprintPure, Category="Ninja Trial|Trap")
	bool IsTrapActive() const { return bTrapActive; }

	UPROPERTY(BlueprintAssignable, Category="Ninja Trial|Trap") FNinjaTrapEvent OnTrapActivated;
	UPROPERTY(BlueprintAssignable, Category="Ninja Trial|Trap") FNinjaTrapEvent OnTrapDeactivated;
	UPROPERTY(BlueprintAssignable, Category="Ninja Trial|Trap") FNinjaTrapEvent OnTrapDamagedPlayer;

protected:
	virtual void HandleActivationChanged(bool bActive) {}
	bool DamageTrialPlayer(AActor* DamageTarget);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninja Trial|Trap", meta=(ClampMin="0")) float Damage = 25.0f;
	UPROPERTY(Transient) TObjectPtr<ANinjaTechniqueTrial> OwningTrial;
	UPROPERTY(Transient) bool bTrapActive = false;

	UFUNCTION(BlueprintImplementableEvent, Category="Ninja Trial|Trap", meta=(DisplayName="On Initialized For Trial"))
	void ReceiveInitializedForTrial();

	UFUNCTION(BlueprintImplementableEvent, Category="Ninja Trial|Trap", meta=(DisplayName="On Trap Activated"))
	void ReceiveTrapActivated();

	UFUNCTION(BlueprintImplementableEvent, Category="Ninja Trial|Trap", meta=(DisplayName="On Trap Deactivated"))
	void ReceiveTrapDeactivated();

};
