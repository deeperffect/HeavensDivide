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

UENUM(BlueprintType)
enum class ETechniqueTrialState : uint8 { Inactive, Active, Result, Completed, Failed };

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTechniqueTrialEvent);

UCLASS(Abstract, Blueprintable)
class HEAVENSDIVIDE_API ATechniqueTrialBase : public AActor, public IInteractable
{
	GENERATED_BODY()
public:
	ATechniqueTrialBase();
	virtual void Tick(float DeltaSeconds) override;
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
	void FinishChallenge();
	void AbortTrial();
	FVector GetTrialOrigin() const { return GetActorLocation() + TrialArenaOffset; }
	ACharacterBase* GetActiveCharacter() const;
	FVector GetGroundedTrialPlayerLocation(ACharacterBase* Character) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Technique Trial|Components") TObjectPtr<USceneComponent> SceneRoot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Technique Trial|Components") TObjectPtr<UStaticMeshComponent> StatueMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Technique Trial|Components") TObjectPtr<USphereComponent> InteractionSphere;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Technique Trial|Components") TObjectPtr<UWidgetComponent> InteractionPrompt;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Technique Trial|Components") TObjectPtr<UStaticMeshComponent> TrialFloor;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Technique Trial|Components") TArray<TObjectPtr<UStaticMeshComponent>> TrialWalls;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Technique Trial|Arena") FVector TrialArenaOffset = FVector(0, 50000, 0);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Technique Trial|Arena") FVector TrialPlayerOffset = FVector(0, -900, 150);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Technique Trial|Arena") FVector ReturnOffset = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Technique Trial|Result", meta=(ClampMin="0")) float ResultDisplayDuration = 1.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Technique Trial|Rules") bool bAllowReactivation = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Technique Trial|Rules") bool bForceSamuraiOnEntry = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Technique Trial|Rules") bool bLockSwappingDuringTrial = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Technique Trial|Rules") bool bSuspendAutoAttacksDuringTrial = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Technique Trial|Interaction") float InteractionRange = 300.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Technique Trial|Interaction") float PromptVerticalOffset = 240.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Technique Trial|Interaction") FText PromptTitle = FText::FromString(TEXT("SAMURAI TRIAL"));
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Technique Trial|Interaction", meta=(ClampMin="1.0")) FVector2D PromptDrawSize = FVector2D(360.0f, 160.0f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Technique Trial|Interaction", meta=(ClampMin="0.01")) float PromptWorldScale = 0.5f;
	UPROPERTY() TObjectPtr<ASurvivorPlayerController> PlayerController;
	UPROPERTY() TObjectPtr<AEnemySpawner> MainSpawner;
	ETechniqueTrialState TrialState = ETechniqueTrialState::Inactive;
private:
	UFUNCTION() void HandlePlayerDeath();
	void FindReferences();
	void UpdatePrompt();
	void FacePromptToCamera();
	void ReturnToArena();
	void CleanupRuntime(bool bReturnPlayer);
	FTransform ReturnTransform;
	bool bSamuraiAutoAttackWasEnabled = true;
	bool bNinjaAutoAttackWasEnabled = true;
	bool bRuntimeOwned = false;
	FTimerHandle ResultTimer;
};
