// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "SynergyMetaProgressionSubsystem.generated.h"

class UHeavensDivideMetaSaveGame;
class UUpgradeDefinition;
enum class EUpgradeCategory : uint8;

UCLASS()
class HEAVENSDIVIDE_API USynergyMetaProgressionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category="Meta Progression") int32 GetSoulSigils() const;
	UFUNCTION(BlueprintPure, Category="Meta Progression") bool IsStanceUnlocked(FName Id) const;
	UFUNCTION(BlueprintPure, Category="Meta Progression") int32 GetStanceCost(FName Id) const;
	UFUNCTION(BlueprintPure, Category="Meta Progression") FString GetStancePurchaseBlock(FName Id) const;
	UFUNCTION(BlueprintCallable, Category="Meta Progression") bool PurchaseStance(FName Id);
	UFUNCTION(BlueprintPure, Category="Meta Progression") int32 GetSynergyCost(FName Id) const;
	UFUNCTION(BlueprintPure, Category="Meta Progression") FString GetSynergyPurchaseBlock(FName Id) const;
	UFUNCTION(BlueprintCallable, Category="Meta Progression") bool PurchaseSynergy(FName Id);
	/** Automatic collection; fractional bonuses accumulate and saves are batched. */
	void AwardCurrencies(int32 Embers, int32 Sigils);
	void BeginCurrencyRun();
	int32 GetRunEmbers() const { return RunEmbers; }
	int32 GetRunSigils() const { return RunSigils; }
	int32 GetPendingEmbers() const { return PendingSkillReward; }
	int32 GetPendingSigils() const { return PendingSigils; }
	UFUNCTION(BlueprintPure, Category = "Meta Progression|Skill Tree") int32 GetSoulEmbers() const;
	UFUNCTION(BlueprintPure, Category = "Meta Progression|Skill Tree") int32 GetSkillRank(FName Id) const;
	UFUNCTION(BlueprintPure, Category = "Meta Progression|Skill Tree") int32 GetSkillCost(FName Id) const;
	UFUNCTION(BlueprintPure, Category = "Meta Progression|Skill Tree") FString GetSkillPurchaseBlock(FName Id) const;
	UFUNCTION(BlueprintCallable, Category = "Meta Progression|Skill Tree") bool PurchaseSkill(FName Id);
	UFUNCTION(BlueprintCallable, Category = "Meta Progression|Skill Tree") bool RefundSkills();
	UFUNCTION(BlueprintPure, Category = "Meta Progression|Skill Tree") float GetSkillBonus(FName Effect) const;
	float GetStanceSkillBonus(FName Effect, FName Stance) const;
	/** Called only by the controller's terminal run transition. */
	bool AwardSkillRun(float Seconds, bool bVictory);
    void AwardObjectiveEmbers(int32 Amount);
	int32 GetLastSkillRunReward() const { return LastSkillRunReward; }
	bool HasPendingSkillReward() const { return PendingSkillReward > 0 || PendingSigils > 0; }
	void RetrySkillReward();
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "Meta Progression|Synergy")
	bool IsSynergyUpgradeUnlocked(FName MetaUnlockId) const;

	UFUNCTION(BlueprintCallable, Category = "Meta Progression|Synergy")
	bool UnlockSynergyUpgrade(FName MetaUnlockId);

	UFUNCTION(BlueprintPure, Category = "Meta Progression|Synergy")
	bool IsUpgradeMetaEligible(const UUpgradeDefinition* Upgrade) const;

	UFUNCTION(BlueprintCallable, Category = "Meta Progression|Synergy")
	bool SaveMetaProgression();

	UFUNCTION(BlueprintCallable, Category = "Meta Progression|Synergy")
	void LoadMetaProgression();

	UFUNCTION(BlueprintCallable, Category = "Meta Progression")
	bool ResetMetaProgression();

	UFUNCTION(BlueprintPure, Category = "Meta Progression|Synergy")
	TArray<FName> GetUnlockedSynergyUpgradeIds() const;

	UFUNCTION(BlueprintPure, Category = "Meta Progression|Synergy")
	TArray<UUpgradeDefinition*> GetSynergyUpgradeDefinitions() const;

	/** Asset-registry-backed collection entries for a gameplay upgrade category. */
	UFUNCTION(BlueprintPure, Category = "Meta Progression|Collection")
	TArray<UUpgradeDefinition*> GetCollectionUpgradeDefinitions(EUpgradeCategory Category) const;

	UFUNCTION(BlueprintPure, Category = "Meta Progression|Collection")
	bool IsCollectionUpgradeUnlocked(const UUpgradeDefinition* Upgrade) const;

	UFUNCTION(BlueprintPure, Category = "Meta Progression|Twin Soul")
	int32 GetTwinSoulDiscoveryProgress() const;
	UFUNCTION(BlueprintPure, Category = "Meta Progression|Twin Soul")
	int32 GetTwinSoulCompletionsPerDiscovery() const;
	UFUNCTION(BlueprintCallable, Category = "Meta Progression|Twin Soul")
	bool RecordTwinSoulCompletion();
	UFUNCTION(BlueprintCallable, Category = "Meta Progression|Twin Soul")
	void ConsumeTwinSoulDiscoveryProgress();
	UFUNCTION(BlueprintCallable, Category = "Meta Progression|Twin Soul")
	void ResetTwinSoulDiscoveryProgress();

	static const FString& GetSaveSlotName();
	static int32 GetSaveUserIndex() { return 0; }

private:
	friend class FMetaSkillTreeTest;
	friend class FDraftToolsTest;
	friend class FUpgradeAvailabilityTest;
	friend class FRunPreparationTest;
	friend class FRunPreparationVisualTest;
	friend class FMetaStanceTest;
#if WITH_DEV_AUTOMATION_TESTS
	bool bSimulateSaveFailure = false;
#endif
	int32 LastSkillRunReward = 0;
	int32 PendingSkillReward = 0;
	int32 PendingSigils = 0;
	int32 RunEmbers = 0;
	int32 RunSigils = 0;
	double EmberFraction = 0.0;
	double SigilFraction = 0.0;
	FTSTicker::FDelegateHandle RewardSaveTicker;
	FString TestSaveSlot;
	void RefreshSkillBonuses();
	TMap<FName, float> CachedSkillBonuses;
	TMap<FName, TMap<FName, float>> CachedStanceSkillBonuses;
	mutable TSet<FName> KnownSynergyIds;
	void CreateFreshSave();
	bool AddDefaultUnlocks();

	UPROPERTY()
	TObjectPtr<UHeavensDivideMetaSaveGame> CurrentSave;
};
