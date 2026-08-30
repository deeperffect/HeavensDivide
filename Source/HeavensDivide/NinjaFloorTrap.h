#pragma once
#include "CoreMinimal.h"
#include "NinjaTrialTrapBase.h"
#include "NinjaFloorTrap.generated.h"

class UBoxComponent;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
UENUM(BlueprintType) enum class ENinjaFloorTrapState : uint8 { Inactive, Waiting, Telegraphing, Active, Cooldown };

UCLASS(Blueprintable)
class HEAVENSDIVIDE_API ANinjaFloorTrap : public ANinjaTrialTrapBase
{
	GENERATED_BODY()
public:
	ANinjaFloorTrap();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	UPROPERTY(BlueprintAssignable, Category="Ninja Trial|Trap") FNinjaTrapEvent OnTelegraphStarted;
	UPROPERTY(BlueprintAssignable, Category="Ninja Trial|Trap") FNinjaTrapEvent OnDamageResolved;
protected:
	virtual void HandleActivationChanged(bool bActive) override;
	void BeginTelegraph();
	void ResolveDamage();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ninja Trial|Trap") TObjectPtr<UBoxComponent> HazardArea;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ninja Trial|Trap") TObjectPtr<UStaticMeshComponent> TelegraphVisual;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninja Trial|Trap|Timing", meta=(ClampMin="0.01")) float TelegraphDuration = 1.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninja Trial|Trap|Timing", meta=(ClampMin="0")) float ActiveDuration = 0.15f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninja Trial|Trap|Timing", meta=(ClampMin="0")) float CooldownDuration = 1.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninja Trial|Trap|Timing", meta=(ClampMin="0")) float InitialDelay = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninja Trial|Trap|Timing") bool bLooping = true;
private:
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> TelegraphMaterial;
	ENinjaFloorTrapState FloorState = ENinjaFloorTrapState::Inactive;
	float StateElapsed = 0.0f;
};
