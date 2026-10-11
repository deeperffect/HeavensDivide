#include "UpgradeBalanceDetails.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "PropertyHandle.h"
#include "Widgets/Text/STextBlock.h"
#include "UpgradeDefinition.h"

void FUpgradeBalanceDetails::CustomizeDetails(IDetailLayoutBuilder& Builder)
{
    TArray<TWeakObjectPtr<UObject>> Objects;Builder.GetObjectsBeingCustomized(Objects);
    const auto* Card=Objects.Num()==1?Cast<UUpgradeDefinition>(Objects[0].Get()):nullptr;
    if(Card&&Card->UpgradeId!=TEXT("Handoff"))
    { Builder.HideProperty(TEXT("HandoffAttackSpeedBonus"));Builder.HideProperty(TEXT("HandoffDuration")); }
    if(Card&&Card->UpgradeId!=TEXT("AfterimageFrenzy"))Builder.HideProperty(TEXT("AfterimageFrenzyAttackSpeedBonus"));
    Builder.HideProperty(TEXT("bHasRuntimeBalance"));
    Builder.HideProperty(TEXT("bHasRuntimePresentation"));
    const auto Map=Builder.GetProperty(GET_MEMBER_NAME_CHECKED(UUpgradeDefinition,BalanceParameters));
    Builder.HideProperty(Map);
    auto& Category=Builder.EditCategory(TEXT("Runtime Balance"),FText::FromString(TEXT("Ability / mechanic values")),ECategoryPriority::Important);
    uint32 Count=0; Map->GetNumChildren(Count);
    TArray<TPair<FString,TSharedPtr<IPropertyHandle>>> Rows;
    for(uint32 I=0;I<Count;++I)
    {
        auto Value=Map->GetChildHandle(I); FString Key;
        if(!Value||!Value->GetKeyHandle())continue;
        Value->GetKeyHandle()->GetValueAsFormattedString(Key);
        // Retained serialization from the old partial-transfer implementation; no longer read by combat.
        if(Card&&Card->UpgradeId==TEXT("BloodTransfer")&&(Key==TEXT("TransferFraction")||Key==TEXT("Targets")))continue;
        Rows.Emplace(Key,Value);
    }
    Rows.Sort([](const auto& A,const auto& B){return A.Key<B.Key;});
    for(const auto& Row:Rows)
    {
        FString Units=TEXT("Multiplier / amount. Fractions: 0.15 = 15%.");
        if(Row.Key.Contains(TEXT("Duration"))||Row.Key.Contains(TEXT("Interval"))||Row.Key.Contains(TEXT("Cooldown"))||Row.Key.Contains(TEXT("Delay")))Units=TEXT("Seconds.");
        else if(Row.Key.Contains(TEXT("Radius"))||Row.Key.Contains(TEXT("Distance"))||Row.Key.Contains(TEXT("Range"))||Row.Key.Contains(TEXT("Width")))Units=TEXT("Unreal units (centimeters), unless the name specifies a multiplier or per-rank bonus.");
        else if(Row.Key.Contains(TEXT("Chance")))Units=TEXT("Probability fraction: 0.15 = 15%. Final proc chances are clamped to 0-1.");
        if(!Row.Key.Contains(TEXT("Offset"))&&!Row.Key.Contains(TEXT("Angle")))Row.Value->SetInstanceMetaData(TEXT("ClampMin"),TEXT("0"));
        Category.AddCustomRow(FText::FromString(Row.Key))
            .NameContent()[SNew(STextBlock).Text(FText::FromString(FName::NameToDisplayString(Row.Key,false))).Font(IDetailLayoutBuilder::GetDetailFont()).ToolTipText(FText::FromString(Row.Key+TEXT("\n")+Units))]
            .ValueContent().MinDesiredWidth(180)[Row.Value->CreatePropertyValueWidget()];
    }
}
