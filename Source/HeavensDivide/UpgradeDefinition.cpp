// Copyright Epic Games, Inc. All Rights Reserved.

#include "UpgradeDefinition.h"

FText UUpgradeDefinition::GetBalanceDescription() const { return FormatBalanceText(Description); }
FText UUpgradeDefinition::FormatBalanceText(const FText& Text) const
{
    // Preserve ordinary localized/manual copy unchanged. Named tokens are opt-in.
    if(!Text.ToString().Contains(TEXT("{")))return Text;
    FFormatNamedArguments Args;
    for(const auto& Pair:BalanceParameters)
    {
        Args.Add(Pair.Key.ToString(),FText::AsNumber(Pair.Value));
        Args.Add(Pair.Key.ToString()+TEXT("Percent"),FText::AsNumber(Pair.Value*100.f));
    }
    for(int32 I=0;I<StatModifiers.Num();++I)
    {
        const auto& M=StatModifiers[I];const FString Key=FString::Printf(TEXT("Stat%d"),I);
        Args.Add(Key,FText::AsNumber(M.ValuePerLevel));
        Args.Add(Key+TEXT("Percent"),FText::AsNumber(M.ValuePerLevel*100.f));
        Args.Add(Key+TEXT("BonusPercent"),FText::AsNumber((M.ValuePerLevel-(M.Operation==EStatModifierOperation::Multiply?1.f:0.f))*100.f));
    }
    Args.Add(TEXT("MaxLevel"),FText::AsNumber(MaxLevel));
    Args.Add(TEXT("HandoffDuration"),FText::AsNumber(HandoffDuration));
    Args.Add(TEXT("HandoffAttackSpeedBonusPercent"),FText::AsNumber(HandoffAttackSpeedBonus*100.f));
    return FText::Format(Text,Args);
}
