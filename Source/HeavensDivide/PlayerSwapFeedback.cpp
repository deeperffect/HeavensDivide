#include "PlayerHUDWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "NinjaCharacter.h"
void UPlayerHUDWidget::ClearSwapPortraitPulse()
{
    if(PulsingPortrait.IsValid())
    {
        PulsingPortrait->SetRenderScale(OriginalPortraitScale);
        PulsingPortrait->SetColorAndOpacity(OriginalPortraitColor);
    }
    PulsingPortrait.Reset(); PortraitPulseRemaining=0;
}
void UPlayerHUDWidget::StartSwapPortraitPulse(ACharacterBase* Character)
{
    if(!bEnableSwapPortraitPulse || !WidgetTree || !Character) return;
    auto* Portrait=Cast<UImage>(WidgetTree->FindWidget(Cast<ANinjaCharacter>(Character) ? NinjaPortraitName : SamuraiPortraitName));
    if(!Portrait) return;
    PulsingPortrait=Portrait;
    OriginalPortraitScale=Portrait->GetRenderTransform().Scale;
    OriginalPortraitColor=Portrait->GetColorAndOpacity();
    PortraitPulseRemaining=FMath::Max(.01f,SwapPortraitDuration);
    UpdateSwapPortraitPulse(0);
}
void UPlayerHUDWidget::UpdateSwapPortraitPulse(float Delta)
{
    if(!PulsingPortrait.IsValid()) return;
    PortraitPulseRemaining=FMath::Max(0.f,PortraitPulseRemaining-Delta);
    if(!bEnableSwapPortraitPulse || PortraitPulseRemaining<=0) { ClearSwapPortraitPulse(); return; }
    const float Alpha=PortraitPulseRemaining/FMath::Max(.01f,SwapPortraitDuration);
    PulsingPortrait->SetRenderScale(OriginalPortraitScale*FMath::Lerp(1.f,SwapPortraitScale,Alpha));
    FLinearColor Color=OriginalPortraitColor*(1+.75f*Alpha); Color.A=OriginalPortraitColor.A;
    PulsingPortrait->SetColorAndOpacity(Color);
}
