#pragma once
#include "CoreMinimal.h"
#include "NinjaTrialTrapBase.h"
#include "NinjaTimedGate.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class ENinjaGateState : uint8 { Inactive, InitialDelay, Open, Closing, Closed, Opening };

UCLASS(Blueprintable)
class HEAVENSDIVIDE_API ANinjaTimedGate : public ANinjaTrialTrapBase
{
	GENERATED_BODY()
public:
	ANinjaTimedGate();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetTrapActive(bool bActive) override;
	UPROPERTY(BlueprintAssignable, Category="Ninja Trap|Events") FNinjaTrapEvent OnGateClosing;
	UPROPERTY(BlueprintAssignable, Category="Ninja Trap|Events") FNinjaTrapEvent OnGateClosed;
	UPROPERTY(BlueprintAssignable, Category="Ninja Trap|Events") FNinjaTrapEvent OnGateOpened;
protected:
	virtual void ResetTrap() override;
	UFUNCTION() void HandleGateOverlap(UPrimitiveComponent* OverlappedComponent,AActor* OtherActor,UPrimitiveComponent* OtherComponent,int32 OtherBodyIndex,bool bFromSweep,const FHitResult& SweepResult);
	void SetGateAlpha(float ClosedAlpha);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ninja Trap") TObjectPtr<UBoxComponent> HazardBox;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ninja Trap") TObjectPtr<UStaticMeshComponent> BarrierVisual;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninja Trap|Gate", meta=(ClampMin="0")) float OpenDuration=2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninja Trap|Gate", meta=(ClampMin="0")) float ClosedDuration=1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninja Trap|Gate", meta=(ClampMin="0.01")) float TransitionDuration=0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninja Trap|Gate", meta=(ClampMin="0")) float InitialDelay=0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninja Trap|Gate") FVector OpenOffset=FVector(0,0,300);
private:
	ENinjaGateState GateState=ENinjaGateState::Inactive;
	float StateElapsed=0.0f;
	bool bHitThisClosure=false;
};
