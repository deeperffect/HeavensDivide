#pragma once

#include "CoreMinimal.h"
#include "TankMeleeEnemyBase.h"
#include "GoblinBombEnemy.generated.h"

class UNiagaraSystem;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/** Uses the shared slam telegraph/damage, with a timed fuse and one-shot detonation. */
UCLASS(Blueprintable)
class HEAVENSDIVIDE_API AGoblinBombEnemy : public ATankMeleeEnemyBase
{
	GENERATED_BODY()
	friend class FGoblinBombAttackTest;
public:
	AGoblinBombEnemy(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack|Explosion", meta=(ToolTip="One-shot explosion played only when the charge completes. Killing the enemy early does not explode."))
	TObjectPtr<UNiagaraSystem> ExplosionNiagaraSystem;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack|Explosion", meta=(ToolTip="World-space offset from the enemy location. Adjust Z to place the explosion at ground height."))
	FVector ExplosionSpawnOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack|Charge", meta=(ClampMin="0"))
	float ChargeWobbleDegrees = 7.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack|Charge", meta=(ClampMin="0"))
	float ChargeWobbleFrequency = 7.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack|Charge")
	FLinearColor BombFlashColor = FLinearColor(3.0f, 0.0f, 0.0f);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attack|Charge")
	TObjectPtr<UMaterialInterface> BombFlashMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack|Charge", meta=(ClampMin="0.1", ClampMax="12"))
	float BombFlashStartFrequency = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack|Charge", meta=(ClampMin="0.1", ClampMax="12"))
	float BombFlashEndFrequency = 8.0f;
	/** Matches the attached static mesh component name or component tag. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack|Charge")
	FName BombMeshName = TEXT("Bomb");

protected:
	virtual void StartAttack() override;
	virtual void StopEnemyBehavior() override;
	virtual bool UsesContactDamage() const override { return false; }
	// The fuse owns impact timing. Ignore animation notifies or external hit calls.
	virtual void ExecuteAttackHit() override {}
	virtual void BeginDeathPresentation_Implementation() override;

private:
	void Detonate();
	void StartChargePresentation();
	void UpdateChargePresentation();
	void StopChargePresentation();
	FTimerHandle ChargePresentationTimer;
	double ChargeStartTime = 0.0;
	FQuat PreChargeMeshRotation = FQuat::Identity;
	bool bChargePresentationActive = false;
	bool bPreChargePauseAnims = false;
	bool bBombFlashOn = false;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> ChargeBombMesh;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> BombFlashMID;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInterface>> PreChargeBombMaterials;
	FTimerHandle DetonationTimer;
	bool bDetonated = false;
};
