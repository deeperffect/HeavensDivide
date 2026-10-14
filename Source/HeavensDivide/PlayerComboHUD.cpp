#include "PlayerHUDWidget.h"
#include "Blueprint/WidgetTree.h"
#include "ComboRadialWidget.h"
#include "ComboAbilityDefinition.h"
#include "ComboAbilityComponent.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HeavensDivideGameUserSettings.h"
#include "SurvivorPlayerController.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"

#include "UObject/ConstructorHelpers.h"

UPlayerHUDWidget::UPlayerHUDWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    static ConstructorHelpers::FObjectFinder<UFont> Font(TEXT("/Game/Assets/Fonts/Cinzel-Medium_Font"));
    static ConstructorHelpers::FObjectFinder<UTexture2D> SwapIcon(TEXT("/Game/HeavensDivide/Blueprints/UI/Swap/T_SwapInkArrows"));
    SwapBadgeIcon = SwapIcon.Object;
    ComboLabelFont = FSlateFontInfo(Font.Object, 11);
}

float UPlayerHUDWidget::GetComboPercent() const
{
    auto* Combo = SurvivorPlayerController ? SurvivorPlayerController->FindComponentByClass<UComboAbilityComponent>() : nullptr;
    return Combo ? Combo->GetComboPercent() : 0.f;
}
bool UPlayerHUDWidget::IsComboReady() const
{
    auto* Combo = SurvivorPlayerController ? SurvivorPlayerController->FindComponentByClass<UComboAbilityComponent>() : nullptr;
    return Combo && Combo->IsFull() && !Combo->GetActiveAbilityId().IsNone();
}
void UPlayerHUDWidget::EnsureComboPresentation()
{
    if (!bShowComboMeter || !WidgetTree || !WidgetTree->RootWidget) return;
    if (ComboContainer) return;
    // Old Blueprint bindings may still reference the bar. Keep it as a hidden data sink.
    if (ComboMeterBar) ComboMeterBar->SetVisibility(ESlateVisibility::Collapsed);
    UWidget* ExistingRoot = WidgetTree->RootWidget;
    auto* Canvas = Cast<UCanvasPanel>(ExistingRoot);
    if (!Canvas)
    {
        Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ComboPresentationRoot"));
        WidgetTree->RootWidget = Canvas;
        auto* ExistingSlot = Canvas->AddChildToCanvas(ExistingRoot);
        ExistingSlot->SetAnchors(FAnchors(0, 0, 1, 1));
        ExistingSlot->SetOffsets(FMargin(0));
    }
    ComboContainer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ComboContainer"));
    ComboContainer->SetVisibility(ESlateVisibility::HitTestInvisible);
    ComboContainer->SetBrushColor(FLinearColor::Transparent);
    ComboContainer->SetPadding(FMargin(0.f));
    auto* Stack = WidgetTree->ConstructWidget<UVerticalBox>();
    ComboContainer->SetContent(Stack);
    ComboRadialMeter = WidgetTree->ConstructWidget<UComboRadialWidget>(UComboRadialWidget::StaticClass(), TEXT("ComboRadialMeter"));
    Stack->AddChildToVerticalBox(ComboRadialMeter)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    if (!ComboAbilityText)
        ComboAbilityText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ComboAbilityText"));
    else
        ComboAbilityText->RemoveFromParent();
    ComboAbilityText->SetJustification(ETextJustify::Center);
    ComboAbilityText->SetFont(ComboLabelFont);
    ComboAbilityText->SetShadowColorAndOpacity(FLinearColor::Black);
    ComboAbilityText->SetShadowOffset(FVector2D(1, 1));
    Stack->AddChildToVerticalBox(ComboAbilityText)->SetPadding(FMargin(0, 3, 0, 0));
    auto* ComboSlot = Canvas->AddChildToCanvas(ComboContainer);
    ComboSlot->SetAnchors(FAnchors(.5f, 1.f));
    ComboSlot->SetAlignment(FVector2D(.5f, 1.f));
    ComboSlot->SetPosition(ComboMeterPosition);
    ComboSlot->SetSize(ComboMeterSize);
    ComboSlot->SetZOrder(50);
}
void UPlayerHUDWidget::UpdateComboPresentation()
{
    if (ComboContainer) ComboContainer->SetVisibility(bShowComboMeter ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    if (!bShowComboMeter) return;
    const auto* Combo=SurvivorPlayerController?SurvivorPlayerController->FindComponentByClass<UComboAbilityComponent>():nullptr;
    const bool bUnlocked=Combo&&Combo->IsComboUnlocked();
    const bool bEquipped=Combo&&Combo->IsComboEquipped();
    const bool bReady = IsComboReady();
    if (ComboMeterBar) ComboMeterBar->SetPercent(GetComboPercent());
    const FName AbilityId = Combo ? Combo->GetActiveAbilityId() : NAME_None;
    if (AbilityId != DisplayedComboId)
    {
        DisplayedComboId = AbilityId;
        DisplayedComboDefinition = ComboCatalog::Definition(AbilityId);
    }
    const auto* Settings = UHeavensDivideGameUserSettings::GetHeavensDivideGameUserSettings();
    const FKey Key = Settings ? Settings->GetKeyBinding(TEXT("ComboAbility")) : EKeys::Q;
    FText KeyLabel = Key.GetDisplayName();
    if (Key == EKeys::SpaceBar) KeyLabel = NSLOCTEXT("Combo", "Space", "Space");
    else if (Key == EKeys::RightMouseButton) KeyLabel = NSLOCTEXT("Combo", "RMB", "RMB");
    else if (Key == EKeys::LeftMouseButton) KeyLabel = NSLOCTEXT("Combo", "LMB", "LMB");
    else if (Key == EKeys::MiddleMouseButton) KeyLabel = NSLOCTEXT("Combo", "MMB", "MMB");
    if (ComboRadialMeter)
        ComboRadialMeter->SetPresentation(DisplayedComboDefinition ? DisplayedComboDefinition->Icon.Get() : nullptr,
            GetComboPercent(), bReady, bEquipped, KeyLabel, ComboLabelFont, ComboChargingColor, ComboReadyColor,
            ComboGlowIntensity, ComboGlowPulseSpeed);
    if (ComboAbilityText)
    {
        ComboAbilityText->SetColorAndOpacity(FSlateColor(bEquipped ? (bReady ? ComboReadyColor : ComboChargingColor) : FLinearColor(.48f,.60f,.61f)));
        ComboAbilityText->SetText(!bUnlocked ? NSLOCTEXT("Combo", "LockedRadialHUD", "COMBO LOCKED")
            : !bEquipped ? NSLOCTEXT("Combo", "UnequippedRadialHUD", "NO COMBO EQUIPPED") : Combo->GetActiveAbilityName());
    }
}
