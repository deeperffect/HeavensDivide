#pragma once

#include "CoreMinimal.h"
#include "MenuFeedbackWidget.h"
#include "Styling/SlateTypes.h"
#include "MetaSkillTreeWidget.generated.h"

class UMainMenuWidget;
class USynergyMetaProgressionSubsystem;
class UUpgradeDefinition;
class SBox;
class SButton;
class SMetaSkillMap;

enum class EMetaTreePage : uint8 { Stances, Upgrades, Abilities, Stats };

UCLASS()
class HEAVENSDIVIDE_API UMetaSkillTreeWidget : public UMenuFeedbackWidget
{
	GENERATED_BODY()
public:
	UMetaSkillTreeWidget(const FObjectInitializer& ObjectInitializer);
	TWeakObjectPtr<UMainMenuWidget> MenuOwner;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FMetaSkillTreeTest;
#endif
	FSlateFontInfo MenuFont(int32 Size, bool bHeading = false) const;
	UPROPERTY() FSlateFontInfo HeadingFont;
	UPROPERTY() FSlateFontInfo BodyFont;
	UPROPERTY() FSlateBrush PanelBrush;
	UPROPERTY() FSlateBrush DividerBrush;
	UPROPERTY() FSlateBrush VerticalDividerBrush;
	UPROPERTY() FButtonStyle MenuActionStyle;
	UPROPERTY() TArray<FSlateBrush> StanceArtwork;
	UPROPERTY() TArray<TObjectPtr<UUpgradeDefinition>> SynergyDefinitions;
	UPROPERTY() TArray<FSlateBrush> SynergyArtwork;
	USynergyMetaProgressionSubsystem* Progression() const;
	void SelectPage(EMetaTreePage Page);
	FReply OpenStanceTree(FName Id);
	void RefreshPage();
	TSharedRef<SWidget> BuildSkillPage();
	TSharedRef<SWidget> BuildStancePage();
	TSharedRef<SWidget> BuildUpgradePage();
	TArray<FName> VisibleSkills() const;
	TSharedPtr<SBox> PageHost;
	TSharedPtr<SMetaSkillMap> SkillMap;
	TSharedPtr<SButton> StancesTab;
	EMetaTreePage CurrentPage = EMetaTreePage::Stats;
	FName OpenStance;
	FReply CloseTree();
	FName Selected = TEXT("Root.Vitality");
	FString Message;
	bool bConfirmRefund = false;
};
