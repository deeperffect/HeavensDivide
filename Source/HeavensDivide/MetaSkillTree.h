#pragma once

#include "CoreMinimal.h"
#include "UpgradeDefinition.h"
#include "MetaSkillTree.generated.h"

class UHeavensDivideMetaSaveGame;

UENUM(BlueprintType)
enum class EMetaSkillPage : uint8 { Stats, Stances, Abilities };

/** Stable IDs are the save contract. Nodes never grant run cards or mastery. */
USTRUCT(BlueprintType)
struct HEAVENSDIVIDE_API FMetaSkillNode
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Skill", meta=(ToolTip="Stable save ID. Do not rename existing skills."))
	FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill")
	FString Name;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill", meta=(MultiLine=true,ToolTip="Use {Percent} for Per Rank x 100, or {Magnitude} for Per Rank."))
	FString Description;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout",meta=(ClampMin="0",ClampMax="3"))
	int32 Branch = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout",meta=(ClampMin="0",ClampMax="3"))
	int32 Tier = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout",meta=(ClampMin="0",ClampMax="1"))
	int32 Lane = 0;
	/** Position on the 1020 x 850 constellation canvas. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	FVector2D MapPosition = FVector2D(-1, -1);
	/** Applies while this character owns the stance, including when assisting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effect")
	FName RequiredStance;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Layout")
	EMetaSkillPage Page = EMetaSkillPage::Stats;
	/** Consumed by a dedicated mechanic instead of a character/shared stat. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effect")
	bool bPassiveEffect = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill",meta=(ClampMin="1",ClampMax="100"))
	int32 MaxRank = 3;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill",meta=(ClampMin="0",ClampMax="100000"))
	int32 BaseCost = 5;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skill")
	TArray<FName> Prerequisites;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Effect")
	FName Effect;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effect",meta=(ClampMin="0"))
	float PerRank = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effect")
	EUpgradeStatTarget Target = EUpgradeStatTarget::SharedPlayer;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effect")
	ESharedPlayerStatType SharedStat = ESharedPlayerStatType::DamageMultiplier;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Effect")
	ECharacterStatType CharacterStat = ECharacterStatType::DamageMultiplier;
};

namespace MetaSkillTree
{
	const TArray<FMetaSkillNode>& Nodes();
	const TArray<FMetaSkillNode>& DefaultNodes();
	FString Description(const FMetaSkillNode& Node);
	FString StanceName(FName Stance);
	FString Scope(const FMetaSkillNode& Node);
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
