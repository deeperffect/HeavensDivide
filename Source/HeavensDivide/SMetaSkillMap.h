#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

class USynergyMetaProgressionSubsystem;
class SScrollBox;
struct FMetaSkillNode;
DECLARE_DELEGATE_OneParam(FOnMetaNodeSelected, FName);

/** Vertical skill forest. The same layout drives connectors, paint, hit tests and navigation. */
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
	/** An explicit set also filters pointer hit tests and keyboard navigation. */
	void SetVisibleNodes(const TArray<FName>& Ids, const FString& Title);
	void SetScrollTarget(TWeakPtr<SScrollBox> Target) { ScrollTarget=Target; }
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
	friend class FMetaSkillMapLayoutTest;
	friend class FMetaSkillMapFilterTest;
	bool IsNodeVisible(FName Id) const { return !bFiltered || VisibleNodes.Contains(Id); }
	void BuildLayout();
	FVector2D DisplayPosition(const FMetaSkillNode& Node) const;
	TArray<FVector2D> ConnectorPath(const FMetaSkillNode& Parent, const FMetaSkillNode& Child) const;
	double NodeLabelWidth(FName Id) const;
    TArray<FString> NodeLabelLines(const FMetaSkillNode& Node,int32& FontSize,double& LineHeight) const;
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
	TSet<FName> VisibleNodes;
	TMap<FName, FVector2D> LayoutPositions;
	TMap<FName, double> LabelWidths;
	FVector2D CanvasSize = FVector2D(1020,850);
	FString FilterTitle;
	bool bFiltered = false;
    bool bStatsLayout = false;
	FSlateFontInfo LabelFont;
	TWeakPtr<SScrollBox> ScrollTarget;
};
