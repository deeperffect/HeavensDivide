#pragma once
#include "CoreMinimal.h"
#include "MenuFeedbackWidget.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "StanceSelectionWidget.generated.h"

class UMainMenuWidget;
class UUpgradeDefinition;
class SButton;
class SBox;
class SScrollBox;
class SComboButton;

enum class ERunSetupPage : uint8 { ArenaAndStances, SynergiesAndCombo };

UCLASS()
class HEAVENSDIVIDE_API UStanceSelectionWidget : public UMenuFeedbackWidget
{
    GENERATED_BODY()
public:
    UStanceSelectionWidget(const FObjectInitializer& Initializer);
    UPROPERTY(Transient) TObjectPtr<UMainMenuWidget> MenuOwner;
    void Open();
    bool SelectStance(FName Id);
    bool IsStanceAvailable(FName Id) const;
    bool SelectMap(FName Id);
    FName GetSelectedMap() const { return SelectedMap; }
    int32 GetVisibleStanceCount() const;
    void SelectSetupPage(ERunSetupPage Page);
    ERunSetupPage GetSetupPage() const { return SetupPage; }
    int32 GetVisibleSynergyCount() const { return SynergyDefinitions.Num(); }
    bool SetSynergySelected(FName Id, bool bSelected);
    bool IsSynergySelected(FName Id) const;
    bool SelectComboAbility(FName Id);
    bool CanStartRun() const;
    FName GetSelectedSamurai() const { return SelectedSamurai; }
    FName GetSelectedNinja() const { return SelectedNinja; }
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual FReply NativeOnFocusReceived(const FGeometry&, const FFocusEvent&) override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry&, const FKeyEvent&) override;
private:
    friend class FRunPreparationVisualTest;
    friend class FComboTechniquesTest;
    TSharedRef<SWidget> BuildPage();
    TSharedRef<SWidget> BuildArenaPage();
    TSharedRef<SWidget> BuildSynergyPage();
    TSharedRef<SWidget> MakeSetupTab(ERunSetupPage Page);
    TSharedRef<SWidget> MakeSynergyCard(int32 Index);
    TSharedRef<SWidget> MakeLoadoutPicker(int32 Kind);
    TSharedRef<SWidget> MakeLoadoutChoices(int32 Kind);
    TSharedRef<SWidget> MakeMapCard(int32 Index);
    TSharedRef<SWidget> MakeModifierRow(int32 Index);
    void AddNavigation(const TSharedRef<SWidget>& Button, FVector2D Position);
    void MoveFocus(FVector2D Direction);
    bool IsSelected(int32 Index) const;
    void FocusButton(int32 Index);
    FReply ConfirmRun();
    FSlateFontInfo Font(int32 Size, bool bHeading = false) const;
    UPROPERTY(Transient) TArray<TObjectPtr<UUpgradeDefinition>> Definitions;
    UPROPERTY(Transient) TArray<FSlateBrush> Artwork;
    UPROPERTY(Transient) TArray<TObjectPtr<UUpgradeDefinition>> SynergyDefinitions;
    UPROPERTY(Transient) TArray<FSlateBrush> SynergyArtwork;
    UPROPERTY() FSlateFontInfo HeadingFont;
    FButtonStyle CardStyle;
    FButtonStyle ActionStyle;
    FButtonStyle RankStyle;
    FSlateBrush SelectedBrush;
    FSlateBrush EmptyBrush;
    TArray<TSharedPtr<SWidget>> Buttons;
    TSharedPtr<SComboButton> LoadoutPickers[4];
    TArray<FVector2D> NavigationPositions;
    TSharedPtr<SBox> PageContent;
    TSharedPtr<SScrollBox> SynergyScroll;
    TSet<int32> ScrollNavigationIndices;
    ERunSetupPage SetupPage = ERunSetupPage::ArenaAndStances;
    FName SelectedSamurai = TEXT("BattleStance");
    FName SelectedNinja = TEXT("BarrageStance");
    FName SelectedMap = TEXT("FallenGrove");
    int32 FocusedButton = 0;
    bool bStarting = false;
};
