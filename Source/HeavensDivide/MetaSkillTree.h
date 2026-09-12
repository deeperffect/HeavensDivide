#pragma once

#include "CoreMinimal.h"
#include "UpgradeDefinition.h"

class UHeavensDivideMetaSaveGame;

/** Stable IDs are the save contract. Nodes never grant run cards or mastery. */
struct FMetaSkillNode
{
	FName Id;
	FString Name;
	FString Description;
	int32 Branch = 0;
	int32 Tier = 0;
	int32 Lane = 0;
	int32 MaxRank = 3;
	int32 BaseCost = 5;
	TArray<FName> Prerequisites;
	FName Effect;
	float PerRank = 0;
	EUpgradeStatTarget Target = EUpgradeStatTarget::SharedPlayer;
	ESharedPlayerStatType SharedStat = ESharedPlayerStatType::DamageMultiplier;
	ECharacterStatType CharacterStat = ECharacterStatType::DamageMultiplier;
};

namespace MetaSkillTree
{
	const TArray<FMetaSkillNode>& Nodes();
	const FMetaSkillNode* Find(FName Id);
	int32 Rank(const UHeavensDivideMetaSaveGame& Save, FName Id);
	int32 Cost(const UHeavensDivideMetaSaveGame& Save, FName Id);
	FString PurchaseBlock(const UHeavensDivideMetaSaveGame& Save, FName Id);
	bool Purchase(UHeavensDivideMetaSaveGame& Save, FName Id);
	int32 Refund(UHeavensDivideMetaSaveGame& Save);
	void Sanitize(UHeavensDivideMetaSaveGame& Save);
	float Bonus(const UHeavensDivideMetaSaveGame& Save, FName Effect);
	int32 RunReward(float Seconds, bool bVictory);
}
