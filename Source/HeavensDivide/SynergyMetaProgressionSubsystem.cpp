// Copyright Epic Games, Inc. All Rights Reserved.

#include "SynergyMetaProgressionSubsystem.h"
#include "MetaSkillTree.h"
#include "RunStanceCatalog.h"
#include "RunChallengeSubsystem.h"
#include "RunReportSubsystem.h"
#include "BalanceLabDirector.h"
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
	static constexpr int32 CurrentSaveVersion = 7;
	static constexpr int32 TwinSoulCompletionsPerDiscovery = 3;
	static const FName DefaultUnlockedIds[] =
	{
		FName(TEXT("Synergy.TagTeam"))
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
 RewardSaveTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,
  [this](float) { RetrySkillReward(); return true; }), 5.f);
}

void USynergyMetaProgressionSubsystem::Deinitialize()
{
 FTSTicker::GetCoreTicker().RemoveTicker(RewardSaveTicker);
 RetrySkillReward();
 Super::Deinitialize();
}

bool USynergyMetaProgressionSubsystem::IsSynergyUpgradeUnlocked(FName Id) const
{
 return Id == TEXT("Synergy.TagTeam") || (CurrentSave && CurrentSave->UnlockedSynergyUpgradeIds.Contains(Id));
}

bool USynergyMetaProgressionSubsystem::UnlockSynergyUpgrade(FName Id)
{
 // Retained Blueprint entry point obeys the same paid, atomic shop transaction.
 return PurchaseSynergy(Id);
}

bool USynergyMetaProgressionSubsystem::IsUpgradeMetaEligible(const UUpgradeDefinition* Upgrade) const
{
 if (!Upgrade) return false;
 if (RunStances::Find(Upgrade->UpgradeId)) return IsStanceUnlocked(Upgrade->UpgradeId);
 if (Upgrade->Category == EUpgradeCategory::Synergy)
  return IsSynergyUpgradeUnlocked(Upgrade->MetaUnlockId.IsNone() ? Upgrade->UpgradeId : Upgrade->MetaUnlockId);
 return true;
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
 RetrySkillReward();
 if (HasPendingSkillReward()) return; // Keep unsaved earnings and their fractional carry intact.
	CurrentSave = Cast<UHeavensDivideMetaSaveGame>(UGameplayStatics::LoadGameFromSlot(TestSaveSlot.IsEmpty() ? GetSaveSlotName() : TestSaveSlot, GetSaveUserIndex()));
	if (!CurrentSave)
	{
		CreateFreshSave();
		SaveMetaProgression();
		return;
	}

 // Version 5's blanket availability and retired discovery rewards are replaced by the shop.
 // Preserve Ember balances and invested skill ranks; stance ranks reactivate after purchase.
 if (CurrentSave->SaveVersion < 6)
 {
  CurrentSave->UnlockedSynergyUpgradeIds.Reset();
  CurrentSave->UnlockedStanceIds.Reset();
 }
 // Version 7 replaces solo choices with three shared recipes. Reuse purchases,
 // refund duplicate/retired choices, and keep respec's recorded spend exact.
 if (CurrentSave->SaveVersion < 7) {
  int32 Refund=0;
  const auto MoveChoice=[&](const TCHAR* Old,const TCHAR* New) {
   const FName Previous(*FString::Printf(TEXT("Ability.Combo.%s"),Old));
   const int32 Rank=CurrentSave->SkillRanks.FindRef(Previous);
   if (Rank<=0) return;
   CurrentSave->SkillRanks.Remove(Previous);
   const FName Destination(*FString::Printf(TEXT("Ability.Combo.%s"),New));
   if (!*New || CurrentSave->SkillRanks.FindRef(Destination)>0) Refund+=18;
   else CurrentSave->SkillRanks.Add(Destination,1);
  };
  MoveChoice(TEXT("ReturningStorm"),TEXT("HeavensDivide"));
  MoveChoice(TEXT("ThunderNeedles"),TEXT("HeavensDivide"));
  MoveChoice(TEXT("Starfall"),TEXT("Moonwell"));
  MoveChoice(TEXT("SerpentSeal"),TEXT("BloodMoon"));
  for (const TCHAR* Retired : {TEXT("IronLotus"),TEXT("FallingHeavens"),TEXT("VoidStep")}) MoveChoice(Retired,TEXT(""));
  if (CurrentSave->SkillRanks.FindRef(TEXT("Ability.Combo.BloodMoon"))>0) Refund+=18;
  Refund=FMath::Min(Refund,FMath::Max(0,CurrentSave->SkillEmbersSpent));
  CurrentSave->SkillEmbersSpent-=Refund;
  CurrentSave->SoulEmbers=static_cast<int32>(FMath::Min<int64>(MAX_int32,int64(CurrentSave->SoulEmbers)+Refund));
 }
 CurrentSave->SoulSigils = FMath::Max(0, CurrentSave->SoulSigils);
 EmberFraction = FMath::IsFinite(CurrentSave->EmberFraction) ? FMath::Clamp(CurrentSave->EmberFraction,0.0,0.999999999) : 0.0;
 SigilFraction = FMath::IsFinite(CurrentSave->SigilFraction) ? FMath::Clamp(CurrentSave->SigilFraction,0.0,0.999999999) : 0.0;
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
	PendingSkillReward = PendingSigils = LastSkillRunReward = RunEmbers = RunSigils = 0;
 PendingDifficultyRanks.Reset();
 EmberFraction = SigilFraction = 0;
	return true;
}

TArray<FName> USynergyMetaProgressionSubsystem::GetUnlockedSynergyUpgradeIds() const
{
 return CurrentSave ? CurrentSave->UnlockedSynergyUpgradeIds : TArray<FName>{FName(TEXT("Synergy.TagTeam"))};
}

TArray<UUpgradeDefinition*> USynergyMetaProgressionSubsystem::GetSynergyUpgradeDefinitions() const
{
 KnownSynergyIds.Reset();
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
		if (Definition && Definition->Category == EUpgradeCategory::Synergy && !Definition->UpgradeId.IsNone())
		{
   KnownSynergyIds.Add(Definition->MetaUnlockId.IsNone()?Definition->UpgradeId:Definition->MetaUnlockId);
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
	return IsUpgradeMetaEligible(Upgrade);
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
	// Trials are retired. Keep the API and old save data for compatibility.
	return false;
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
 for (const auto& Stance : RunStances::All)
  if (RunStances::IsDefaultUnlocked(Stance.Id) && !CurrentSave->UnlockedStanceIds.Contains(FName(Stance.Id)))
  {
   CurrentSave->UnlockedStanceIds.Add(FName(Stance.Id));
   bChanged = true;
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
int32 USynergyMetaProgressionSubsystem::GetSoulSigils() const { return CurrentSave ? CurrentSave->SoulSigils : 0; }
bool USynergyMetaProgressionSubsystem::IsStanceUnlocked(FName Id) const
{
 return RunStances::Find(Id) && (RunStances::IsDefaultUnlocked(Id) || (CurrentSave && CurrentSave->UnlockedStanceIds.Contains(Id)));
}
int32 USynergyMetaProgressionSubsystem::GetUnlockedDifficultyRank(FName MapId,FName ModifierId) const
{
 if(!RunReports::FindMap(MapId) || !RunChallenges::Modifiers().ContainsByPredicate([ModifierId](const auto& M){return M.Id==ModifierId;}))return 0;
 const FName Key(*(MapId.ToString()+TEXT(".")+ModifierId.ToString()));
 return CurrentSave?FMath::Clamp(CurrentSave->UnlockedDifficultyRanks.FindRef(Key),0,5):0;
}
void USynergyMetaProgressionSubsystem::RecordDifficultyVictory(FName MapId,const TMap<FName,int32>& CompletedRanks)
{
 if(!CurrentSave || !RunReports::FindMap(MapId))return;
 for(const auto& Modifier:RunChallenges::Modifiers())
 {
  const auto* Rank=CompletedRanks.Find(Modifier.Id);
  if(!Rank || *Rank<0 || *Rank>GetUnlockedDifficultyRank(MapId,Modifier.Id))return;
 }
 for(const auto& Modifier:RunChallenges::Modifiers())
 {
  const FName Key(*(MapId.ToString()+TEXT(".")+Modifier.Id.ToString()));
  const int32 Next=FMath::Min(5,CompletedRanks.FindRef(Modifier.Id)+1);
  if(Next>GetUnlockedDifficultyRank(MapId,Modifier.Id))PendingDifficultyRanks.FindOrAdd(Key)=FMath::Max(PendingDifficultyRanks.FindRef(Key),Next);
 }
 RetrySkillReward();
}
int32 USynergyMetaProgressionSubsystem::GetStanceCost(FName Id) const
{
 return RunStances::Find(Id) && !IsStanceUnlocked(Id) ? 18 : 0;
}
FString USynergyMetaProgressionSubsystem::GetStancePurchaseBlock(FName Id) const
{
 if (!SkillChangesAllowed(this)) return TEXT("Return to the main menu to unlock stances.");
 if (!CurrentSave || !RunStances::Find(Id)) return TEXT("Unknown stance.");
 if (IsStanceUnlocked(Id)) return TEXT("Stance unlocked.");
 if (GetSoulSigils() < GetStanceCost(Id)) return TEXT("Earn Soul Sigils from elites, chests, objectives and bosses.");
 return {};
}
bool USynergyMetaProgressionSubsystem::PurchaseStance(FName Id)
{
 RetrySkillReward();
 if (!GetStancePurchaseBlock(Id).IsEmpty()) return false;
 const int32 Price = GetStanceCost(Id);
 CurrentSave->SoulSigils -= Price;
 CurrentSave->UnlockedStanceIds.Add(Id);
 if (SaveMetaProgression()) { RefreshSkillBonuses(); return true; }
 CurrentSave->SoulSigils += Price;
 CurrentSave->UnlockedStanceIds.Remove(Id);
 return false;
}
int32 USynergyMetaProgressionSubsystem::GetSynergyCost(FName Id) const
{
 return !Id.IsNone() && !IsSynergyUpgradeUnlocked(Id) ? 12 : 0;
}
FString USynergyMetaProgressionSubsystem::GetSynergyPurchaseBlock(FName Id) const
{
 if (!SkillChangesAllowed(this)) return TEXT("Return to the main menu to unlock synergies.");
 if (!CurrentSave || Id.IsNone()) return TEXT("Unknown synergy.");
 if (IsSynergyUpgradeUnlocked(Id)) return TEXT("Synergy unlocked.");
 if (KnownSynergyIds.IsEmpty()) GetSynergyUpgradeDefinitions();
 if (!KnownSynergyIds.Contains(Id)) return TEXT("Unknown synergy.");
 if (GetSoulSigils() < GetSynergyCost(Id)) return TEXT("Earn Soul Sigils from elites, chests, objectives and bosses.");
 return {};
}
bool USynergyMetaProgressionSubsystem::PurchaseSynergy(FName Id)
{
 RetrySkillReward();
 if (!GetSynergyPurchaseBlock(Id).IsEmpty()) return false;
 const int32 Price = GetSynergyCost(Id);
 CurrentSave->SoulSigils -= Price;
 CurrentSave->UnlockedSynergyUpgradeIds.Add(Id);
 if (SaveMetaProgression()) return true;
 CurrentSave->SoulSigils += Price;
 CurrentSave->UnlockedSynergyUpgradeIds.Remove(Id);
 return false;
}
int32 USynergyMetaProgressionSubsystem::GetSoulEmbers() const { return CurrentSave ? CurrentSave->SoulEmbers : 0; }
int32 USynergyMetaProgressionSubsystem::GetSkillRank(FName Id) const { return CurrentSave ? MetaSkillTree::Rank(*CurrentSave, Id) : 0; }
int32 USynergyMetaProgressionSubsystem::GetSkillCost(FName Id) const { return CurrentSave ? MetaSkillTree::Cost(*CurrentSave, Id) : 0; }
float USynergyMetaProgressionSubsystem::GetSkillBonus(FName Effect) const { return CachedSkillBonuses.FindRef(Effect); }
float USynergyMetaProgressionSubsystem::GetStanceSkillBonus(FName Effect,FName Stance) const
{
 if (!IsStanceUnlocked(Stance)) return 0.f;
 const auto* Bonuses=CachedStanceSkillBonuses.Find(Stance);
 return Bonuses?Bonuses->FindRef(Effect):0.f;
}
FString USynergyMetaProgressionSubsystem::GetSkillPurchaseBlock(FName Id) const
{
 if (!SkillChangesAllowed(this)) return TEXT("Return to the main menu to learn skills.");
 const auto* Node = MetaSkillTree::Find(Id);
 if (Node && !Node->RequiredStance.IsNone() && !IsStanceUnlocked(Node->RequiredStance)) return TEXT("Unlock this stance first.");
 return CurrentSave ? MetaSkillTree::PurchaseBlock(*CurrentSave, Id) : TEXT("Progression unavailable.");
}
bool USynergyMetaProgressionSubsystem::PurchaseSkill(FName Id)
{
 RetrySkillReward();
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
namespace
{
 int32 AddCurrency(int32 Current, int32 Amount)
 {
  return static_cast<int32>(FMath::Min<int64>(MAX_int32, int64(Current) + FMath::Max(0, Amount)));
 }
 int32 ScaleCurrency(int32 Amount,double Multiplier,double& Fraction)
 {
  const double Total=FMath::Max(0,Amount)*Multiplier+Fraction;
  const int32 Whole=static_cast<int32>(FMath::Min<double>(MAX_int32,FMath::FloorToDouble(Total+0.000001)));
  Fraction=Total>=MAX_int32?0.0:FMath::Max(0.0,Total-Whole);
  return Whole;
 }
}
FIntPoint USynergyMetaProgressionSubsystem::PreviewCurrencyReward(int32 Embers,int32 Sigils) const
{
 const auto* GI=GetGameInstance();
 const auto* Challenge=GI?GI->GetSubsystem<URunChallengeSubsystem>():nullptr;
 const double Multiplier=(1.0+GetSkillBonus(TEXT("Currency")))*(Challenge?Challenge->GetRewardMultiplier():1.f);
 double EF=EmberFraction,SF=SigilFraction;
 return FIntPoint(ScaleCurrency(Embers,Multiplier,EF),ScaleCurrency(Sigils,Multiplier,SF));
}
void USynergyMetaProgressionSubsystem::BeginCurrencyRun()
{
 RetrySkillReward();
 RunEmbers = RunSigils = LastSkillRunReward = 0;
 // Fractional currency bonuses belong to the permanent wallet, not this run.
}
void USynergyMetaProgressionSubsystem::AwardCurrencies(int32 Embers, int32 Sigils)
{
 if (ABalanceLabDirector::Find(GetWorld()) || (Embers <= 0 && Sigils <= 0)) return;
 if (!CurrentSave) CreateFreshSave();
 const auto* GI = GetGameInstance();
 const auto* Challenge = GI ? GI->GetSubsystem<URunChallengeSubsystem>() : nullptr;
 const double Multiplier = (1.0 + GetSkillBonus(TEXT("Currency"))) * (Challenge ? Challenge->GetRewardMultiplier() : 1.f);
 const int32 EarnedEmbers = ScaleCurrency(Embers, Multiplier, EmberFraction), EarnedSigils = ScaleCurrency(Sigils, Multiplier, SigilFraction);
 PendingSkillReward = AddCurrency(PendingSkillReward, EarnedEmbers);
 PendingSigils = AddCurrency(PendingSigils, EarnedSigils);
 RunEmbers = AddCurrency(RunEmbers, EarnedEmbers);
 RunSigils = AddCurrency(RunSigils, EarnedSigils);
}
bool USynergyMetaProgressionSubsystem::AwardSkillRun(float Seconds, bool bVictory)
{
 const int32 Before = RunEmbers;
 AwardCurrencies(MetaSkillTree::RunReward(Seconds, bVictory), bVictory ? 5 : 0);
 LastSkillRunReward = RunEmbers - Before;
 RetrySkillReward();
 return !HasPendingSkillReward();
}
void USynergyMetaProgressionSubsystem::AwardObjectiveEmbers(int32 Amount)
{
 AwardCurrencies(Amount, 0);
 RetrySkillReward();
}
void USynergyMetaProgressionSubsystem::RetrySkillReward()
{
 if (!HasPendingSkillReward() || !CurrentSave) return;
 const auto OldDifficulty=CurrentSave->UnlockedDifficultyRanks;
 for(const auto& Unlock:PendingDifficultyRanks)CurrentSave->UnlockedDifficultyRanks.FindOrAdd(Unlock.Key)=FMath::Max(CurrentSave->UnlockedDifficultyRanks.FindRef(Unlock.Key),Unlock.Value);
 const int32 OldEmbers = CurrentSave->SoulEmbers, OldSigils = CurrentSave->SoulSigils;
 const double OldEmberFraction=CurrentSave->EmberFraction, OldSigilFraction=CurrentSave->SigilFraction;
 CurrentSave->EmberFraction=EmberFraction; CurrentSave->SigilFraction=SigilFraction;
 CurrentSave->SoulEmbers = AddCurrency(OldEmbers, PendingSkillReward);
 CurrentSave->SoulSigils = AddCurrency(OldSigils, PendingSigils);
 if (SaveMetaProgression()) { PendingSkillReward = PendingSigils = 0;PendingDifficultyRanks.Reset(); }
 else { CurrentSave->UnlockedDifficultyRanks=OldDifficulty;CurrentSave->SoulEmbers = OldEmbers; CurrentSave->SoulSigils = OldSigils;
  CurrentSave->EmberFraction=OldEmberFraction; CurrentSave->SigilFraction=OldSigilFraction; }
}

void USynergyMetaProgressionSubsystem::RefreshSkillBonuses()
{
 CachedSkillBonuses.Reset(); CachedStanceSkillBonuses.Reset();
 if (!CurrentSave) return;
 for (const FMetaSkillNode& N : MetaSkillTree::Nodes())
 {
  auto& Bonuses=N.RequiredStance.IsNone()?CachedSkillBonuses:CachedStanceSkillBonuses.FindOrAdd(N.RequiredStance);
  Bonuses.FindOrAdd(N.Effect)+=MetaSkillTree::Rank(*CurrentSave,N.Id)*N.PerRank;
 }
}
