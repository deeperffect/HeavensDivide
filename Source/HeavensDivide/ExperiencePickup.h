// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ExperiencePickup.generated.h"

class ACharacterBase;
class USceneComponent;
class USphereComponent;
class UCharacterManagerComponent;
class UExperienceComponent;
class USharedPlayerStatsComponent;
class USoundBase;

UCLASS(Blueprintable)
class HEAVENSDIVIDE_API AExperiencePickup : public AActor
{
	GENERATED_BODY()

public:
	AExperiencePickup();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "Experience Pickup")
	void InitializePickup(int32 InXPValue, UExperienceComponent* InExperienceComponent, UCharacterManagerComponent* InCharacterManager);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> PickupCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> VisualRoot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Experience Pickup", meta = (ClampMin = "0", UIMin = "0"))
	int32 XPValue = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Experience Pickup", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float PickupRadius = 55.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Experience Pickup", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float AttractionRadius = 350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Experience Pickup", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AttractionSpeed = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Experience Pickup|Audio", meta = (ToolTip = "Fire-and-forget sound played when this pickup reaches the player and is collected."))
	TObjectPtr<USoundBase> PickupSound;

private:
	UFUNCTION()
	void HandlePickupOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleSharedPlayerStatsChanged();

	bool IsActiveCharacter(AActor* Actor) const;
	ACharacterBase* GetActiveCharacter() const;
	void BeginAttraction();
	void Collect();
	void CacheSharedPlayerState();
	void ApplySharedPickupRadiusStats();
	void CheckInitialActiveCharacterProximity();

	UPROPERTY()
	TObjectPtr<UExperienceComponent> ExperienceComponent;

	UPROPERTY()
	TObjectPtr<UCharacterManagerComponent> CharacterManager;

	UPROPERTY()
	TObjectPtr<USharedPlayerStatsComponent> SharedPlayerStats;

	bool bAttracting = false;
	bool bCollected = false;
	float BasePickupRadius = 0.0f;
	float BaseAttractionRadius = 0.0f;
};
