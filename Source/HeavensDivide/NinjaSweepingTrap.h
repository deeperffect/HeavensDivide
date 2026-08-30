#pragma once
#include "CoreMinimal.h"
#include "NinjaTrialTrapBase.h"
#include "NinjaSweepingTrap.generated.h"
class UBoxComponent; class UStaticMeshComponent;
UCLASS(Blueprintable)
class HEAVENSDIVIDE_API ANinjaSweepingTrap : public ANinjaTrialTrapBase
{
	GENERATED_BODY()
public:
	ANinjaSweepingTrap(); virtual void BeginPlay() override; virtual void Tick(float DeltaSeconds) override;
	UPROPERTY(BlueprintAssignable, Category="Ninja Trial|Trap") FNinjaTrapEvent OnSweepStarted;
	UPROPERTY(BlueprintAssignable, Category="Ninja Trial|Trap") FNinjaTrapEvent OnSweepReset;
protected:
	virtual void HandleActivationChanged(bool bActive) override;
	UFUNCTION() void HandleHazardOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ninja Trial|Trap") TObjectPtr<UBoxComponent> HazardBox;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ninja Trial|Trap") TObjectPtr<UStaticMeshComponent> Visual;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninja Trial|Trap|Sweep") FVector MovementDirection=FVector(1,0,0);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninja Trial|Trap|Sweep", meta=(ClampMin="0")) float MovementDistance=1000;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninja Trial|Trap|Sweep", meta=(ClampMin="0.01")) float SweepDuration=2;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninja Trial|Trap|Sweep", meta=(ClampMin="0")) float DelayBetweenSweeps=1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninja Trial|Trap|Sweep", meta=(ClampMin="0")) float InitialDelay=.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninja Trial|Trap|Sweep") bool bLooping=true;
private:
	FVector StartHazardRelativeLocation=FVector::ZeroVector; float PhaseElapsed=0; bool bSweeping=false; bool bHitThisSweep=false; bool bWaitingForFirstSweep=true;
};
