#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HealingPickup.generated.h"

class UNiagaraSystem;
class UMaterialInterface;
class USceneComponent;
class USoundBase;
class USphereComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable)
class HEAVENSDIVIDE_API AHealingPickup : public AActor
{
	GENERATED_BODY()

public:
	AHealingPickup();
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Healing Pickup|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Healing Pickup|Components")
	TObjectPtr<UStaticMeshComponent> PickupMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Healing Pickup|Components")
	TObjectPtr<USphereComponent> PickupCollision;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Healing Pickup", meta=(ClampMin="0.0"))
	float HealAmount = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Healing Pickup|Effects")
	TObjectPtr<UNiagaraSystem> PickupBurstFX;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Healing Pickup|Effects")
	TObjectPtr<UMaterialInterface> HealOverlayMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Healing Pickup|Effects", meta=(ClampMin="0.0", Units="s"))
	float HealOverlayDuration = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Healing Pickup|Effects")
	TObjectPtr<USoundBase> HealingSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Healing Pickup", meta=(ClampMin="1.0", Units="cm"))
	float PickupCollisionRadius = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Healing Pickup|Idle")
	bool bRotate = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Healing Pickup|Idle", meta=(EditCondition="bRotate", Units="deg/s"))
	float RotationRate = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Healing Pickup|Idle")
	bool bEnableBobbing = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Healing Pickup|Idle", meta=(EditCondition="bEnableBobbing", ClampMin="0.0", Units="cm"))
	float BobAmplitude = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Healing Pickup|Idle", meta=(EditCondition="bEnableBobbing", ClampMin="0.0", Units="Hz"))
	float BobFrequency = 0.6f;

private:
	UFUNCTION()
	void HandlePickupOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);

	bool bConsumed = false;
	float IdleElapsed = 0.0f;
	FVector InitialMeshRelativeLocation = FVector::ZeroVector;
};
