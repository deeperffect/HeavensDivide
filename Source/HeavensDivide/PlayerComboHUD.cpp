#include "PlayerHUDWidget.h"
#include "Blueprint/WidgetTree.h"
#include "CharacterBase.h"
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
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

UPlayerHUDWidget::UPlayerHUDWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    static ConstructorHelpers::FObjectFinder<UTexture2D> Ink(TEXT("/Game/HeavensDivide/Blueprints/UI/Combo/T_ComboInkSlash"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Glow(TEXT("/Game/HeavensDivide/Blueprints/UI/Combo/M_ComboInkGlow"));
    static ConstructorHelpers::FObjectFinder<UFont> Font(TEXT("/Game/Assets/Fonts/Cinzel-Medium_Font"));
    ComboInkTexture = Ink.Object;
    ComboGlowMaterial = Glow.Object;
    ComboLabelFont = FSlateFontInfo(Font.Object, 14);
}

void UPlayerHUDWidget::ConfigureComboMeterStyle()
{
    if (!ComboMeterBar) return;
    FProgressBarStyle Style = ComboMeterBar->GetWidgetStyle();
    Style.EnableFillAnimation = false;
    if (ComboInkTexture)
    {
        Style.BackgroundImage.SetResourceObject(ComboInkTexture);
        Style.BackgroundImage.DrawAs = ESlateBrushDrawType::Image;
        Style.BackgroundImage.TintColor = FSlateColor(FLinearColor(.15f, .13f, .10f, .95f));
        Style.BackgroundImage.ImageSize = FVector2D(360, 80);
        Style.FillImage = Style.BackgroundImage;
        Style.FillImage.TintColor = FSlateColor(FLinearColor::White);
        if (ComboGlowMaterial)
        {
            ComboGlowInstance = UMaterialInstanceDynamic::Create(ComboGlowMaterial, this);
            ComboGlowInstance->SetTextureParameterValue(TEXT("InkTexture"), ComboInkTexture);
            ComboGlowInstance->SetScalarParameterValue(TEXT("GlowStrength"), 0.f);
            Style.FillImage.SetResourceObject(ComboGlowInstance);
        }
        ComboMeterBar->SetWidgetStyle(Style);
        ComboMeterBar->SetBarFillStyle(EProgressBarFillStyle::Mask);
        ComboMeterBar->SetBarFillType(EProgressBarFillType::LeftToRight);
        ComboMeterBar->SetBorderPadding(FVector2D::ZeroVector);
    }
    if (ComboAbilityText)
    {
        ComboAbilityText->SetFont(ComboLabelFont);
        ComboAbilityText->SetShadowColorAndOpacity(FLinearColor::Black);
        ComboAbilityText->SetShadowOffset(FVector2D(1, 2));
    }
}

float UPlayerHUDWidget::GetComboPercent() const
{
    auto* Combo = SurvivorPlayerController ? SurvivorPlayerController->FindComponentByClass<UComboAbilityComponent>() : nullptr;
    return Combo ? Combo->GetComboPercent() : 0.f;
}
bool UPlayerHUDWidget::IsComboReady() const
{
    auto* Combo = SurvivorPlayerController ? SurvivorPlayerController->FindComponentByClass<UComboAbilityComponent>() : nullptr;
    return Combo && Combo->IsFull();
}
void UPlayerHUDWidget::EnsureComboPresentation()
{
    if (!bShowComboMeter || !WidgetTree || !WidgetTree->RootWidget) return;
    if (ComboMeterBar) { ConfigureComboMeterStyle(); return; }
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
    if (!ComboAbilityText)
    {
        ComboAbilityText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ComboAbilityText"));
        ComboAbilityText->SetJustification(ETextJustify::Center);
        Stack->AddChildToVerticalBox(ComboAbilityText)->SetPadding(FMargin(0, 0, 0, -16));
    }
    ComboMeterBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("ComboMeterBar"));
    Stack->AddChildToVerticalBox(ComboMeterBar)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    ConfigureComboMeterStyle();
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
    const bool bReady = IsComboReady();
    const float RealTime = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.f;
    const float Glow = bReady ? ComboGlowIntensity * (.7f + .25f * FMath::Sin(RealTime * ComboGlowPulseSpeed * 2.f * PI)) : 0.f;
    if (ComboGlowInstance) ComboGlowInstance->SetScalarParameterValue(TEXT("GlowStrength"), Glow);
    if (ComboMeterBar)
    {
        ComboMeterBar->SetPercent(GetComboPercent());
        ComboMeterBar->SetFillColorAndOpacity(bReady ? ComboReadyColor : ComboChargingColor);
    }
    if (ComboAbilityText)
    {
        ComboAbilityText->SetColorAndOpacity(FSlateColor(bReady ? ComboReadyColor : FLinearColor(.9f, .87f, .8f)));
        const auto* Character = GetActiveCharacter();
        const auto* Settings = UHeavensDivideGameUserSettings::GetHeavensDivideGameUserSettings();
        const FKey Key = Settings ? Settings->GetKeyBinding(TEXT("ComboAbility")) : EKeys::Q;
        const FText Name = Character ? Character->ComboAbility.DisplayName : FText::FromString(TEXT("Combo"));
        ComboAbilityText->SetText(FText::Format(NSLOCTEXT("Combo", "HUD", "{0} [{1}]  {2}"), Name,
            Key.GetDisplayName(), IsComboReady() ? NSLOCTEXT("Combo", "Ready", "READY") : FText::AsPercent(GetComboPercent(), &FNumberFormattingOptions::DefaultNoGrouping())));
    }
}
