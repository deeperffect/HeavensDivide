#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "NinjaTechniqueTrial.generated.h"

class UChildActorComponent;
class UAutoAttackComponent;
class AEnemySpawner;
class ANinjaTrialTrapBase;
class ANinjaTrialGoal;
class ASurvivorPlayerController;
class UObjectiveInteractionComponent;
class USceneComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class ENinjaTrialState : uint8
{
	Inactive,
	Running,
	AwaitingReward,
	Returning,
	Completed,
	Failed
};

UCLASS(Blueprintable)
class HEAVENSDIVIDE_API ANinjaTechniqueTrial : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ANinjaTechniqueTrial();

	virtual bool CanInteract_Implementation(APawn* InteractingPawn) const override;
	virtual void Interact_Implementation(APawn* InteractingPawn) override;

	UFUNCTION(BlueprintCallable, Category="Ninja Trial")
	bool EnterTrial(APawn* InteractingPawn);

	UFUNCTION(BlueprintCallable, Category="Ninja Trial")
	void NotifyGoalReached(AActor* ReachingActor);

	UFUNCTION(BlueprintPure, Category="Ninja Trial")
	ENinjaTrialState GetTrialState() const { return TrialState; }
	UFUNCTION(BlueprintPure, Category="Ninja Trial") bool IsTrialRunning() const { return TrialState == ENinjaTrialState::Running; }
	bool IsActivePlayerCharacter(const AActor* Candidate) const;
	bool ApplyTrialHazardDamage(float DamageAmount);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninja Trial|Reward", meta=(ClampMin="1"))
	int32 RewardChoiceCount = 3;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ninja Trial|Entrance")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ninja Trial|Entrance")
	TObjectPtr<UStaticMeshComponent> NinjaStatue;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ninja Trial|Entrance")
	TObjectPtr<UObjectiveInteractionComponent> ObjectiveInteraction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ninja Trial|Arena")
	TObjectPtr<USceneComponent> ArenaRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ninja Trial|Arena")
	TObjectPtr<UStaticMeshComponent> TrialFloor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ninja Trial|Arena")
	TObjectPtr<USceneComponent> TrialPlayerStart;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ninja Trial|Arena")
	TObjectPtr<UChildActorComponent> FloorTrap01;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ninja Trial|Arena")
	TObjectPtr<UChildActorComponent> SweepingTrap01;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ninja Trial|Arena")
	TObjectPtr<UChildActorComponent> TimedGate01;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ninja Trial|Arena")
	TObjectPtr<UChildActorComponent> Goal;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Ninja Trial|Traps")
	TArray<TObjectPtr<ANinjaTrialTrapBase>> RegisteredTraps;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Ninja Trial|Course")
	TObjectPtr<ANinjaTrialGoal> RegisteredGoal;

private:
	void FindRuntimeReferences();
	void DiscoverAndInitializeTraps();
	void ActivateRegisteredTraps();
	void DeactivateRegisteredTraps();
	void RestoreGameplayState(bool bReturnPlayer);
	UFUNCTION() void HandleRewardCompleted();
	UFUNCTION() void HandlePlayerDeath();

	UPROPERTY(Transient)
	TObjectPtr<ASurvivorPlayerController> PlayerController;

	UPROPERTY(Transient)
	TObjectPtr<AEnemySpawner> MainSpawner;

	ENinjaTrialState TrialState = ENinjaTrialState::Inactive;
	bool bNinjaAutoAttackWasEnabled = false;
	bool bSamuraiAutoAttackWasEnabled = false;
	bool bHasReturnTransform = false;
	FTransform ReturnTransform;
};
