// Copyright Epic Games, Inc. All Rights Reserved.

#include "SynergyMetaProgressionSubsystem.h"
#include "MetaSkillTree.h"
#include "SurvivorPlayerController.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "HeavensDivideMetaSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Modules/ModuleManager.h"
#include "UpgradeDefinition.h"

namespace SynergyMetaProgression
{
	static const FString SaveSlotName(TEXT("HeavensDivide_MetaProgression"));
	static constexpr int32 CurrentSaveVersion = 3;
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
#if WITH_DEV_AUTOMATION_TESTS
	FString AutomationSlot;
	if (FParse::Value(FCommandLine::Get(), TEXT("MetaSaveSlot="), AutomationSlot) && AutomationSlot.StartsWith(TEXT("Automation_"))) TestSaveSlot = AutomationSlot;
#endif
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
#if WITH_DEV_AUTOMATION_TESTS
	if (bSimulateSaveFailure) return false;
#endif
	return CurrentSave && UGameplayStatics::SaveGameToSlot(CurrentSave, TestSaveSlot.IsEmpty() ? GetSaveSlotName() : TestSaveSlot, GetSaveUserIndex());
}

void USynergyMetaProgressionSubsystem::LoadMetaProgression()
{
	CurrentSave = Cast<UHeavensDivideMetaSaveGame>(UGameplayStatics::LoadGameFromSlot(TestSaveSlot.IsEmpty() ? GetSaveSlotName() : TestSaveSlot, GetSaveUserIndex()));
	if (!CurrentSave)
	{
		CreateFreshSave();
		SaveMetaProgression();
		return;
	}

	MetaSkillTree::Sanitize(*CurrentSave);
	RefreshSkillBonuses();
	const bool bMigrated = CurrentSave->SaveVersion != SynergyMetaProgression::CurrentSaveVersion;
	CurrentSave->SaveVersion = SynergyMetaProgression::CurrentSaveVersion;
	if (AddDefaultUnlocks() || bMigrated)
	{
		SaveMetaProgression();
	}
}

bool USynergyMetaProgressionSubsystem::ResetMetaProgression()
{
	const auto PreviousSave = CurrentSave;
	CreateFreshSave();
	if (!SaveMetaProgression())
	{
		CurrentSave = PreviousSave;
		RefreshSkillBonuses();
		return false;
	}
	PendingSkillReward = LastSkillRunReward = 0;
	return true;
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
		RefreshSkillBonuses();
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

namespace
{
 bool SkillChangesAllowed(const USynergyMetaProgressionSubsystem* Meta)
 {
  // Purchases only between runs, including Blueprint callers.
  const UWorld* World = Meta->GetWorld();
  return !World || !Cast<ASurvivorPlayerController>(World->GetFirstPlayerController());
 }
}
int32 USynergyMetaProgressionSubsystem::GetSoulEmbers() const { return CurrentSave ? CurrentSave->SoulEmbers : 0; }
int32 USynergyMetaProgressionSubsystem::GetSkillRank(FName Id) const { return CurrentSave ? MetaSkillTree::Rank(*CurrentSave, Id) : 0; }
int32 USynergyMetaProgressionSubsystem::GetSkillCost(FName Id) const { return CurrentSave ? MetaSkillTree::Cost(*CurrentSave, Id) : 0; }
float USynergyMetaProgressionSubsystem::GetSkillBonus(FName Effect) const { return CachedSkillBonuses.FindRef(Effect); }
FString USynergyMetaProgressionSubsystem::GetSkillPurchaseBlock(FName Id) const
{
 if (!SkillChangesAllowed(this)) return TEXT("Return to the main menu to learn skills.");
 return CurrentSave ? MetaSkillTree::PurchaseBlock(*CurrentSave, Id) : TEXT("Progression unavailable.");
}
bool USynergyMetaProgressionSubsystem::PurchaseSkill(FName Id)
{
 if (!GetSkillPurchaseBlock(Id).IsEmpty()) return false;
 const auto OldRanks = CurrentSave->SkillRanks;
 const int32 OldWallet = CurrentSave->SoulEmbers, OldSpent = CurrentSave->SkillEmbersSpent;
 if (!MetaSkillTree::Purchase(*CurrentSave, Id)) return false;
 if (SaveMetaProgression()) { RefreshSkillBonuses(); return true; }
 CurrentSave->SkillRanks = OldRanks; CurrentSave->SoulEmbers = OldWallet; CurrentSave->SkillEmbersSpent = OldSpent;
 return false;
}
bool USynergyMetaProgressionSubsystem::RefundSkills()
{
 if (!CurrentSave || !SkillChangesAllowed(this)) return false;
 const auto OldRanks = CurrentSave->SkillRanks;
 const int32 OldWallet = CurrentSave->SoulEmbers, OldSpent = CurrentSave->SkillEmbersSpent;
 MetaSkillTree::Refund(*CurrentSave);
 if (SaveMetaProgression()) { RefreshSkillBonuses(); return true; }
 CurrentSave->SkillRanks = OldRanks; CurrentSave->SoulEmbers = OldWallet; CurrentSave->SkillEmbersSpent = OldSpent;
 return false;
}
bool USynergyMetaProgressionSubsystem::AwardSkillRun(float Seconds, bool bVictory)
{
 LastSkillRunReward = MetaSkillTree::RunReward(Seconds, bVictory);
 PendingSkillReward = static_cast<int32>(FMath::Min<int64>(MAX_int32, int64(PendingSkillReward) + LastSkillRunReward));
 RetrySkillReward();
 return PendingSkillReward == 0;
}
void USynergyMetaProgressionSubsystem::RetrySkillReward()
{
 if (PendingSkillReward <= 0 || !CurrentSave) return;
 const int32 OldWallet = CurrentSave->SoulEmbers;
 CurrentSave->SoulEmbers = static_cast<int32>(FMath::Min<int64>(MAX_int32, int64(OldWallet) + PendingSkillReward));
 if (SaveMetaProgression()) PendingSkillReward = 0;
 else CurrentSave->SoulEmbers = OldWallet;
}

void USynergyMetaProgressionSubsystem::RefreshSkillBonuses()
{
 CachedSkillBonuses.Reset();
 if (!CurrentSave) return;
 for (const FMetaSkillNode& N : MetaSkillTree::Nodes()) CachedSkillBonuses.FindOrAdd(N.Effect) += MetaSkillTree::Rank(*CurrentSave, N.Id) * N.PerRank;
}
