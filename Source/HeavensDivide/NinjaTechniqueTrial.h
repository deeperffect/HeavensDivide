#pragma once

#include "CoreMinimal.h"
#include "TechniqueTrialBase.h"
#include "NinjaTechniqueTrial.generated.h"

class ANinjaTrialTrapBase;
class ANinjaFloorTrap;
class ANinjaSweepingTrap;
class ANinjaTimedGate;
class ANinjaTrialGoal;
class UHealthComponent;

UENUM(BlueprintType)
enum class ENinjaTrialState : uint8 { Inactive, Entering, Running, Reward, Returning, Completed, Failed };

UCLASS(Blueprintable)
class HEAVENSDIVIDE_API ANinjaTechniqueTrial : public ATechniqueTrialBase
{
	GENERATED_BODY()
public:
	ANinjaTechniqueTrial();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UFUNCTION(BlueprintPure, Category="Ninja Trial") ENinjaTrialState GetNinjaTrialState() const { return NinjaState; }
	UFUNCTION(BlueprintPure, Category="Ninja Trial") bool IsTrialRunning() const { return NinjaState==ENinjaTrialState::Running; }
	UFUNCTION(BlueprintCallable, Category="Ninja Trial") void RegisterTrap(ANinjaTrialTrapBase* Trap);
	UFUNCTION(BlueprintCallable, Category="Ninja Trial") bool ApplyTrialHazardDamage(float DamageAmount);
	UFUNCTION(BlueprintCallable, Category="Ninja Trial") void CompleteCourse();
protected:
	virtual bool BeginChallenge() override;
	virtual void StopChallenge() override;
	virtual bool PrepareActiveCharacter() override;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninja Trial|Reward", meta=(ClampMin="1")) int32 RewardChoiceCount=3;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Ninja Trial") ENinjaTrialState NinjaState=ENinjaTrialState::Inactive;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninja Trial|Arena") TSubclassOf<ANinjaFloorTrap> FloorTrapClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninja Trial|Arena") TSubclassOf<ANinjaSweepingTrap> SweepingTrapClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninja Trial|Arena") TSubclassOf<ANinjaTimedGate> TimedGateClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninja Trial|Arena") TSubclassOf<ANinjaTrialGoal> GoalClass;
private:
	UFUNCTION() void HandleRewardCompleted();
	void SetAllTrapsActive(bool bActive);
	UPROPERTY() TArray<TObjectPtr<ANinjaTrialTrapBase>> RegisteredTraps;
	UPROPERTY() TArray<TObjectPtr<AActor>> OwnedArenaActors;
	bool bCompletionHandled=false;
};
