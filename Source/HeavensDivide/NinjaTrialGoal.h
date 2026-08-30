#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NinjaTrialGoal.generated.h"

class USceneComponent;
class USphereComponent;
class UStaticMeshComponent;
class ANinjaTechniqueTrial;

UCLASS(Blueprintable)
class HEAVENSDIVIDE_API ANinjaTrialGoal : public AActor
{
	GENERATED_BODY()
public:
	ANinjaTrialGoal();

	UFUNCTION(BlueprintCallable, Category="Ninja Trial|Goal")
	void InitializeForTrial(ANinjaTechniqueTrial* InOwningTrial);

protected:
	UFUNCTION()
	void HandleGoalOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ninja Trial|Shell") TObjectPtr<USceneComponent> SceneRoot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ninja Trial|Shell") TObjectPtr<UStaticMeshComponent> GoalVisual;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ninja Trial|Goal") TObjectPtr<USphereComponent> GoalTrigger;

private:
	UPROPERTY(Transient)
	TObjectPtr<ANinjaTechniqueTrial> OwningTrial;
};
