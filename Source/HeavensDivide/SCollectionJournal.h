#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "UObject/StrongObjectPtr.h"
#include "Styling/SlateBrush.h"

class UGameInstance;
class UUpgradeDefinition;
class UCollectionProgressSubsystem;
class USynergyMetaProgressionSubsystem;
class SBox;
class SScrollBox;
class SButton;

class SCollectionJournal : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCollectionJournal) {} SLATE_ARGUMENT(UGameInstance*, GameInstance) SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    void Refresh();
    void FocusFirst();
    virtual FReply OnKeyDown(const FGeometry&,const FKeyEvent&) override;
private:
    friend class FCollectionVisualTest;
    void RebuildEntries();
    void Select(int32 Index);
    FString Requirement(const UUpgradeDefinition* D) const;
    TWeakObjectPtr<UCollectionProgressSubsystem> Progress;
    TWeakObjectPtr<USynergyMetaProgressionSubsystem> Meta;
    TArray<TStrongObjectPtr<UUpgradeDefinition>> Definitions;
    TArray<FSlateBrush> Artwork;
    TArray<TSharedPtr<SButton>> Tiles;
    TSharedPtr<SBox> Entries,Details;
    TSharedPtr<SScrollBox> Scroll;
    FString Summary;
    FButtonStyle ButtonStyle;
    FProgressBarStyle ProgressStyle;
    int32 Category=0,Filter=0,Selected=INDEX_NONE;
};
