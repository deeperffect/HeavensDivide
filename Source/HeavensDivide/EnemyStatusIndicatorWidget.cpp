// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemyStatusIndicatorWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"

UEnemyStatusIndicatorWidget::UEnemyStatusIndicatorWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UTexture2D> BleedIcon(TEXT("/Game/HeavensDivide/Blueprints/UI/EnemyUI/BleedIcon.BleedIcon"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> PoisonIcon(TEXT("/Game/HeavensDivide/Blueprints/UI/EnemyUI/PoisonIcon.PoisonIcon"));
	DefaultBleedIcon = BleedIcon.Object;
	DefaultPoisonIcon = PoisonIcon.Object;
}

void UEnemyStatusIndicatorWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree || WidgetTree->RootWidget) return;

	UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("StatusIconOverlay"));
	// The owning widget component supplies the size (small mobs and elites differ).
	// Let the icon fill that space instead of imposing a second, fixed 36px size.
	WidgetTree->RootWidget = Overlay;

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

	StatusIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("StatusIcon"));
	StatusIcon->SetVisibility(ESlateVisibility::Collapsed);
	if (UOverlaySlot* IconSlot = Overlay->AddChildToOverlay(StatusIcon))
	{
		IconSlot->SetHorizontalAlignment(HAlign_Fill);
		IconSlot->SetVerticalAlignment(VAlign_Fill);
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

void UEnemyStatusIndicatorWidget::SetStatusPresentation(EEnemyStatusEffect Status, int32 StackCount, bool bShowCountAtOne, int32 FontSize, UTexture2D* Icon)
{
	if (!IconBackground || !StatusGlyph || !StackCountText) return;
	// Older/newly created enemy Blueprints can have no per-enemy override.
	// Resolve that to the same artwork used by the rest of the roster.
	if (!Icon) Icon = Status == EEnemyStatusEffect::Bleed ? DefaultBleedIcon.Get() : DefaultPoisonIcon.Get();
	if (StatusIcon)
	{
		StatusIcon->SetBrushFromTexture(Icon, false);
		StatusIcon->SetVisibility(Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	IconBackground->SetVisibility(Icon ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	StatusGlyph->SetVisibility(Icon ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
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
