#include "PlayerHUDWidget.h"
#include "Blueprint/WidgetTree.h"
#include "CharacterBase.h"
#include "Components/HorizontalBox.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "HeavensDivideGameUserSettings.h"

// The existing Blueprint widgets retain their charge/cooldown event bindings.
// Apply state colors after those bindings and keep the captions in sync with rebinding.
void UpdatePlayerActionHUD(UPlayerHUDWidget& HUD)
{
    if (!HUD.WidgetTree) return;
    const auto* Settings = UHeavensDivideGameUserSettings::GetHeavensDivideGameUserSettings();
    const FLinearColor Ivory(.83f, .78f, .65f);
    const FLinearColor Gold(1.f, .65f, .1f);
    const bool bAbilityActive = HUD.GetActiveCharacter() && HUD.GetActiveCharacter()->bComboAbilityActive;
    for (const FName Action : {FName(TEXT("Dash")), FName(TEXT("Swap"))})
    {
        const FName WidgetName = Action == TEXT("Dash") ? TEXT("TXT_DashAction") : TEXT("TXT_SwapAction");
        if (auto* Label = Cast<UTextBlock>(HUD.WidgetTree->FindWidget(WidgetName)))
        {
            const FKey Key = Settings ? Settings->GetKeyBinding(Action) : (Action == TEXT("Dash") ? EKeys::SpaceBar : EKeys::Tab);
            FText KeyLabel = Key.GetDisplayName();
            if (Key == EKeys::SpaceBar) KeyLabel = NSLOCTEXT("ActionHUD", "Space", "Space");
            else if (Key == EKeys::RightMouseButton) KeyLabel = NSLOCTEXT("ActionHUD", "RMB", "RMB");
            else if (Key == EKeys::LeftMouseButton) KeyLabel = NSLOCTEXT("ActionHUD", "LMB", "LMB");
            else if (Key == EKeys::MiddleMouseButton) KeyLabel = NSLOCTEXT("ActionHUD", "MMB", "MMB");
            Label->SetText(FText::Format(NSLOCTEXT("ActionHUD", "KeyLabel", "{0} [{1}]"),
                Action == TEXT("Dash") ? NSLOCTEXT("ActionHUD", "Dash", "Dash") : NSLOCTEXT("ActionHUD", "Swap", "Swap"), KeyLabel));
        }
    }
    if (auto* Charges = Cast<UHorizontalBox>(HUD.WidgetTree->FindWidget(TEXT("DashChargeContainer"))))
    {
        if (auto* Label = HUD.WidgetTree->FindWidget(TEXT("TXT_DashAction")))
            if (auto* Slot = Cast<UCanvasPanelSlot>(Label->Slot))
            {
                Slot->SetAlignment(FVector2D(.5f, 0.f));
                Slot->SetPosition(FVector2D(-216.f - 30.f * FMath::Max(1, Charges->GetChildrenCount()), -252.f));
            }
        for (int32 Index = 0; Index < Charges->GetChildrenCount(); ++Index)
        {
            auto* Charge = Cast<UUserWidget>(Charges->GetChildAt(Index));
            if (!Charge || !Charge->WidgetTree) continue;
            if (auto* Fill = Cast<UProgressBar>(Charge->WidgetTree->FindWidget(TEXT("ChargeFull"))))
                Fill->SetFillColorAndOpacity(Index == HUD.GetRechargingDashSlotIndex() ? Gold : Ivory);
            Charge->SetRenderOpacity(bAbilityActive ? .45f : 1.f);
        }
    }
    if (auto* Swap = Cast<UUserWidget>(HUD.WidgetTree->FindWidget(TEXT("SwapCooldownWidget"))))
        if (Swap->WidgetTree)
            if (auto* Icon = Cast<UImage>(Swap->WidgetTree->FindWidget(TEXT("IMG_SwapIcon"))))
            {
                const bool bCooling = HUD.IsSwapCoolingDown();
                Icon->SetColorAndOpacity(bCooling || bAbilityActive ? Ivory : Gold);
                Icon->SetRenderOpacity(bAbilityActive ? .3f : (bCooling ? .45f : 1.f));
            }
}
