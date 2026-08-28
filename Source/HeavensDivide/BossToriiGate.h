#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "BossToriiGate.generated.h"

class AEnemySpawner;
class ASurvivorPlayerController;
class UMinimapMarkerComponent;
class USceneComponent;
class USphereComponent;
class UStaticMeshComponent;
class UWidgetComponent;
class UObjectiveInteractionComponent;

UENUM(BlueprintType)
enum class EBossToriiGateState : uint8 { Locked, Unlocked, TravelCommitted };
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBossGateEvent);

UCLASS(Blueprintable)
class HEAVENSDIVIDE_API ABossToriiGate : public AActor, public IInteractable
{
	GENERATED_BODY()
public:
	ABossToriiGate();
	virtual bool CanInteract_Implementation(APawn* InteractingPawn) const override;
	virtual void Interact_Implementation(APawn* InteractingPawn) override;
	UFUNCTION(BlueprintPure, Category="Boss Gate") EBossToriiGateState GetGateState() const { return GateState; }
	UPROPERTY(BlueprintAssignable, Category="Boss Gate|Events") FBossGateEvent OnBossGateUnlocked;
	UPROPERTY(BlueprintAssignable, Category="Boss Gate|Events") FBossGateEvent OnBossGateTravelStarted;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boss Gate|Components") TObjectPtr<USceneComponent> SceneRoot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boss Gate|Components") TObjectPtr<UStaticMeshComponent> GateVisual;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boss Gate|Components") TObjectPtr<UObjectiveInteractionComponent> ObjectiveInteraction;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Minimap") TObjectPtr<UMinimapMarkerComponent> MinimapMarker;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss Gate", meta=(ClampMin="0.0")) float UnlockRunTimeSeconds = 480.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss Gate") TSoftObjectPtr<UWorld> BossArenaLevel;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss Gate|References") TObjectPtr<AEnemySpawner> EnemySpawner;

private:
	void FindRequiredReferences();
	void PollUnlockState();
	void UnlockGate();
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Boss Gate", meta=(AllowPrivateAccess="true")) EBossToriiGateState GateState = EBossToriiGateState::Locked;
	UPROPERTY() TObjectPtr<ASurvivorPlayerController> PlayerController;
	FTimerHandle UnlockPollTimer;
};
