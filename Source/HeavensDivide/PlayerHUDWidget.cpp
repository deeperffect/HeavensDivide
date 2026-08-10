// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlayerHUDWidget.h"

#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "ExperienceComponent.h"
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
	UnbindPlayerExperience();

	if (CharacterManager)
	{
		CharacterManager->OnCharacterSwapped.RemoveDynamic(this, &UPlayerHUDWidget::HandleCharacterSwapped);
	}

	SurvivorPlayerController = InPlayerController;
	CharacterManager = SurvivorPlayerController->GetCharacterManager();
	Samurai = CharacterManager ? CharacterManager->GetSamurai() : nullptr;
	Ninja = CharacterManager ? CharacterManager->GetNinja() : nullptr;
	PlayerHealth = SurvivorPlayerController->GetPlayerHealthComponent();
	PlayerExperience = SurvivorPlayerController->GetExperienceComponent();

	if (CharacterManager)
	{
		CharacterManager->OnCharacterSwapped.AddUniqueDynamic(this, &UPlayerHUDWidget::HandleCharacterSwapped);
	}

	BindCharacterHealth();
	BindPlayerExperience();
	BroadcastInitialState();
}

void UPlayerHUDWidget::NativeDestruct()
{
	StopHealthChipChase();
	UnbindCharacterHealth();
	UnbindPlayerExperience();

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

int32 UPlayerHUDWidget::GetCurrentXP() const
{
	return PlayerExperience ? PlayerExperience->GetCurrentXP() : 0;
}

int32 UPlayerHUDWidget::GetCurrentLevel() const
{
	return PlayerExperience ? PlayerExperience->GetCurrentLevel() : 1;
}

int32 UPlayerHUDWidget::GetXPToNextLevel() const
{
	return PlayerExperience ? PlayerExperience->GetXPToNextLevel() : 0;
}

float UPlayerHUDWidget::GetXPPercent() const
{
	return PlayerExperience ? PlayerExperience->GetXPPercent() : 0.0f;
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

void UPlayerHUDWidget::HandlePlayerXPChanged(int32 CurrentXP, int32 XPToNextLevel, float XPPercent)
{
	PlayerXPUpdated.Broadcast(CurrentXP, XPToNextLevel, XPPercent);
	OnPlayerXPUpdated(CurrentXP, XPToNextLevel, XPPercent);
}

void UPlayerHUDWidget::HandlePlayerLevelUp(int32 NewLevel)
{
	PlayerLevelUp.Broadcast(NewLevel);
	OnPlayerLevelUp(NewLevel);
	PlayerLevelUpdated.Broadcast(NewLevel);
	OnPlayerLevelUpdated(NewLevel);
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

void UPlayerHUDWidget::BindPlayerExperience()
{
	if (PlayerExperience)
	{
		PlayerExperience->OnXPChanged.AddUniqueDynamic(this, &UPlayerHUDWidget::HandlePlayerXPChanged);
		PlayerExperience->OnLevelUp.AddUniqueDynamic(this, &UPlayerHUDWidget::HandlePlayerLevelUp);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("PlayerHUDWidget: shared ExperienceComponent invalid."));
	}
}

void UPlayerHUDWidget::UnbindPlayerExperience()
{
	if (PlayerExperience)
	{
		PlayerExperience->OnXPChanged.RemoveDynamic(this, &UPlayerHUDWidget::HandlePlayerXPChanged);
		PlayerExperience->OnLevelUp.RemoveDynamic(this, &UPlayerHUDWidget::HandlePlayerLevelUp);
	}

	PlayerExperience = nullptr;
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

	if (PlayerExperience)
	{
		PlayerXPUpdated.Broadcast(
			PlayerExperience->GetCurrentXP(),
			PlayerExperience->GetXPToNextLevel(),
			PlayerExperience->GetXPPercent());
		OnPlayerXPUpdated(
			PlayerExperience->GetCurrentXP(),
			PlayerExperience->GetXPToNextLevel(),
			PlayerExperience->GetXPPercent());
		PlayerLevelUpdated.Broadcast(PlayerExperience->GetCurrentLevel());
		OnPlayerLevelUpdated(PlayerExperience->GetCurrentLevel());
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
