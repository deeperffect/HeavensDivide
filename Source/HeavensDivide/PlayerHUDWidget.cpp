// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlayerHUDWidget.h"

#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "HealthComponent.h"
#include "NinjaCharacter.h"
#include "SamuraiCharacter.h"
#include "SurvivorPlayerController.h"
#include "TimerManager.h"

void UPlayerHUDWidget::InitializeFromCharacterManager(UCharacterManagerComponent* InCharacterManager)
{
	InitializeFromPlayerController(InCharacterManager ? Cast<ASurvivorPlayerController>(InCharacterManager->GetOwner()) : nullptr);
}

void UPlayerHUDWidget::InitializeFromPlayerController(ASurvivorPlayerController* InPlayerController)
{
	if (!InPlayerController)
	{
		UE_LOG(LogTemp, Warning, TEXT("PlayerHUDWidget initialization skipped: PlayerController invalid."));
		return;
	}

	if (SurvivorPlayerController == InPlayerController)
	{
		BroadcastInitialState();
		return;
	}

	UnbindCharacterHealth();

	if (CharacterManager)
	{
		CharacterManager->OnCharacterSwapped.RemoveDynamic(this, &UPlayerHUDWidget::HandleCharacterSwapped);
	}

	SurvivorPlayerController = InPlayerController;
	CharacterManager = SurvivorPlayerController->GetCharacterManager();
	Samurai = CharacterManager ? CharacterManager->GetSamurai() : nullptr;
	Ninja = CharacterManager ? CharacterManager->GetNinja() : nullptr;
	PlayerHealth = SurvivorPlayerController->GetPlayerHealthComponent();

	if (CharacterManager)
	{
		CharacterManager->OnCharacterSwapped.AddUniqueDynamic(this, &UPlayerHUDWidget::HandleCharacterSwapped);
	}

	BindCharacterHealth();
	BroadcastInitialState();
}

void UPlayerHUDWidget::NativeDestruct()
{
	StopHealthChipChase();
	UnbindCharacterHealth();

	if (CharacterManager)
	{
		CharacterManager->OnCharacterSwapped.RemoveDynamic(this, &UPlayerHUDWidget::HandleCharacterSwapped);
	}

	Super::NativeDestruct();
}

void UPlayerHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bHealthChipChasing)
	{
		return;
	}

	DelayedHealthPercent = FMath::FInterpTo(
		DelayedHealthPercent,
		TargetHealthPercent,
		InDeltaTime,
		HealthChipChaseSpeed);

	if (FMath::Abs(DelayedHealthPercent - TargetHealthPercent) <= HealthChipSnapTolerance)
	{
		DelayedHealthPercent = TargetHealthPercent;
		bHealthChipChasing = false;
	}
}

ASamuraiCharacter* UPlayerHUDWidget::GetSamurai() const
{
	return Samurai;
}

ANinjaCharacter* UPlayerHUDWidget::GetNinja() const
{
	return Ninja;
}

ACharacterBase* UPlayerHUDWidget::GetActiveCharacter() const
{
	return CharacterManager ? CharacterManager->GetActiveCharacter() : nullptr;
}

float UPlayerHUDWidget::GetCurrentHealth() const
{
	return PlayerHealth ? PlayerHealth->GetCurrentHealth() : 0.0f;
}

float UPlayerHUDWidget::GetMaxHealth() const
{
	return PlayerHealth ? PlayerHealth->GetMaxHealth() : 0.0f;
}

float UPlayerHUDWidget::GetHealthPercent() const
{
	return PlayerHealth ? PlayerHealth->GetHealthPercent() : 0.0f;
}

float UPlayerHUDWidget::GetDisplayedHealthPercent() const
{
	return DisplayedHealthPercent;
}

float UPlayerHUDWidget::GetDelayedHealthPercent() const
{
	return DelayedHealthPercent;
}

bool UPlayerHUDWidget::IsSamuraiActive() const
{
	return Samurai && GetActiveCharacter() == Samurai;
}

bool UPlayerHUDWidget::IsNinjaActive() const
{
	return Ninja && GetActiveCharacter() == Ninja;
}

void UPlayerHUDWidget::HandlePlayerHealthChanged(float CurrentHealth, float MaxHealth, float HealthPercent)
{
	UpdateHealthAnimation(HealthPercent);
	PlayerHealthUpdated.Broadcast(CurrentHealth, MaxHealth, HealthPercent);
	OnPlayerHealthUpdated(CurrentHealth, MaxHealth, HealthPercent);
}

void UPlayerHUDWidget::HandleCharacterSwapped(ACharacterBase* OldCharacter, ACharacterBase* NewCharacter)
{
	ActiveCharacterChanged.Broadcast(NewCharacter);
	OnActiveCharacterChanged(NewCharacter);
}

void UPlayerHUDWidget::BindCharacterHealth()
{
	if (PlayerHealth)
	{
		PlayerHealth->OnHealthChanged.AddUniqueDynamic(this, &UPlayerHUDWidget::HandlePlayerHealthChanged);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("PlayerHUDWidget: shared player HealthComponent invalid."));
	}
}

void UPlayerHUDWidget::UnbindCharacterHealth()
{
	StopHealthChipChase();

	if (PlayerHealth)
	{
		PlayerHealth->OnHealthChanged.RemoveDynamic(this, &UPlayerHUDWidget::HandlePlayerHealthChanged);
	}

	PlayerHealth = nullptr;
}

void UPlayerHUDWidget::BroadcastInitialState()
{
	if (PlayerHealth)
	{
		const float HealthPercent = PlayerHealth->GetHealthPercent();
		DisplayedHealthPercent = HealthPercent;
		DelayedHealthPercent = HealthPercent;
		TargetHealthPercent = HealthPercent;
		bHealthChipChasing = false;

		PlayerHealthUpdated.Broadcast(
			PlayerHealth->GetCurrentHealth(),
			PlayerHealth->GetMaxHealth(),
			HealthPercent);
		OnPlayerHealthUpdated(
			PlayerHealth->GetCurrentHealth(),
			PlayerHealth->GetMaxHealth(),
			HealthPercent);
	}

	ActiveCharacterChanged.Broadcast(GetActiveCharacter());
	OnActiveCharacterChanged(GetActiveCharacter());
}

void UPlayerHUDWidget::UpdateHealthAnimation(float NewHealthPercent)
{
	NewHealthPercent = FMath::Clamp(NewHealthPercent, 0.0f, 1.0f);

	const float PreviousDisplayedPercent = DisplayedHealthPercent;
	DisplayedHealthPercent = NewHealthPercent;
	TargetHealthPercent = NewHealthPercent;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HealthChipDelayTimerHandle);
	}

	if (NewHealthPercent >= PreviousDisplayedPercent)
	{
		DelayedHealthPercent = NewHealthPercent;
		bHealthChipChasing = false;
		return;
	}

	if (DelayedHealthPercent < PreviousDisplayedPercent)
	{
		DelayedHealthPercent = PreviousDisplayedPercent;
	}

	bHealthChipChasing = false;

	if (HealthChipDelay <= 0.0f)
	{
		bHealthChipChasing = true;
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			HealthChipDelayTimerHandle,
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				bHealthChipChasing = true;
			}),
			HealthChipDelay,
			false);
	}
}

void UPlayerHUDWidget::StopHealthChipChase()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HealthChipDelayTimerHandle);
	}

	bHealthChipChasing = false;
}
