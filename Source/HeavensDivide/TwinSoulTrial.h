// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "TwinSoulTrial.generated.h"

class AEnemyBase;
class ACharacterBase;
class AEnemySpawner;
class ASurvivorPlayerController;
class UMinimapMarkerComponent;
class UBloodShrineWidget;
class UBoxComponent;
class USceneComponent;
class USphereComponent;
class UStaticMeshComponent;
class UWidgetComponent;
class UMaterialInterface;
class ATrialArenaAnchor;
class UObjectiveInteractionComponent;

UENUM(BlueprintType)
enum class ETwinSoulTrialState : uint8
{
	Inactive,
	TrialActive,
	AwaitingReward,
	Completed,
	Failed
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTwinSoulTrialEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTwinSoulTargetEvent, AEnemyBase*, Target);

UCLASS(Blueprintable)
class HEAVENSDIVIDE_API ATwinSoulTrial : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ATwinSoulTrial();

	virtual bool CanInteract_Implementation(APawn* InteractingPawn) const override;
	virtual void Interact_Implementation(APawn* InteractingPawn) override;

	UFUNCTION(BlueprintCallable, Category = "Twin Soul Trial")
	bool EnterTrial(APawn* InteractingPawn);

	UFUNCTION(BlueprintPure, Category = "Twin Soul Trial")
	ETwinSoulTrialState GetTrialState() const { return TrialState; }
	AEnemyBase* GetCrimsonTarget() const { return CrimsonTarget; }
	AEnemyBase* GetVioletTarget() const { return VioletTarget; }

	UPROPERTY(BlueprintAssignable, Category = "Twin Soul Trial|Events")
	FTwinSoulTrialEvent OnTrialEntered;
	UPROPERTY(BlueprintAssignable, Category = "Twin Soul Trial|Events")
	FTwinSoulTargetEvent OnCrimsonSpawned;
	UPROPERTY(BlueprintAssignable, Category = "Twin Soul Trial|Events")
	FTwinSoulTargetEvent OnVioletSpawned;
	UPROPERTY(BlueprintAssignable, Category = "Twin Soul Trial|Events")
	FTwinSoulTrialEvent OnCrimsonDefeated;
	UPROPERTY(BlueprintAssignable, Category = "Twin Soul Trial|Events")
	FTwinSoulTrialEvent OnVioletDefeated;
	UPROPERTY(BlueprintAssignable, Category = "Twin Soul Trial|Events")
	FTwinSoulTrialEvent OnTrialCompleted;
	UPROPERTY(BlueprintAssignable, Category = "Twin Soul Trial|Events")
	FTwinSoulTrialEvent OnPlayerReturned;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Twin Soul Trial|Components")
	TObjectPtr<USceneComponent> SceneRoot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Twin Soul Trial|Components")
	TObjectPtr<UStaticMeshComponent> PortalMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Twin Soul Trial|Components")
	TObjectPtr<UObjectiveInteractionComponent> ObjectiveInteraction;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Minimap") TObjectPtr<UMinimapMarkerComponent> MinimapMarker;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Twin Soul Trial|Components")
	TObjectPtr<UStaticMeshComponent> TrialFloor;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Twin Soul Trial|Components")
	TArray<TObjectPtr<UStaticMeshComponent>> TrialWalls;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Twin Soul Trial|Challenge")
	TSubclassOf<AEnemyBase> CrimsonEnemyClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Twin Soul Trial|Challenge")
	TSubclassOf<AEnemyBase> VioletEnemyClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Twin Soul Trial|Challenge", meta = (ClampMin = "1.0"))
	float CrimsonMaxHealth = 120.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Twin Soul Trial|Challenge", meta = (ClampMin = "1.0"))
	float VioletMaxHealth = 120.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Twin Soul Trial|Challenge")
	FVector TrialArenaOffset = FVector(50000.0f, 0.0f, 0.0f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Twin Soul Trial|Challenge")
	FVector TrialPlayerOffset = FVector(0.0f, -900.0f, 150.0f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Twin Soul Trial|Challenge")
	FVector CrimsonSpawnOffset = FVector(-500.0f, 500.0f, 150.0f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Twin Soul Trial|Challenge")
	FVector VioletSpawnOffset = FVector(500.0f, 500.0f, 150.0f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Twin Soul Trial|Challenge")
	FVector ReturnOffset = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Twin Soul Trial|Challenge", meta = (ClampMin = "1"))
	int32 RewardChoiceCount = 3;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Twin Soul Trial|Visuals")
	TObjectPtr<UMaterialInterface> CrimsonOverlayMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Twin Soul Trial|Visuals")
	TObjectPtr<UMaterialInterface> VioletOverlayMaterial;

private:
	UFUNCTION()
	void HandleCrimsonDied(AEnemyBase* Enemy);
	UFUNCTION()
	void HandleVioletDied(AEnemyBase* Enemy);
	UFUNCTION()
	void HandlePlayerDeath();
	UFUNCTION()
	void HandleRewardCompleted();

	void FindReferences();
	bool SpawnTargets();
	bool InitializeTrialArena();
	bool IsTrialArenaValid(const ACharacterBase* Character, FVector& OutArrivalLocation) const;
	void CompleteTrial();
	void ReturnPlayerToArena();
	void CleanupTargetBindings();

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Twin Soul Trial", meta = (AllowPrivateAccess = "true"))
	ETwinSoulTrialState TrialState = ETwinSoulTrialState::Inactive;
	UPROPERTY()
	TObjectPtr<AEnemySpawner> MainSpawner;
	UPROPERTY()
	TObjectPtr<ASurvivorPlayerController> PlayerController;
	UPROPERTY()
	TObjectPtr<AEnemyBase> CrimsonTarget;
	UPROPERTY()
	TObjectPtr<AEnemyBase> VioletTarget;
	FTransform ReturnTransform;
	bool bCrimsonDead = false;
	bool bVioletDead = false;
	bool bArenaReady = false;
	FVector ArenaOrigin = FVector::ZeroVector;
	UPROPERTY() TObjectPtr<ATrialArenaAnchor> ArenaAnchor;
};
