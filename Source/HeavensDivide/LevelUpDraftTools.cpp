#include "LevelUpWidget.h"
#include "MainMenuWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/ScaleBox.h"

void ULevelUpWidget::RefreshDraftTools()
{
 if (!WidgetTree || !WidgetTree->RootWidget) return;
 if (!DraftToolsRow)
 {
  auto* Root = Cast<UCanvasPanel>(WidgetTree->RootWidget);
  if (!Root) return;
  auto* Scale = WidgetTree->ConstructWidget<UScaleBox>();
  Scale->SetStretch(EStretch::ScaleToFit);
  Scale->SetStretchDirection(EStretchDirection::DownOnly);
  auto* FooterSlot = Root->AddChildToCanvas(Scale);
  FooterSlot->SetAnchors(FAnchors(0.05f, 0.87f, 0.95f, 0.98f));
  FooterSlot->SetOffsets(FMargin(0));
  FooterSlot->SetZOrder(20);
  DraftToolsRow = WidgetTree->ConstructWidget<UHorizontalBox>();
  Scale->SetContent(DraftToolsRow);
  const auto* MenuClass = LoadClass<UMainMenuWidget>(nullptr, TEXT("/Game/HeavensDivide/Blueprints/UI/MainMenu/WBP_MainMenu.WBP_MainMenu_C"));
  const auto* Menu = MenuClass ? MenuClass->GetDefaultObject<UMainMenuWidget>() : GetDefault<UMainMenuWidget>();
  auto AddButton = [&](FName Name, TObjectPtr<UButton>& Button, TObjectPtr<UTextBlock>& Label)
  {
   Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
   PRAGMA_DISABLE_DEPRECATION_WARNINGS
   Button->IsFocusable = false;
   PRAGMA_ENABLE_DEPRECATION_WARNINGS
   FButtonStyle Style = Button->GetStyle();
   FSlateBrush Empty;
   Empty.DrawAs = ESlateBrushDrawType::NoDrawType;
   Style.SetNormal(Empty); Style.SetHovered(Empty); Style.SetPressed(Empty); Style.SetDisabled(Empty);
   Button->SetStyle(Style);
   Label = WidgetTree->ConstructWidget<UTextBlock>();
   auto Font = Menu->GetMenuButtonFont();
   Font.Size = 22;
   Label->SetFont(Font);
   Label->SetColorAndOpacity(FLinearColor(0.93f, 0.93f, 0.90f));
   Label->SetJustification(ETextJustify::Center);
   Label->SetVisibility(ESlateVisibility::HitTestInvisible);
   Button->SetContent(Label);
   auto* ButtonSlot = DraftToolsRow->AddChildToHorizontalBox(Button);
   ButtonSlot->SetPadding(FMargin(22, 4));
  };
  AddButton(TEXT("DraftReroll"), RerollButton, RerollLabel);
  AddButton(TEXT("DraftBanish"), BanishButton, BanishLabel);
  RerollButton->OnClicked.AddDynamic(this, &ULevelUpWidget::HandleReroll);
  BanishButton->OnClicked.AddDynamic(this, &ULevelUpWidget::HandleBanishMode);
 }
 const bool bVisible = PlayerUpgrades && bCategoryChoiceCommitted && !bUpgradeChoiceCommitted
  && !bSynergyDiscoveryMode && PlayerUpgrades->IsDraftToolOffer();
 DraftToolsRow->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
 if (!bVisible) { bBanishMode = false; return; }
 RerollLabel->SetText(FText::FromString(FString::Printf(TEXT("Reroll (%d)"), PlayerUpgrades->GetRerollsRemaining())));
 BanishLabel->SetText(FText::FromString(FString::Printf(TEXT("Banish (%d)"), PlayerUpgrades->GetBanishesRemaining())));
 BanishLabel->SetColorAndOpacity(bBanishMode ? FLinearColor(0.94f, 0.72f, 0.24f) : FLinearColor(0.93f, 0.93f, 0.90f));
 for (auto* Button : {RerollButton.Get(), BanishButton.Get()})
  if (auto* ContentSlot = Cast<UButtonSlot>(Button->GetContent()->Slot))
   ContentSlot->SetPadding(FMargin(24, 14));
 RerollButton->SetIsEnabled(PlayerUpgrades->CanReroll());
 BanishButton->SetIsEnabled(bBanishMode || PlayerUpgrades->CanBanish());
 RerollButton->SetToolTipText(FText::FromString(TEXT("Replace these choices. [R / X]")));
 BanishButton->SetToolTipText(FText::FromString(bBanishMode ? TEXT("Select a card to banish. Click Banish again to cancel. [B / Y]") : TEXT("Choose a card to remove from this run and replace it. [B / Y]")));
}

void ULevelUpWidget::RefreshDraftCards()
{
 ShowUpgradeChoices(PlayerUpgrades->GetCurrentUpgradeChoices());
 ShowUpgradeOffers(PlayerUpgrades->GetCurrentUpgradeOffers());
 RefreshUpgradeCardVisuals();
 ControllerFocusedChoiceIndex = 0;
 MouseHoveredButton = nullptr;
 RefreshControllerFocus();
}

void ULevelUpWidget::HandleReroll()
{
 if (!PlayerUpgrades || !bCategoryChoiceCommitted || bUpgradeChoiceCommitted || bSynergyDiscoveryMode) return;
 if (!PlayerUpgrades->RerollUpgrades()) return;
 if (!RerollButton->IsHovered())
 {
  SetMenuFocusedButton(RerollButton);
  PressMenuFocusedButton();
 }
 bBanishMode = false;
 RefreshDraftCards();
}

void ULevelUpWidget::HandleBanishMode()
{
 if (!PlayerUpgrades || !bCategoryChoiceCommitted || bUpgradeChoiceCommitted || bSynergyDiscoveryMode) return;
 if (!bBanishMode && !PlayerUpgrades->CanBanish()) return;
 if (!BanishButton->IsHovered())
 {
  SetMenuFocusedButton(BanishButton);
  PressMenuFocusedButton();
 }
 bBanishMode = !bBanishMode;
 RefreshDraftTools();
}
