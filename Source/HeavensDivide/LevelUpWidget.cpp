// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelUpWidget.h"

#include "PlayerUpgradeComponent.h"
#include "SurvivorPlayerController.h"

void ULevelUpWidget::InitializeLevelUpWidget(ASurvivorPlayerController* InPlayerController)
{
	SurvivorPlayerController = InPlayerController;
	PlayerUpgrades = SurvivorPlayerController ? SurvivorPlayerController->GetPlayerUpgrades() : nullptr;
	bCategoryChoiceCommitted = false;
	bUpgradeChoiceCommitted = false;
	RefreshCategoryChoices();
}

void ULevelUpWidget::InitializeDirectUpgradeWidget(ASurvivorPlayerController* InPlayerController)
{
	SurvivorPlayerController = InPlayerController;
	PlayerUpgrades = SurvivorPlayerController ? SurvivorPlayerController->GetPlayerUpgrades() : nullptr;
	bCategoryChoiceCommitted = true;
	bUpgradeChoiceCommitted = false;
	ShowUpgradeChoices(PlayerUpgrades ? PlayerUpgrades->GetCurrentUpgradeChoices() : TArray<UUpgradeDefinition*>());
}

void ULevelUpWidget::RefreshCategoryChoices()
{
	if (!PlayerUpgrades)
	{
		ShowCategoryChoices(TArray<EUpgradeCategory>());
		return;
	}

	ShowCategoryChoices(PlayerUpgrades->GetCurrentCategoryChoices());
}

bool ULevelUpWidget::SelectCategoryChoice(int32 ChoiceIndex)
{
	if (bCategoryChoiceCommitted)
	{
		UE_LOG(LogTemp, Warning, TEXT("LevelUpWidget category selection ignored: category already selected for this offer."));
		return false;
	}

	if (!PlayerUpgrades)
	{
		return false;
	}

	const TArray<EUpgradeCategory> CategoryChoices = PlayerUpgrades->GetCurrentCategoryChoices();
	if (!CategoryChoices.IsValidIndex(ChoiceIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("LevelUpWidget category index rejected: %d"), ChoiceIndex);
		return false;
	}

	const EUpgradeCategory SelectedCategory = CategoryChoices[ChoiceIndex];
	if (!PlayerUpgrades->SelectCategory(SelectedCategory, 3))
	{
		return false;
	}

	bCategoryChoiceCommitted = true;
	bUpgradeChoiceCommitted = false;

	const TArray<UUpgradeDefinition*> UpgradeChoices = PlayerUpgrades->GetCurrentUpgradeChoices();
	UE_LOG(LogTemp, Log, TEXT("UPGRADE OFFER:"));
	for (const UUpgradeDefinition* Upgrade : UpgradeChoices)
	{
		UE_LOG(LogTemp, Log, TEXT("  %s"), Upgrade ? *Upgrade->DisplayName.ToString() : TEXT("None"));
	}

	ShowUpgradeChoices(UpgradeChoices);
	return UpgradeChoices.Num() > 0;
}

bool ULevelUpWidget::SelectUpgradeChoice(int32 ChoiceIndex)
{
	if (bUpgradeChoiceCommitted)
	{
		UE_LOG(LogTemp, Warning, TEXT("LevelUpWidget upgrade selection ignored: upgrade already selected for this offer."));
		return false;
	}

	if (!PlayerUpgrades)
	{
		return false;
	}

	const TArray<UUpgradeDefinition*> UpgradeChoices = PlayerUpgrades->GetCurrentUpgradeChoices();
	if (!UpgradeChoices.IsValidIndex(ChoiceIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("LevelUpWidget upgrade index rejected: %d"), ChoiceIndex);
		return false;
	}

	UUpgradeDefinition* SelectedUpgrade = UpgradeChoices[ChoiceIndex];
	if (!PlayerUpgrades->SelectUpgrade(SelectedUpgrade))
	{
		return false;
	}

	bUpgradeChoiceCommitted = true;

	UE_LOG(LogTemp, Log, TEXT("UPGRADE SELECTED: %s"), SelectedUpgrade ? *SelectedUpgrade->DisplayName.ToString() : TEXT("None"));
	UE_LOG(LogTemp, Log, TEXT("New upgrade level = %d"), PlayerUpgrades->GetUpgradeLevel(SelectedUpgrade));
	OnSelectionCompleted.Broadcast();
	return true;
}

int32 ULevelUpWidget::GetUpgradeLevel(UUpgradeDefinition* Upgrade) const
{
	return PlayerUpgrades ? PlayerUpgrades->GetUpgradeLevel(Upgrade) : 0;
}

UPlayerUpgradeComponent* ULevelUpWidget::GetPlayerUpgrades() const
{
	return PlayerUpgrades;
}
