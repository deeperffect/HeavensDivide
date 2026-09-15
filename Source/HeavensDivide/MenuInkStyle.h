#pragma once

#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Fonts/FontMeasure.h"

namespace MenuInkStyle
{
// Reserve space for the complete trailing strokes, outside the label area.
inline FMargin ContentPadding(float LabelWidth = 340.0f)
{
	// Keep the label in the first 70% of the full brush, before its sparse tail.
	return FMargin(18.0f, 12.0f, (LabelWidth + 18.0f) * (0.30f / 0.70f) + 12.0f, 12.0f);
}

inline FMargin ContentPadding(const FText& Label, const FSlateFontInfo& Font)
{
	return ContentPadding(FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Label, Font).X);
}

inline FSlateBrush MakeBrush(UTexture2D* Texture)
{
	FSlateBrush Brush;
	Brush.SetResourceObject(Texture);
	Brush.DrawAs = Texture ? ESlateBrushDrawType::Image : ESlateBrushDrawType::NoDrawType;
	Brush.ImageSize = FVector2D::ZeroVector; // The label, not the source texture, determines layout size.
	Brush.Tiling = ESlateBrushTileType::NoTile;
	// Use the entire texture, including its painted edges and trailing strokes.
	return Brush;
}
}
