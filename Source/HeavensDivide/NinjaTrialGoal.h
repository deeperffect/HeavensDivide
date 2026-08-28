#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NinjaTrialGoal.generated.h"
class ANinjaTechniqueTrial;
class UBoxComponent;
class UStaticMeshComponent;
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNinjaGoalEvent);
UCLASS(Blueprintable)
class HEAVENSDIVIDE_API ANinjaTrialGoal : public AActor
{
	GENERATED_BODY()
public:
	ANinjaTrialGoal();
	void InitializeForTrial(ANinjaTechniqueTrial* Trial) { OwningTrial=Trial; }
	UPROPERTY(BlueprintAssignable,Category="Ninja Trial|Events") FNinjaGoalEvent OnGoalReached;
protected:
	UFUNCTION() void HandleOverlap(UPrimitiveComponent* OverlappedComponent,AActor* OtherActor,UPrimitiveComponent* OtherComponent,int32 OtherBodyIndex,bool bFromSweep,const FHitResult& SweepResult);
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UBoxComponent> GoalTrigger;
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> GoalVisual;
	UPROPERTY(EditInstanceOnly,BlueprintReadOnly,Category="Ninja Trial") TObjectPtr<ANinjaTechniqueTrial> OwningTrial;
	bool bCompleted=false;
};
