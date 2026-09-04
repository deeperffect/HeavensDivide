// Copyright Epic Games, Inc. All Rights Reserved.

#include "SynergyMetaProgressionSubsystem.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "HeavensDivideMetaSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Modules/ModuleManager.h"
#include "UpgradeDefinition.h"

namespace SynergyMetaProgression
{
	static const FString SaveSlotName(TEXT("HeavensDivide_MetaProgression"));
	static constexpr int32 CurrentSaveVersion = 2;
	static constexpr int32 TwinSoulCompletionsPerDiscovery = 3;
	static const FName DefaultUnlockedIds[] =
	{
		FName(TEXT("Synergy.TagTeam")),
		FName(TEXT("Synergy.Handoff"))
	};
}

void USynergyMetaProgressionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadMetaProgression();
}

bool USynergyMetaProgressionSubsystem::IsSynergyUpgradeUnlocked(FName MetaUnlockId) const
{
	return !MetaUnlockId.IsNone() && CurrentSave && CurrentSave->UnlockedSynergyUpgradeIds.Contains(MetaUnlockId);
}

bool USynergyMetaProgressionSubsystem::UnlockSynergyUpgrade(FName MetaUnlockId)
{
	if (MetaUnlockId.IsNone())
	{
		return false;
	}
	if (!CurrentSave)
	{
		CreateFreshSave();
	}
	if (CurrentSave->UnlockedSynergyUpgradeIds.Contains(MetaUnlockId))
	{
		return false;
	}

	CurrentSave->UnlockedSynergyUpgradeIds.Add(MetaUnlockId);
	SaveMetaProgression();
	return true;
}

bool USynergyMetaProgressionSubsystem::IsUpgradeMetaEligible(const UUpgradeDefinition* Upgrade) const
{
	if (!Upgrade || Upgrade->Category != EUpgradeCategory::Synergy || !Upgrade->bRequiresMetaUnlock)
	{
		return true;
	}
	if (Upgrade->MetaUnlockId.IsNone())
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Warning, TEXT("Meta-locked Synergy upgrade '%s' has no MetaUnlockId and is excluded."), *Upgrade->GetPathName());
#endif
		return false;
	}
	return IsSynergyUpgradeUnlocked(Upgrade->MetaUnlockId);
}

bool USynergyMetaProgressionSubsystem::SaveMetaProgression()
{
	return CurrentSave && UGameplayStatics::SaveGameToSlot(CurrentSave, GetSaveSlotName(), GetSaveUserIndex());
}

void USynergyMetaProgressionSubsystem::LoadMetaProgression()
{
	CurrentSave = Cast<UHeavensDivideMetaSaveGame>(UGameplayStatics::LoadGameFromSlot(GetSaveSlotName(), GetSaveUserIndex()));
	if (!CurrentSave)
	{
		CreateFreshSave();
		SaveMetaProgression();
		return;
	}

	CurrentSave->SaveVersion = SynergyMetaProgression::CurrentSaveVersion;
	if (AddDefaultUnlocks())
	{
		SaveMetaProgression();
	}
}

bool USynergyMetaProgressionSubsystem::ResetMetaProgression()
{
	UGameplayStatics::DeleteGameInSlot(GetSaveSlotName(), GetSaveUserIndex());
	CreateFreshSave();
	return SaveMetaProgression();
}

TArray<FName> USynergyMetaProgressionSubsystem::GetUnlockedSynergyUpgradeIds() const
{
	return CurrentSave ? CurrentSave->UnlockedSynergyUpgradeIds : TArray<FName>();
}

TArray<UUpgradeDefinition*> USynergyMetaProgressionSubsystem::GetSynergyUpgradeDefinitions() const
{
	FARFilter Filter;
	Filter.PackagePaths.Add(TEXT("/Game/HeavensDivide/Upgrades/Synergy"));
	Filter.ClassPaths.Add(UUpgradeDefinition::StaticClass()->GetClassPathName());
	Filter.bRecursivePaths = true;
	Filter.bRecursiveClasses = true;

	TArray<FAssetData> Assets;
	FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().GetAssets(Filter, Assets);
	TArray<UUpgradeDefinition*> Definitions;
	for (const FAssetData& Asset : Assets)
	{
		UUpgradeDefinition* Definition = Cast<UUpgradeDefinition>(Asset.GetAsset());
		if (Definition && Definition->Category == EUpgradeCategory::Synergy && !Definition->MetaUnlockId.IsNone())
		{
			Definitions.Add(Definition);
		}
	}
	Definitions.Sort([](const UUpgradeDefinition& A, const UUpgradeDefinition& B)
	{
		return A.MetaUnlockId.LexicalLess(B.MetaUnlockId);
	});
	return Definitions;
}

TArray<UUpgradeDefinition*> USynergyMetaProgressionSubsystem::GetCollectionUpgradeDefinitions(EUpgradeCategory Category) const
{
	FARFilter Filter;
	Filter.PackagePaths.Add(TEXT("/Game/HeavensDivide/Upgrades"));
	Filter.ClassPaths.Add(UUpgradeDefinition::StaticClass()->GetClassPathName());
	Filter.bRecursivePaths = true;
	Filter.bRecursiveClasses = true;

	TArray<FAssetData> Assets;
	FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().GetAssets(Filter, Assets);
	TArray<UUpgradeDefinition*> Definitions;
	for (const FAssetData& Asset : Assets)
	{
		UUpgradeDefinition* Definition = Cast<UUpgradeDefinition>(Asset.GetAsset());
		if (Definition && Definition->Category == Category)
		{
			Definitions.Add(Definition);
		}
	}
	Definitions.Sort([](const UUpgradeDefinition& A, const UUpgradeDefinition& B)
	{
		return A.DisplayName.ToString() < B.DisplayName.ToString();
	});
	return Definitions;
}

bool USynergyMetaProgressionSubsystem::IsCollectionUpgradeUnlocked(const UUpgradeDefinition* Upgrade) const
{
	if (!Upgrade) return false;
	return Upgrade->Category != EUpgradeCategory::Synergy || IsUpgradeMetaEligible(Upgrade);
}

int32 USynergyMetaProgressionSubsystem::GetTwinSoulDiscoveryProgress() const
{
	return CurrentSave ? FMath::Max(0, CurrentSave->TwinSoulCompletionsTowardDiscovery) : 0;
}

int32 USynergyMetaProgressionSubsystem::GetTwinSoulCompletionsPerDiscovery() const
{
	return SynergyMetaProgression::TwinSoulCompletionsPerDiscovery;
}

bool USynergyMetaProgressionSubsystem::RecordTwinSoulCompletion()
{
	if (!CurrentSave) CreateFreshSave();
	if (!CurrentSave) return false;
	++CurrentSave->TwinSoulCompletionsTowardDiscovery;
	SaveMetaProgression();
	return CurrentSave->TwinSoulCompletionsTowardDiscovery >= SynergyMetaProgression::TwinSoulCompletionsPerDiscovery;
}

void USynergyMetaProgressionSubsystem::ConsumeTwinSoulDiscoveryProgress()
{
	if (!CurrentSave) return;
	CurrentSave->TwinSoulCompletionsTowardDiscovery = FMath::Max(0,
		CurrentSave->TwinSoulCompletionsTowardDiscovery - SynergyMetaProgression::TwinSoulCompletionsPerDiscovery);
	SaveMetaProgression();
}

void USynergyMetaProgressionSubsystem::ResetTwinSoulDiscoveryProgress()
{
	if (!CurrentSave || CurrentSave->TwinSoulCompletionsTowardDiscovery == 0) return;
	CurrentSave->TwinSoulCompletionsTowardDiscovery = 0;
	SaveMetaProgression();
}

const FString& USynergyMetaProgressionSubsystem::GetSaveSlotName()
{
	return SynergyMetaProgression::SaveSlotName;
}

void USynergyMetaProgressionSubsystem::CreateFreshSave()
{
	CurrentSave = Cast<UHeavensDivideMetaSaveGame>(UGameplayStatics::CreateSaveGameObject(UHeavensDivideMetaSaveGame::StaticClass()));
	if (CurrentSave)
	{
		CurrentSave->SaveVersion = SynergyMetaProgression::CurrentSaveVersion;
		AddDefaultUnlocks();
	}
}

bool USynergyMetaProgressionSubsystem::AddDefaultUnlocks()
{
	if (!CurrentSave)
	{
		return false;
	}

	bool bChanged = false;
	for (const FName DefaultId : SynergyMetaProgression::DefaultUnlockedIds)
	{
		if (!CurrentSave->UnlockedSynergyUpgradeIds.Contains(DefaultId))
		{
			CurrentSave->UnlockedSynergyUpgradeIds.Add(DefaultId);
			bChanged = true;
		}
	}
	return bChanged;
}
