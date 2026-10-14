#include "PlayerHUDWidget.h"
#include "Blueprint/WidgetTree.h"
#include "CharacterBase.h"
#include "ComboRadialWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "HeavensDivideGameUserSettings.h"
#include "SurvivorPlayerController.h"

void UpdatePlayerActionHUD(UPlayerHUDWidget& HUD)
{
    if (!HUD.WidgetTree) return;
    auto* Canvas = Cast<UCanvasPanel>(HUD.WidgetTree->RootWidget);
    if (!Canvas) return;
    // Preserve Blueprint event sinks while the native badges present live controller state.
    for (FName Name : {TEXT("DashChargeContainer"), TEXT("SwapCooldownWidget"), TEXT("TXT_DashAction"), TEXT("TXT_SwapAction")})
        if (auto* Old = HUD.WidgetTree->FindWidget(Name)) Old->SetVisibility(ESlateVisibility::Collapsed);
    const auto* Settings = UHeavensDivideGameUserSettings::GetHeavensDivideGameUserSettings();
    auto* PC = HUD.SurvivorPlayerController.Get();
    const bool bCombo = HUD.GetActiveCharacter() && HUD.GetActiveCharacter()->bComboAbilityActive;
    for (bool bDash : {true, false})
    {
        const FName Name = bDash ? TEXT("DashRadialBadge") : TEXT("SwapRadialBadge");
        auto* Badge = Cast<UComboRadialWidget>(HUD.WidgetTree->FindWidget(Name));
        if (!Badge) {
            Badge = HUD.WidgetTree->ConstructWidget<UComboRadialWidget>(UComboRadialWidget::StaticClass(), Name);
            auto* Slot = Canvas->AddChildToCanvas(Badge);
            Slot->SetAnchors(FAnchors(.5f,1.f)); Slot->SetAlignment(FVector2D(.5f,1.f)); Slot->SetZOrder(50);
        }
        auto* Slot = CastChecked<UCanvasPanelSlot>(Badge->Slot);
        Slot->SetPosition(bDash ? HUD.DashBadgePosition : HUD.SwapBadgePosition); Slot->SetSize(HUD.ActionBadgeSize);
        const FKey Key = Settings ? Settings->GetKeyBinding(bDash ? TEXT("Dash") : TEXT("Swap")) : (bDash ? EKeys::SpaceBar : EKeys::Tab);
        FText Label = Key.GetDisplayName();
        if (Key == EKeys::SpaceBar) Label = FText::FromString(TEXT("Space"));
        else if (Key == EKeys::RightMouseButton) Label = FText::FromString(TEXT("RMB"));
        else if (Key == EKeys::LeftMouseButton) Label = FText::FromString(TEXT("LMB"));
        else if (Key == EKeys::MiddleMouseButton) Label = FText::FromString(TEXT("MMB"));
        const int32 Charges = HUD.GetCurrentDashCharges(), MaxCharges = HUD.GetMaxDashCharges();
        const bool bCooling = HUD.GetSwapCooldownRemaining() > 0.f;
        const bool bAvailable = bDash ? Charges > 0 : PC && PC->CanSwap();
        const bool bBlocked = bCombo || (!bDash && !bCooling && !bAvailable);
        const float Progress = bDash ? (Charges >= MaxCharges ? 1.f : HUD.GetDashRechargePercent()) : (bCooling ? 1.f-HUD.GetSwapCooldownProgress() : 1.f);
        const FText Status = bDash ? FText::FromString(FString::Printf(TEXT("%d / %d"),Charges,MaxCharges))
            : bCooling ? FText::FromString(FString::Printf(TEXT("%.1fs"),HUD.GetSwapCooldownRemaining())) : NSLOCTEXT("ActionHUD","SwapReady","SWAP");
        Badge->SetActionPresentation(bDash ? nullptr : HUD.SwapBadgeIcon.Get(), Progress, bAvailable, bBlocked, Label, Status,
            HUD.ComboLabelFont, HUD.ComboChargingColor, HUD.ComboReadyColor, bDash ? Charges : -1, bDash ? MaxCharges : 0);
    }
}
