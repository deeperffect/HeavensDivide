#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

class USynergyMetaProgressionSubsystem;
DECLARE_DELEGATE_OneParam(FOnMetaNodeSelected, FName);

/** Vector-drawn constellation. The same transform drives paint, hit tests and navigation. */
class SMetaSkillMap : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SMetaSkillMap) {}
		SLATE_ARGUMENT(TWeakObjectPtr<USynergyMetaProgressionSubsystem>, Progression)
		SLATE_ATTRIBUTE(FName, Selected)
		SLATE_EVENT(FOnMetaNodeSelected, OnSelected)
	SLATE_END_ARGS()
	void Construct(const FArguments& Args);
	void ResetView();
	void SetInspectTarget(TWeakPtr<SWidget> Target) { InspectTarget=Target; }
	void Refresh() { Invalidate(EInvalidateWidgetReason::Paint); }
	void SetLabelFont(const FSlateFontInfo& Font) { LabelFont=Font; Refresh(); }
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual int32 OnPaint(const FPaintArgs&, const FGeometry&, const FSlateRect&, FSlateWindowElementList&, int32, const FWidgetStyle&, bool) const override;
	virtual FReply OnMouseMove(const FGeometry&, const FPointerEvent&) override;
	virtual void OnMouseLeave(const FPointerEvent&) override;
	virtual FReply OnMouseButtonDown(const FGeometry&, const FPointerEvent&) override;
	virtual FReply OnMouseButtonUp(const FGeometry&, const FPointerEvent&) override;
	virtual FReply OnMouseWheel(const FGeometry&, const FPointerEvent&) override;
	virtual FReply OnKeyDown(const FGeometry&, const FKeyEvent&) override;
private:
	friend class FMetaSkillMapHighlightTest;
	TSet<FName> GetHighlightedAncestors() const;
	void SetHoveredNode(FName Id);
	FName HitNode(const FGeometry&, FVector2D Local) const;
	FVector2D ToScreen(const FGeometry&, FVector2D Point) const;
	FVector2D ToMap(const FGeometry&, FVector2D Point) const;
	float ViewScale(const FGeometry&) const;
	void Select(FName Id);
	TWeakObjectPtr<USynergyMetaProgressionSubsystem> Progression;
	TWeakPtr<SWidget> InspectTarget;
	TAttribute<FName> Selected;
	FOnMetaNodeSelected OnSelected;
	FName Hovered;
	FSlateFontInfo LabelFont;
	FVector2D Pan = FVector2D::ZeroVector;
	FVector2D LastDrag = FVector2D::ZeroVector;
	float Zoom = 1.f;
	bool bDragging = false;
};

