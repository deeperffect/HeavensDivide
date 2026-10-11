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
    bool CanStartRun() const;
    FName GetSelectedSamurai() const { return SelectedSamurai; }
    FName GetSelectedNinja() const { return SelectedNinja; }
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual FReply NativeOnFocusReceived(const FGeometry&, const FFocusEvent&) override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry&, const FKeyEvent&) override;
private:
    TSharedRef<SWidget> BuildPage();
    TSharedRef<SWidget> MakeStanceCard(int32 Index, int32 Count, int32 Column);
    TSharedRef<SWidget> MakeCharacterGroup(bool bSamurai);
    TSharedRef<SWidget> MakeMapCard(int32 Index);
    TSharedRef<SWidget> MakeModifierRow(int32 Index);
    void AddNavigation(const TSharedRef<SButton>& Button, FVector2D Position);
    void MoveFocus(FVector2D Direction);
    bool IsSelected(int32 Index) const;
    void FocusButton(int32 Index);
    FReply ConfirmRun();
    FSlateFontInfo Font(int32 Size, bool bHeading = false) const;
    UPROPERTY(Transient) TArray<TObjectPtr<UUpgradeDefinition>> Definitions;
    UPROPERTY(Transient) TArray<FSlateBrush> Artwork;
    UPROPERTY() FSlateFontInfo HeadingFont;
    FButtonStyle CardStyle;
    FButtonStyle ActionStyle;
    FButtonStyle RankStyle;
    FSlateBrush SelectedBrush;
    FSlateBrush EmptyBrush;
    TArray<TSharedPtr<SButton>> Buttons;
    TArray<FVector2D> NavigationPositions;
    TSharedPtr<SBox> PageContent;
    FName SelectedSamurai = TEXT("BattleStance");
    FName SelectedNinja = TEXT("BarrageStance");
    FName SelectedMap = TEXT("FallenGrove");
    int32 FocusedButton = 0;
    bool bStarting = false;
};
