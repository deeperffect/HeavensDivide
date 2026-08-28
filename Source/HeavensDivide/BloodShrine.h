// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "BloodShrine.generated.h"

class AEnemyBase;
class AEnemySpawner;
class ASurvivorPlayerController;
class UMinimapMarkerComponent;
class UBloodShrineWidget;
class USphereComponent;
class UStaticMeshComponent;
class UWidgetComponent;
class UObjectiveInteractionComponent;

UENUM(BlueprintType)
enum class EBloodShrineState : uint8
{
	Inactive,
	Active,
	Success,
	Failed
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBloodShrineSimpleEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBloodShrineProgressEvent, int32, CurrentBlood, int32, RequiredBlood);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBloodShrineRewardRequested, class ABloodShrine*, Shrine);

UCLASS(Blueprintable)
class HEAVENSDIVIDE_API ABloodShrine : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ABloodShrine();

	virtual bool CanInteract_Implementation(APawn* InteractingPawn) const override;
	virtual void Interact_Implementation(APawn* InteractingPawn) override;

	UFUNCTION(BlueprintCallable, Category = "Blood Shrine")
	bool ActivateShrine(APawn* InteractingPawn);

	UFUNCTION(BlueprintPure, Category = "Blood Shrine")
	EBloodShrineState GetShrineState() const;

	UFUNCTION(BlueprintPure, Category = "Blood Shrine")
	int32 GetCurrentBlood() const;

	UFUNCTION(BlueprintPure, Category = "Blood Shrine", meta = (DeprecatedFunction, DeprecationMessage = "Use GetCurrentBlood."))
	int32 GetCurrentKills() const;

	UFUNCTION(BlueprintPure, Category = "Blood Shrine")
	float GetTimeRemaining() const;

	UPROPERTY(BlueprintAssignable, Category = "Blood Shrine|Events")
	FBloodShrineSimpleEvent OnShrineActivated;

	UPROPERTY(BlueprintAssignable, Category = "Blood Shrine|Events")
	FBloodShrineProgressEvent OnProgressChanged;

	UPROPERTY(BlueprintAssignable, Category = "Blood Shrine|Events")
	FBloodShrineSimpleEvent OnShrineSucceeded;

	UPROPERTY(BlueprintAssignable, Category = "Blood Shrine|Events")
	FBloodShrineSimpleEvent OnShrineFailed;

	UPROPERTY(BlueprintAssignable, Category = "Blood Shrine|Reward")
	FBloodShrineRewardRequested OnBloodShrineRewardRequested;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Blood Shrine|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Blood Shrine|Components")
	TObjectPtr<UStaticMeshComponent> ShrineMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Blood Shrine|Components")
	TObjectPtr<UObjectiveInteractionComponent> ObjectiveInteraction;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Minimap") TObjectPtr<UMinimapMarkerComponent> MinimapMarker;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood Shrine|Challenge", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float ChallengeDuration = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood Shrine|Challenge", meta = (ClampMin = "1", UIMin = "1"))
	int32 RequiredBlood = 30;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood Shrine|Challenge", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SpawnPressureMultiplier = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood Shrine|Bloodbound", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BloodboundHealthMultiplier = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood Shrine|Bloodbound", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BloodboundDamageMultiplier = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood Shrine|Bloodbound", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BloodboundMovementSpeedMultiplier = 1.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood Shrine|Bloodbound")
	bool bBloodboundDropsXP = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood Shrine|Bloodbound", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float InitialBloodboundConversionPercent = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood Shrine|Reward", meta = (ClampMin = "1", UIMin = "1"))
	int32 RewardUpgradeChoices = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood Shrine|Reward", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ResultDisplayDuration = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood Shrine|References")
	TObjectPtr<AEnemySpawner> EnemySpawner;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood Shrine|Rules")
	bool bAllowReactivation = false;

	UFUNCTION(BlueprintImplementableEvent, Category = "Blood Shrine|Events")
	void ReceiveShrineActivated();

	UFUNCTION(BlueprintImplementableEvent, Category = "Blood Shrine|Events")
	void ReceiveProgressChanged(int32 NewCurrentBlood, int32 NewRequiredBlood);

	UFUNCTION(BlueprintImplementableEvent, Category = "Blood Shrine|Events")
	void ReceiveShrineSucceeded();

	UFUNCTION(BlueprintImplementableEvent, Category = "Blood Shrine|Events")
	void ReceiveShrineFailed();

private:
	UFUNCTION()
	void HandleEnemyKilled(AEnemyBase* Enemy);

	UFUNCTION()
	void HandleEnemyBecameBloodbound(AEnemyBase* Enemy);

	UFUNCTION()
	void HandlePlayerDeath();

	void UpdateChallenge();
	void SucceedChallenge();
	void FailChallenge(bool bShowFailure);
	void EndChallenge();
	void RevertOwnedBloodboundEnemies();
	void RequestReward();
	void FindRequiredReferences();
	void CreateStatusWidget();
	FName GetPressureModifierId() const;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Blood Shrine", meta = (AllowPrivateAccess = "true"))
	EBloodShrineState ShrineState = EBloodShrineState::Inactive;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Blood Shrine", meta = (AllowPrivateAccess = "true"))
	int32 CurrentBlood = 0;

	UPROPERTY()
	TObjectPtr<ASurvivorPlayerController> PlayerController;

	UPROPERTY()
	TObjectPtr<UBloodShrineWidget> StatusWidget;

	FTimerHandle ChallengeUpdateTimer;
	FTimerHandle RewardTimer;
	double ChallengeEndTime = 0.0;
	bool bRewardRequested = false;
	TArray<TWeakObjectPtr<AEnemyBase>> OwnedBloodboundEnemies;
};
