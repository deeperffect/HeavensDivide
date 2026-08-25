// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SamuraiBladeWave.generated.h"

class ASamuraiCharacter;
class UBoxComponent;
class UProjectileMovementComponent;
class UStaticMeshComponent;
class UPlayerUpgradeComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBladeWaveEvent, ASamuraiBladeWave*, BladeWave);

UCLASS(Blueprintable)
class HEAVENSDIVIDE_API ASamuraiBladeWave : public AActor
{
	GENERATED_BODY()
public:
	ASamuraiBladeWave();
	void InitializeBladeWave(ASamuraiCharacter* InSamurai, UPlayerUpgradeComponent* InUpgrades, FVector Direction,
		float InDamage, float InWidth, float InTravelDistance, float InSpeed, bool bInReturns);

	UPROPERTY(BlueprintAssignable, Category="Blade Wave|Events") FBladeWaveEvent OnOutboundStarted;
	UPROPERTY(BlueprintAssignable, Category="Blade Wave|Events") FBladeWaveEvent OnReturnStarted;
	UPROPERTY(BlueprintAssignable, Category="Blade Wave|Events") FBladeWaveEvent OnWaveFinished;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Blade Wave") TObjectPtr<UBoxComponent> Collision;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Blade Wave") TObjectPtr<UStaticMeshComponent> Visual;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Blade Wave") TObjectPtr<UProjectileMovementComponent> Movement;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blade Wave") float WaveThickness = 55.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blade Wave") float WaveHeight = 120.0f;
private:
	UFUNCTION() void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	void BeginReturn();
	void FinishWave();
	TWeakObjectPtr<ASamuraiCharacter> SourceSamurai;
	TWeakObjectPtr<UPlayerUpgradeComponent> SourceUpgrades;
	TSet<TObjectPtr<AActor>> HitThisPhase;
	float Damage = 0.0f;
	float Speed = 0.0f;
	bool bReturns = false;
	bool bReturning = false;
	FTimerHandle PhaseTimer;
};
