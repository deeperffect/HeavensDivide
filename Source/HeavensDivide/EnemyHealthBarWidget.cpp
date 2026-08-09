// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemyHealthBarWidget.h"

#include "HealthComponent.h"

void UEnemyHealthBarWidget::InitializeFromHealthComponent(UHealthComponent* InHealthComponent)
{
	if (HealthComponent == InHealthComponent)
	{
		if (HealthComponent)
		{
			UpdateHealthBar(
				HealthComponent->GetCurrentHealth(),
				HealthComponent->GetMaxHealth(),
				HealthComponent->GetHealthPercent());
		}
		return;
	}

	if (HealthComponent)
	{
		HealthComponent->OnHealthChanged.RemoveDynamic(this, &UEnemyHealthBarWidget::HandleHealthChanged);
	}

	HealthComponent = InHealthComponent;
	if (!HealthComponent)
	{
		UpdateHealthBar(0.0f, 0.0f, 0.0f);
		return;
	}

	HealthComponent->OnHealthChanged.AddUniqueDynamic(this, &UEnemyHealthBarWidget::HandleHealthChanged);
	UpdateHealthBar(
		HealthComponent->GetCurrentHealth(),
		HealthComponent->GetMaxHealth(),
		HealthComponent->GetHealthPercent());
}

void UEnemyHealthBarWidget::UpdateHealthBar(float NewCurrentHealth, float NewMaxHealth, float NewHealthPercent)
{
	CurrentHealth = NewCurrentHealth;
	MaxHealth = NewMaxHealth;
	HealthPercent = NewHealthPercent;

	HealthBarUpdated.Broadcast(CurrentHealth, MaxHealth, HealthPercent);
	OnHealthBarUpdated(CurrentHealth, MaxHealth, HealthPercent);
}

float UEnemyHealthBarWidget::GetCurrentHealth() const
{
	return CurrentHealth;
}

float UEnemyHealthBarWidget::GetMaxHealth() const
{
	return MaxHealth;
}

float UEnemyHealthBarWidget::GetHealthPercent() const
{
	return HealthPercent;
}

void UEnemyHealthBarWidget::NativeDestruct()
{
	if (HealthComponent)
	{
		HealthComponent->OnHealthChanged.RemoveDynamic(this, &UEnemyHealthBarWidget::HandleHealthChanged);
	}

	Super::NativeDestruct();
}

void UEnemyHealthBarWidget::HandleHealthChanged(float NewCurrentHealth, float NewMaxHealth, float NewHealthPercent)
{
	UpdateHealthBar(NewCurrentHealth, NewMaxHealth, NewHealthPercent);
}
