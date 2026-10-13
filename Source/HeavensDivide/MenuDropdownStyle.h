#pragma once
#include "BloodshiftMenuStyle.h"
#include "MenuLockVisual.h"

namespace MenuDropdown
{
inline const FComboButtonStyle& ButtonStyle()
{
    static const auto Style=[]
    {
        auto Button=BloodshiftMenu::ActionStyle();
        const FSlateRoundedBoxBrush Outline(BloodshiftMenu::Surface,0.f,BloodshiftMenu::Gold,2.f);
        const FSlateRoundedBoxBrush Hover(FLinearColor(.035f,.065f,.070f),0.f,BloodshiftMenu::Gold,2.f);
        Button.SetNormal(Outline).SetDisabled(Outline).SetHovered(Hover).SetPressed(Hover)
            .SetHoveredForeground(BloodshiftMenu::Paper).SetPressedForeground(BloodshiftMenu::Paper);
        return FComboButtonStyle().SetButtonStyle(Button).SetMenuBorderBrush(Outline).SetMenuBorderPadding(FMargin(2));
    }();
    return Style;
}
inline const FComboBoxStyle& BoxStyle()
{
    static const auto Style=FComboBoxStyle().SetComboButtonStyle(ButtonStyle());
    return Style;
}
}
