// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SynergyMetaProgressionSubsystem.generated.h"

class UHeavensDivideMetaSaveGame;
class UUpgradeDefinition;

UCLASS()
class HEAVENSDIVIDE_API USynergyMetaProgressionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

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
	void CreateFreshSave();
	bool AddDefaultUnlocks();

	UPROPERTY()
	TObjectPtr<UHeavensDivideMetaSaveGame> CurrentSave;
};
