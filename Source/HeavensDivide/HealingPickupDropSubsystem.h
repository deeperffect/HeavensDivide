#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HealingPickupDropSubsystem.generated.h"

class AEnemyBase;
class AHealingPickup;

UCLASS()
class HEAVENSDIVIDE_API UHealingPickupDropSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	void NotifyEnemyDied(AEnemyBase* Enemy);

private:
	UFUNCTION()
	void HandlePickupDestroyed(AActor* DestroyedActor);

	void PruneActivePickups();
	float GetBaseDropChance(const AEnemyBase* Enemy) const;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AHealingPickup>> ActivePickups;

	float LastSuccessfulSpawnTime = -1.0f;
	float PityStartTime = 0.0f;
};
