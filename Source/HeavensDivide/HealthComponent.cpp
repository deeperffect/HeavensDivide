// Copyright Epic Games, Inc. All Rights Reserved.

#include "HealthComponent.h"

#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarHDDebugEnemyDamage(
	TEXT("hd.DebugEnemyDamage"),
	0,
	TEXT("Logs HealthComponent damage attempts, rejections, accepted damage, and invalid health states when enabled."));

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
	LogInvalidHealthState(TEXT("BeginPlay"));
	BroadcastHealthChanged();
}

void UHealthComponent::ApplyDamage(float DamageAmount)
{
	LogInvalidHealthState(TEXT("ApplyDamage:Before"));

	const float PreviousHealth = CurrentHealth;
	if (!bDamageEnabled)
	{
		LogDamageDebug(TEXT("Rejected: damage disabled"), DamageAmount, PreviousHealth, CurrentHealth);
		return;
	}

	if (!FMath::IsFinite(DamageAmount) || DamageAmount <= 0.0f)
	{
		LogDamageDebug(TEXT("Rejected: non-positive or invalid damage"), DamageAmount, PreviousHealth, CurrentHealth);
		return;
	}

	if (IsDead())
	{
		LogDamageDebug(TEXT("Rejected: already dead"), DamageAmount, PreviousHealth, CurrentHealth);
		return;
	}

	CurrentHealth = FMath::Clamp(CurrentHealth - DamageAmount, 0.0f, MaxHealth);
	const float ActualDamage = PreviousHealth - CurrentHealth;

	if (ActualDamage <= 0.0f)
	{
		LogDamageDebug(TEXT("Rejected: no effective health change"), DamageAmount, PreviousHealth, CurrentHealth);
		return;
	}

	LogDamageDebug(TEXT("Accepted"), DamageAmount, PreviousHealth, CurrentHealth);
	OnDamaged.Broadcast(ActualDamage, CurrentHealth);
	BroadcastHealthChanged();

	if (CurrentHealth <= 0.0f && !bDeathBroadcast)
	{
		bDeathBroadcast = true;
		OnDeath.Broadcast();
	}
}

void UHealthComponent::SetDamageEnabled(bool bEnabled)
{
	bDamageEnabled = bEnabled;
}

bool UHealthComponent::IsDamageEnabled() const
{
	return bDamageEnabled;
}

void UHealthComponent::Heal(float HealAmount)
{
	LogInvalidHealthState(TEXT("Heal:Before"));

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
	if (!FMath::IsFinite(NewMaxHealth))
	{
		if (CVarHDDebugEnemyDamage.GetValueOnGameThread() != 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("[EnemyDamage] Invalid NewMaxHealth %.3f on %s. Keeping previous MaxHealth %.3f."),
				NewMaxHealth,
				*GetNameSafe(GetOwner()),
				MaxHealth);
		}
		NewMaxHealth = MaxHealth;
	}

	NewMaxHealth = FMath::Max(0.0f, NewMaxHealth);
	const float PreviousHealthPercent = GetHealthPercent();
	MaxHealth = NewMaxHealth;
	CurrentHealth = MaxHealth * PreviousHealthPercent;
	bDeathBroadcast = CurrentHealth <= 0.0f;
	LogInvalidHealthState(TEXT("SetMaxHealthPreservePercent"));
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

void UHealthComponent::LogDamageDebug(const TCHAR* Reason, float DamageAmount, float PreviousHealth, float NewHealth) const
{
	if (CVarHDDebugEnemyDamage.GetValueOnGameThread() == 0)
	{
		return;
	}

	const AActor* OwnerActor = GetOwner();
	UE_LOG(LogTemp, Log, TEXT("[EnemyDamage] %s Owner=%s Class=%s Damage=%.3f Health %.3f -> %.3f Max=%.3f IsDead=%s DeathBroadcast=%s CanBeDamaged=%s Collision=%s"),
		Reason,
		*GetNameSafe(OwnerActor),
		OwnerActor ? *GetNameSafe(OwnerActor->GetClass()) : TEXT("None"),
		DamageAmount,
		PreviousHealth,
		NewHealth,
		MaxHealth,
		IsDead() ? TEXT("true") : TEXT("false"),
		bDeathBroadcast ? TEXT("true") : TEXT("false"),
		OwnerActor && OwnerActor->CanBeDamaged() ? TEXT("true") : TEXT("false"),
		OwnerActor && OwnerActor->GetActorEnableCollision() ? TEXT("enabled") : TEXT("disabled"));
}

void UHealthComponent::LogInvalidHealthState(const TCHAR* Context) const
{
	const bool bInvalidCurrent = !FMath::IsFinite(CurrentHealth) || CurrentHealth < 0.0f;
	const bool bInvalidMax = !FMath::IsFinite(MaxHealth) || MaxHealth < 0.0f;
	const bool bCurrentAboveMax = FMath::IsFinite(CurrentHealth) && FMath::IsFinite(MaxHealth) && CurrentHealth > MaxHealth + KINDA_SMALL_NUMBER;
	if (!bInvalidCurrent && !bInvalidMax && !bCurrentAboveMax)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[EnemyDamage] Invalid health state Context=%s Owner=%s Current=%.3f Max=%.3f DeathBroadcast=%s"),
		Context,
		*GetNameSafe(GetOwner()),
		CurrentHealth,
		MaxHealth,
		bDeathBroadcast ? TEXT("true") : TEXT("false"));
}
