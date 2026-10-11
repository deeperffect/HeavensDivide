#pragma once
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboBox.h"

class ABalanceLabDirector;
struct FBalanceLabUpgrade
{
    FName Id;
    FString Label;
    FString Description;
};

/** Editor-only controls for the live PIE lab. No asset defaults are edited here. */
class SBalanceLab : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SBalanceLab) {} SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    FReply Execute(const FString& Action, const FString& Value = FString(), int32 Count = 1);
    FString GetFeedback() const { return Feedback; }
    bool CanControlLab() const;
private:
    static ABalanceLabDirector* GetLab();
    void LoadUpgrades();
    void FilterUpgrades();
    FReply OpenMap();
    FReply StartTest();
    FText SessionText() const;
    FText UpgradeText() const;
    TSharedRef<SWidget> ActionButton(const FString& Label, const FString& Action, const FString& Value = FString());
    TArray<TSharedPtr<FString>> Targets;
    TSharedPtr<FString> SelectedTarget;
    TArray<TSharedPtr<FBalanceLabUpgrade>> Upgrades, FilteredUpgrades;
    TSharedPtr<FBalanceLabUpgrade> SelectedUpgrade;
    TSharedPtr<SComboBox<TSharedPtr<FBalanceLabUpgrade>>> UpgradePicker;
    FString Search;
    FString Feedback = TEXT("Open the lab map, then Start test. During play, Shift+F1 releases the mouse so you can click this panel.");
    int32 BuildRanks = 0, TargetCount = 1, GrantRanks = 1;
};
