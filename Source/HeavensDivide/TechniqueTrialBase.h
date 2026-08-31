#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "TechniqueTrialBase.generated.h"

class ACharacterBase;
class AEnemySpawner;
class ASurvivorPlayerController;
class UAutoAttackComponent;
class UBloodShrineWidget;
class UHealthComponent;
class USceneComponent;
class USphereComponent;
class UStaticMeshComponent;
class UWidgetComponent;
class UMinimapMarkerComponent;
class UObjectiveInteractionComponent;
class ATrialArenaAnchor;

UENUM(BlueprintType)
enum class ETechniqueTrialState : uint8 { Inactive, Active, Result, Completed, Failed };

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTechniqueTrialEvent);

UCLASS(Abstract, Blueprintable)
class HEAVENSDIVIDE_API ATechniqueTrialBase : public AActor, public IInteractable
{
	GENERATED_BODY()
public:
	ATechniqueTrialBase();
	virtual bool CanInteract_Implementation(APawn* InteractingPawn) const override;
	virtual void Interact_Implementation(APawn* InteractingPawn) override;
	UFUNCTION(BlueprintCallable, Category="Technique Trial") bool EnterTrial(APawn* InteractingPawn);
	UFUNCTION(BlueprintPure, Category="Technique Trial") ETechniqueTrialState GetTrialState() const { return TrialState; }
	UPROPERTY(BlueprintAssignable, Category="Technique Trial|Events") FTechniqueTrialEvent OnTrialEntered;
	UPROPERTY(BlueprintAssignable, Category="Technique Trial|Events") FTechniqueTrialEvent OnTechniqueTrialCompleted;
	UPROPERTY(BlueprintAssignable, Category="Technique Trial|Events") FTechniqueTrialEvent OnPlayerReturned;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool BeginChallenge() PURE_VIRTUAL(ATechniqueTrialBase::BeginChallenge, return false;);
	virtual void StopChallenge() {}
	virtual bool PrepareActiveCharacter();
	virtual bool ShouldSuspendAutoAttacksDuringTrial() const { return bSuspendAutoAttacksDuringTrial; }
	void FinishChallenge();
	void AbortTrial();
	FVector GetTrialOrigin() const { return ArenaOrigin; }
	FTransform GetArenaWorldTransform() const;
	ACharacterBase* GetActiveCharacter() const;
	FVector GetGroundedTrialPlayerLocation(ACharacterBase* Character) const;
	bool IsTrialArenaValid(const ACharacterBase* Character, FVector& OutArrivalLocation) const;
	virtual void RelocateAdditionalArenaComponents(const FVector& WorldDelta) {}
	bool IsArenaReady() const { return bArenaReady; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Technique Trial|Components") TObjectPtr<USceneComponent> SceneRoot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Technique Trial|Components") TObjectPtr<UStaticMeshComponent> StatueMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Technique Trial|Components") TObjectPtr<UObjectiveInteractionComponent> ObjectiveInteraction;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Technique Trial|Components") TObjectPtr<USceneComponent> TrialArena;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Technique Trial|Components") TObjectPtr<UStaticMeshComponent> TrialFloor;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Technique Trial|Components") TObjectPtr<USceneComponent> TrialPlayerStart;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Minimap") TObjectPtr<UMinimapMarkerComponent> MinimapMarker;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Technique Trial|Components") TArray<TObjectPtr<UStaticMeshComponent>> TrialWalls;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Technique Trial|Arena") FVector TrialArenaOffset = FVector(0, 50000, 0);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Technique Trial|Arena") FVector TrialPlayerOffset = FVector(0, -900, 150);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Technique Trial|Arena") FVector ReturnOffset = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Technique Trial|Result", meta=(ClampMin="0")) float ResultDisplayDuration = 1.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Technique Trial|Rules") bool bAllowReactivation = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Technique Trial|Rules") bool bForceSamuraiOnEntry = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Technique Trial|Rules") bool bLockSwappingDuringTrial = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Technique Trial|Rules") bool bSuspendAutoAttacksDuringTrial = true;
	UPROPERTY() TObjectPtr<ASurvivorPlayerController> PlayerController;
	UPROPERTY() TObjectPtr<AEnemySpawner> MainSpawner;
	ETechniqueTrialState TrialState = ETechniqueTrialState::Inactive;
private:
	UFUNCTION() void HandlePlayerDeath();
	void FindReferences();
	void ReturnToArena();
	void CleanupRuntime(bool bReturnPlayer);
	FTransform ReturnTransform;
	bool bSamuraiAutoAttackWasEnabled = true;
	bool bNinjaAutoAttackWasEnabled = true;
	bool bRuntimeOwned = false;
	bool bArenaReady = false;
	FVector ArenaOrigin = FVector::ZeroVector;
	UPROPERTY() TObjectPtr<ATrialArenaAnchor> ArenaAnchor;
	FTimerHandle ResultTimer;
};
