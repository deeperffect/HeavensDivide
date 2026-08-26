#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "BossToriiGate.generated.h"

class AEnemySpawner;
class ASurvivorPlayerController;
class USceneComponent;
class USphereComponent;
class UStaticMeshComponent;
class UWidgetComponent;

UENUM(BlueprintType)
enum class EBossToriiGateState : uint8 { Locked, Unlocked, TravelCommitted };
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBossGateEvent);

UCLASS(Blueprintable)
class HEAVENSDIVIDE_API ABossToriiGate : public AActor, public IInteractable
{
	GENERATED_BODY()
public:
	ABossToriiGate();
	virtual void Tick(float DeltaSeconds) override;
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
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boss Gate|Components") TObjectPtr<USphereComponent> InteractionSphere;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boss Gate|Components") TObjectPtr<UWidgetComponent> InteractionPromptComponent;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction|Prompt") float PromptVerticalOffset = 240.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction|Prompt") FVector2D PromptDrawSize = FVector2D(360.0f, 160.0f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction|Prompt", meta=(ClampMin="0.01")) float PromptWorldScale = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction|Prompt") int32 PromptTranslucencySortPriority = 100;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss Gate|Interaction", meta=(ClampMin="1.0")) float InteractionRadius = 300.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss Gate", meta=(ClampMin="0.0")) float UnlockRunTimeSeconds = 480.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss Gate") TSoftObjectPtr<UWorld> BossArenaLevel;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss Gate|References") TObjectPtr<AEnemySpawner> EnemySpawner;

private:
	void FindRequiredReferences();
	void CreateInteractionPrompt();
	void UpdateInactivePrompt();
	void FaceInteractionPromptToCamera();
	bool ShouldShowPrompt(APawn* InteractingPawn) const;
	void PollUnlockState();
	void UnlockGate();
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Boss Gate", meta=(AllowPrivateAccess="true")) EBossToriiGateState GateState = EBossToriiGateState::Locked;
	UPROPERTY() TObjectPtr<ASurvivorPlayerController> PlayerController;
	FTimerHandle UnlockPollTimer;
};
