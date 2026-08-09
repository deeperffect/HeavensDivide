// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EnemyHealthBarWidget.generated.h"

class UHealthComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FEnemyHealthBarUpdated, float, CurrentHealth, float, MaxHealth, float, HealthPercent);

UCLASS(BlueprintType, Blueprintable)
class HEAVENSDIVIDE_API UEnemyHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Enemy Health Bar")
	void InitializeFromHealthComponent(UHealthComponent* InHealthComponent);

	UFUNCTION(BlueprintCallable, Category = "Enemy Health Bar")
	void UpdateHealthBar(float NewCurrentHealth, float NewMaxHealth, float NewHealthPercent);

	UFUNCTION(BlueprintPure, Category = "Enemy Health Bar")
	float GetCurrentHealth() const;

	UFUNCTION(BlueprintPure, Category = "Enemy Health Bar")
	float GetMaxHealth() const;

	UFUNCTION(BlueprintPure, Category = "Enemy Health Bar")
	float GetHealthPercent() const;

	UFUNCTION(BlueprintImplementableEvent, Category = "Enemy Health Bar")
	void OnHealthBarUpdated(float NewCurrentHealth, float NewMaxHealth, float NewHealthPercent);

	UPROPERTY(BlueprintAssignable, Category = "Enemy Health Bar")
	FEnemyHealthBarUpdated HealthBarUpdated;

protected:
	virtual void NativeDestruct() override;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Enemy Health Bar")
	TObjectPtr<UHealthComponent> HealthComponent;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Enemy Health Bar")
	float CurrentHealth = 0.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Enemy Health Bar")
	float MaxHealth = 0.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Enemy Health Bar")
	float HealthPercent = 0.0f;

private:
	UFUNCTION()
	void HandleHealthChanged(float NewCurrentHealth, float NewMaxHealth, float NewHealthPercent);
};
