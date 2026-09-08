#pragma once

#include "CoreMinimal.h"
#include "MeleeEnemyBase.h"
#include "MontageMeleeEnemyBase.generated.h"

class UAnimMontage;

/** Animation-driven melee support retained only for special enemies. */
UCLASS(Blueprintable)
class HEAVENSDIVIDE_API AMontageMeleeEnemyBase : public AMeleeEnemyBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="Enemy|Attack")
	virtual void PerformAttackHit();
protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack")
	TObjectPtr<UAnimMontage> AttackMontage;
	virtual void StartAttack() override;
	virtual float GetChaseStopDistance() const override { return StopDistance; }
	void HandleAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);
};
