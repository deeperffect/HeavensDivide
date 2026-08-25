#pragma once
#include "CoreMinimal.h"
#include "NinjaTrialTrapBase.h"
#include "NinjaSweepingTrap.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable)
class HEAVENSDIVIDE_API ANinjaSweepingTrap : public ANinjaTrialTrapBase
{
	GENERATED_BODY()
public:
	ANinjaSweepingTrap();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetTrapActive(bool bActive) override;
	UPROPERTY(BlueprintAssignable, Category="Ninja Trap|Events") FNinjaTrapEvent OnSweepStarted;
	UPROPERTY(BlueprintAssignable, Category="Ninja Trap|Events") FNinjaTrapEvent OnSweepReset;
protected:
	virtual void ResetTrap() override;
	UFUNCTION() void HandleHazardOverlap(UPrimitiveComponent* OverlappedComponent,AActor* OtherActor,UPrimitiveComponent* OtherComponent,int32 OtherBodyIndex,bool bFromSweep,const FHitResult& SweepResult);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ninja Trap") TObjectPtr<UBoxComponent> HazardBox;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ninja Trap") TObjectPtr<UStaticMeshComponent> Visual;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninja Trap|Sweep") FVector MovementDirection=FVector(1,0,0);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninja Trap|Sweep", meta=(ClampMin="0")) float MovementDistance=1000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninja Trap|Sweep", meta=(ClampMin="0.01")) float SweepDuration=2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninja Trap|Sweep", meta=(ClampMin="0")) float DelayBetweenSweeps=1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninja Trap|Sweep", meta=(ClampMin="0")) float InitialDelay=0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninja Trap|Sweep") bool bLooping=true;
private:
	FVector StartLocation=FVector::ZeroVector;
	float PhaseElapsed=0.0f;
	bool bSweeping=false;
	bool bHitThisSweep=false;
	bool bWaitingForFirstSweep=true;
};
