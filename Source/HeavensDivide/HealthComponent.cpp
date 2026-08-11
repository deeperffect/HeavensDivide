// Copyright Epic Games, Inc. All Rights Reserved.

#include "HealthComponent.h"

UHealthComponent::UHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	MaxHealth = FMath::Max(0.0f, MaxHealth);
	CurrentHealth = MaxHealth;
	bDeathBroadcast = CurrentHealth <= 0.0f;
	BroadcastHealthChanged();
}

void UHealthComponent::ApplyDamage(float DamageAmount)
{
	if (DamageAmount <= 0.0f || IsDead())
	{
		return;
	}

	const float PreviousHealth = CurrentHealth;
	CurrentHealth = FMath::Clamp(CurrentHealth - DamageAmount, 0.0f, MaxHealth);
	const float ActualDamage = PreviousHealth - CurrentHealth;

	if (ActualDamage <= 0.0f)
	{
		return;
	}

	OnDamaged.Broadcast(ActualDamage, CurrentHealth);
	BroadcastHealthChanged();

	if (CurrentHealth <= 0.0f && !bDeathBroadcast)
	{
		bDeathBroadcast = true;
		OnDeath.Broadcast();
	}
}

void UHealthComponent::Heal(float HealAmount)
{
	if (HealAmount <= 0.0f || IsDead())
	{
		return;
	}

	const float PreviousHealth = CurrentHealth;
	CurrentHealth = FMath::Clamp(CurrentHealth + HealAmount, 0.0f, MaxHealth);

	if (!FMath::IsNearlyEqual(CurrentHealth, PreviousHealth))
	{
		BroadcastHealthChanged();
	}
}

void UHealthComponent::SetMaxHealthPreservePercent(float NewMaxHealth)
{
	NewMaxHealth = FMath::Max(0.0f, NewMaxHealth);
	const float PreviousHealthPercent = GetHealthPercent();
	MaxHealth = NewMaxHealth;
	CurrentHealth = MaxHealth * PreviousHealthPercent;
	bDeathBroadcast = CurrentHealth <= 0.0f;
	BroadcastHealthChanged();
}

bool UHealthComponent::IsDead() const
{
	return CurrentHealth <= 0.0f;
}

float UHealthComponent::GetCurrentHealth() const
{
	return CurrentHealth;
}

float UHealthComponent::GetMaxHealth() const
{
	return MaxHealth;
}

float UHealthComponent::GetHealthPercent() const
{
	return MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f;
}

void UHealthComponent::BroadcastHealthChanged()
{
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth, GetHealthPercent());
}
