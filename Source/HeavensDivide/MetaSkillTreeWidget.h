#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateTypes.h"
#include "MetaSkillTreeWidget.generated.h"

class UMainMenuWidget;

UCLASS()
class HEAVENSDIVIDE_API UMetaSkillTreeWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UMetaSkillTreeWidget(const FObjectInitializer& ObjectInitializer);
	TWeakObjectPtr<UMainMenuWidget> MenuOwner;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
private:
	FSlateFontInfo MenuFont(int32 Size, bool bHeading = false) const;
	UPROPERTY() FSlateFontInfo HeadingFont;
	UPROPERTY() FSlateFontInfo BodyFont;
	UPROPERTY() FSlateBrush PanelBrush;
	UPROPERTY() FSlateBrush DividerBrush;
	UPROPERTY() FSlateBrush VerticalDividerBrush;
	UPROPERTY() FButtonStyle MenuActionStyle;
	void CloseTree();
	FName Selected = TEXT("Root.Vitality");
	FString Message;
	bool bConfirmRefund = false;
};
