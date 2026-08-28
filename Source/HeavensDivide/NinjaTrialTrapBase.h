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
	virtual void BeginPlay() override;
	UFUNCTION(BlueprintCallable, Category="Ninja Trap") virtual void SetTrapActive(bool bActive);
	UFUNCTION(BlueprintPure, Category="Ninja Trap") bool IsTrapActive() const { return bTrapActive; }
	UFUNCTION(BlueprintPure, Category="Ninja Trap") ANinjaTechniqueTrial* GetOwningTrial() const { return OwningTrial; }
	void InitializeForTrial(ANinjaTechniqueTrial* Trial);
	UPROPERTY(BlueprintAssignable, Category="Ninja Trap|Events") FNinjaTrapEvent OnTrapActivated;
	UPROPERTY(BlueprintAssignable, Category="Ninja Trap|Events") FNinjaTrapEvent OnTrapDeactivated;
	UPROPERTY(BlueprintAssignable, Category="Ninja Trap|Events") FNinjaTrapEvent OnTrapDamagedPlayer;
protected:
	virtual void ResetTrap() {}
	bool DamageTrialPlayer();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ninja Trap") TObjectPtr<USceneComponent> SceneRoot;
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Ninja Trap") TObjectPtr<ANinjaTechniqueTrial> OwningTrial;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninja Trap", meta=(ClampMin="0")) float Damage=25.0f;
	bool bTrapActive=false;
};
