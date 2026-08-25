// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemyStatusIndicatorWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"

void UEnemyStatusIndicatorWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree || WidgetTree->RootWidget) return;

	USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("StatusIconSize"));
	SizeBox->SetWidthOverride(36.0f);
	SizeBox->SetHeightOverride(36.0f);
	WidgetTree->RootWidget = SizeBox;

	UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("StatusIconOverlay"));
	SizeBox->SetContent(Overlay);

	IconBackground = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("IconBackground"));
	IconBackground->SetPadding(FMargin(2.0f));
	if (UOverlaySlot* OverlaySlot = Overlay->AddChildToOverlay(IconBackground))
	{
		OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
		OverlaySlot->SetVerticalAlignment(VAlign_Fill);
	}

	StatusGlyph = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusGlyph"));
	StatusGlyph->SetJustification(ETextJustify::Center);
	StatusGlyph->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	FSlateFontInfo GlyphFont = StatusGlyph->GetFont();
	GlyphFont.Size = 16;
	GlyphFont.OutlineSettings.OutlineSize = 1;
	GlyphFont.OutlineSettings.OutlineColor = FLinearColor::Black;
	StatusGlyph->SetFont(GlyphFont);
	if (UOverlaySlot* OverlaySlot = Overlay->AddChildToOverlay(StatusGlyph))
	{
		OverlaySlot->SetHorizontalAlignment(HAlign_Center);
		OverlaySlot->SetVerticalAlignment(VAlign_Center);
	}

	StackCountText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StackCountText"));
	StackCountText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	StackCountText->SetShadowOffset(FVector2D(1.0f, 1.0f));
	StackCountText->SetShadowColorAndOpacity(FLinearColor::Black);
	if (UOverlaySlot* OverlaySlot = Overlay->AddChildToOverlay(StackCountText))
	{
		OverlaySlot->SetHorizontalAlignment(HAlign_Right);
		OverlaySlot->SetVerticalAlignment(VAlign_Bottom);
		OverlaySlot->SetPadding(FMargin(0.0f, 0.0f, 2.0f, 0.0f));
	}
}

void UEnemyStatusIndicatorWidget::SetStatusPresentation(EEnemyStatusEffect Status, int32 StackCount, bool bShowCountAtOne, int32 FontSize)
{
	if (!IconBackground || !StatusGlyph || !StackCountText) return;
	const bool bBleed = Status == EEnemyStatusEffect::Bleed;
	IconBackground->SetBrushColor(bBleed ? FLinearColor(0.55f, 0.015f, 0.02f, 0.92f) : FLinearColor(0.03f, 0.42f, 0.04f, 0.92f));
	StatusGlyph->SetText(FText::FromString(bBleed ? TEXT("B") : TEXT("P")));
	StackCountText->SetText(FText::AsNumber(FMath::Max(0, StackCount)));
	StackCountText->SetVisibility(StackCount > 1 || (bShowCountAtOne && StackCount == 1) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	FSlateFontInfo StackFont = StackCountText->GetFont();
	const int32 DigitCount = FString::FromInt(FMath::Max(0, StackCount)).Len();
	const int32 LargeCountReduction = FMath::Max(0, DigitCount - 1) * 2;
	StackFont.Size = FMath::Clamp(FontSize - LargeCountReduction, 6, 32);
	StackFont.OutlineSettings.OutlineSize = 1;
	StackFont.OutlineSettings.OutlineColor = FLinearColor::Black;
	StackCountText->SetFont(StackFont);
}
