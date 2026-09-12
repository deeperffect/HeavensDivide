#include "MetaSkillTreeWidget.h"
#include "MetaSkillTree.h"
#include "SMetaSkillMap.h"
#include "MainMenuWidget.h"
#include "SynergyMetaProgressionSubsystem.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "InputCoreTypes.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
 const FLinearColor Paper(.93f,.93f,.90f);
 const FLinearColor Muted(.58f,.58f,.55f);
 const FLinearColor Ember(.82f,.80f,.73f);
 const TCHAR* PathNames[]={TEXT("SHARED ROOTS"),TEXT("WAY OF STEEL"),TEXT("WAY OF SHADOW"),TEXT("TWIN SOUL BOND")};
}

UMetaSkillTreeWidget::UMetaSkillTreeWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
 static ConstructorHelpers::FObjectFinder<UObject> Heading(TEXT("/Game/Assets/Fonts/Cinzel-Medium_Font.Cinzel-Medium_Font"));
 static ConstructorHelpers::FObjectFinder<UObject> Body(TEXT("/Game/Assets/Fonts/Cinzel-Regular_Font.Cinzel-Regular_Font"));
 static ConstructorHelpers::FObjectFinder<UTexture2D> Panel(TEXT("/Game/HeavensDivide/Blueprints/UI/SkillTree/AscensionPanel.AscensionPanel"));
 static ConstructorHelpers::FObjectFinder<UTexture2D> Ink(TEXT("/Game/HeavensDivide/Blueprints/UI/MainMenu/InkHover.InkHover"));
 static ConstructorHelpers::FObjectFinder<UTexture2D> Divider(TEXT("/Game/HeavensDivide/Blueprints/UI/MainMenu/MenuBrushStroke.MenuBrushStroke"));
 static ConstructorHelpers::FObjectFinder<UTexture2D> VerticalDivider(TEXT("/Game/HeavensDivide/Blueprints/UI/MainMenu/MenuDivider.MenuDivider"));
 HeadingFont=FSlateFontInfo(Heading.Object,24,TEXT("Default"));
 BodyFont=FSlateFontInfo(Body.Object,16,TEXT("Default"));
 PanelBrush.SetResourceObject(Panel.Object);
 PanelBrush.ImageSize=FVector2D(1536,1024);
 DividerBrush.SetResourceObject(Divider.Object);
 DividerBrush.ImageSize=FVector2D(300,8);
 VerticalDividerBrush.SetResourceObject(VerticalDivider.Object);
 VerticalDividerBrush.ImageSize=FVector2D(6,600);
 FSlateBrush Normal;
 Normal.DrawAs=ESlateBrushDrawType::NoDrawType;
 FSlateBrush Hover;
 Hover.SetResourceObject(Ink.Object);
 Hover.ImageSize=FVector2D(300,56);
 Hover.TintColor=FLinearColor::White;
 MenuActionStyle.SetNormal(Normal).SetHovered(Hover).SetPressed(Hover).SetDisabled(Normal)
  .SetNormalForeground(Paper).SetHoveredForeground(FLinearColor::Black).SetPressedForeground(FLinearColor::Black)
  .SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
}

FSlateFontInfo UMetaSkillTreeWidget::MenuFont(int32 Size,bool bHeading) const
{
 FSlateFontInfo Font=bHeading?HeadingFont:BodyFont;
 Font.Size=Size;
 return Font;
}

TSharedRef<SWidget> UMetaSkillTreeWidget::RebuildWidget()
{
 if(MenuOwner.IsValid()) PanelBrush.SetResourceObject(MenuOwner->GetPageBackgroundTexture());
 SetIsFocusable(true);
 const auto Meta=[this]() -> USynergyMetaProgressionSubsystem* {return GetGameInstance()?GetGameInstance()->GetSubsystem<USynergyMetaProgressionSubsystem>():nullptr;};
 const auto Node=[this]() {return MetaSkillTree::Find(Selected);};
 TSharedRef<SMetaSkillMap> Map=SNew(SMetaSkillMap)
  .Progression(Meta()).Selected_Lambda([this](){return Selected;})
  .OnSelected_Lambda([this](FName Id){Selected=Id;Message.Empty();bConfirmRefund=false;});

 TSharedPtr<SButton> LearnButton;
 TSharedRef<SVerticalBox> Inspector=SNew(SVerticalBox)
  +SVerticalBox::Slot().AutoHeight().Padding(0,26,0,18)
  [SNew(STextBlock).Font(MenuFont(11)).ColorAndOpacity(Ember)
   .Text_Lambda([Node](){return FText::FromString(Node()?PathNames[Node()->Branch]:TEXT(""));})]
  +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,16)
  [SNew(STextBlock).Font(MenuFont(27,true)).ColorAndOpacity(Paper).WrapTextAt(290)
   .Text_Lambda([Node](){return FText::FromString(Node()?Node()->Name:TEXT("Choose a skill"));})]
  +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,18)
  [SNew(SSeparator).SeparatorImage(&DividerBrush).ColorAndOpacity(Muted).Thickness(6)]
  +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,24)
  [SNew(STextBlock).Font(MenuFont(13)).ColorAndOpacity(Ember)
   .Text_Lambda([this,Meta,Node](){const auto* M=Meta();const auto* N=Node();return FText::FromString(N&&M?FString::Printf(TEXT("RANK  %d / %d     |     %s"),M->GetSkillRank(Selected),N->MaxRank,M->GetSkillRank(Selected)==N->MaxRank?TEXT("MASTERED"):M->GetSkillRank(Selected)>0?TEXT("LEARNED"):M->GetSkillPurchaseBlock(Selected).IsEmpty()?TEXT("AVAILABLE"):TEXT("UNLEARNED")):TEXT(""));})]
  +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,22)
  [SNew(STextBlock).Font(MenuFont(17)).ColorAndOpacity(Paper).WrapTextAt(290)
   .Text_Lambda([Node](){return FText::FromString(Node()?Node()->Description:TEXT("Inspect a node on the map."));})]
  +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,12)
  [SNew(STextBlock).Text(FText::FromString(TEXT("PERMANENT PASSIVE"))).Font(MenuFont(10)).ColorAndOpacity(Muted)]
  +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,24)
  [SNew(STextBlock).Font(MenuFont(13)).ColorAndOpacity(Muted).WrapTextAt(290)
   .Text_Lambda([Meta,Node](){const auto* M=Meta();const auto* N=Node();if(!M||!N)return FText::GetEmpty();FString S;if(N->Prerequisites.IsEmpty())S=TEXT("A starting node. No prerequisite skills.");else{S=TEXT("REQUIRES ONE RANK IN\n");for(FName Id:N->Prerequisites)S+=FString(M->GetSkillRank(Id)>0?TEXT("+  "):TEXT("-  "))+MetaSkillTree::Find(Id)->Name+TEXT("\n");}return FText::FromString(S);})]
  +SVerticalBox::Slot().FillHeight(1)
  [SNew(STextBlock).Font(MenuFont(13)).ColorAndOpacity(Ember).WrapTextAt(290)
   .Text_Lambda([this,Meta](){return FText::FromString(!Message.IsEmpty()?Message:Meta()?Meta()->GetSkillPurchaseBlock(Selected):TEXT("Progression unavailable."));})]
  +SVerticalBox::Slot().AutoHeight().Padding(0,12,0,10)
  [SAssignNew(LearnButton,SButton).ButtonStyle(&MenuActionStyle).ContentPadding(16).HAlign(HAlign_Center)
   .IsEnabled_Lambda([this,Meta](){return Meta()&&Meta()->GetSkillPurchaseBlock(Selected).IsEmpty();})
   .OnClicked_Lambda([this,Meta,Map](){if(auto* M=Meta())Message=M->PurchaseSkill(Selected)?TEXT("Learned. Active in every new run."):TEXT("Could not save. No Soul Embers were spent; try again.");bConfirmRefund=false;Map->Refresh();return FReply::Handled();})
   [SNew(STextBlock).Font(MenuFont(14,true)).ColorAndOpacity(FSlateColor::UseForeground())
    .Text_Lambda([this,Meta](){const int32 Cost=Meta()?Meta()->GetSkillCost(Selected):0;return FText::FromString(Cost>0?FString::Printf(TEXT("LEARN   /   %d EMBERS"),Cost):TEXT("FULLY LEARNED"));})]]
  +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,24)
  [SNew(STextBlock).Text(FText::FromString(TEXT("One rank opens the next node.\nYour choices can be refunded in full."))).Font(MenuFont(11)).ColorAndOpacity(Muted).WrapTextAt(290)];

 Map->SetInspectTarget(LearnButton);
 Map->SetLabelFont(MenuFont(12,true));
 return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("NoBrush")).Padding(0)
 [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
  [SNew(SBox).WidthOverride(1440).HeightOverride(920)
   [SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(48,56))
   [SNew(SVerticalBox)
    +SVerticalBox::Slot().AutoHeight().Padding(28,22,28,8)
    [SNew(SHorizontalBox)
     +SHorizontalBox::Slot().FillWidth(1)
     [SNew(SVerticalBox)
      +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(TEXT("TWIN SOUL"))).Font(MenuFont(28,true)).ColorAndOpacity(Paper)]
      +SVerticalBox::Slot().AutoHeight().Padding(0,7,0,0)[SNew(STextBlock).Text(FText::FromString(TEXT("PATHS OF ASCENSION"))).Font(MenuFont(10)).ColorAndOpacity(Muted)]]
     +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
     [SNew(SVerticalBox)
      +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)[SNew(STextBlock).Font(MenuFont(27)).ColorAndOpacity(Ember).Text_Lambda([Meta](){return FText::FromString(FString::Printf(TEXT("%d  SOUL EMBERS"),Meta()?Meta()->GetSoulEmbers():0));})]
      +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0,6,0,0)[SNew(STextBlock).Text(FText::FromString(TEXT("Earned through survival and victory"))).Font(MenuFont(11)).ColorAndOpacity(Muted)]]]
    +SVerticalBox::Slot().AutoHeight().Padding(28,12)
    [SNew(SSeparator).SeparatorImage(&DividerBrush).ColorAndOpacity(FLinearColor(.42f,.42f,.39f)).Thickness(8)]
    +SVerticalBox::Slot().FillHeight(1).Padding(28,0,12,0)
    [SNew(SHorizontalBox)
     +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(300)[Inspector]]
     +SHorizontalBox::Slot().AutoWidth().Padding(22,26,12,24)[SNew(SSeparator).Orientation(Orient_Vertical).SeparatorImage(&VerticalDividerBrush).ColorAndOpacity(FLinearColor(.36f,.36f,.33f)).Thickness(6)]
     +SHorizontalBox::Slot().FillWidth(1)[Map]]
    +SVerticalBox::Slot().AutoHeight().Padding(28,6,28,12)
    [SNew(SHorizontalBox)
     +SHorizontalBox::Slot().AutoWidth().Padding(0,0,18,0)
     [SNew(SButton).ButtonStyle(&MenuActionStyle).ContentPadding(FMargin(22,12)).OnClicked_Lambda([this](){CloseTree();return FReply::Handled();})[SNew(STextBlock).Font(MenuFont(13,true)).Text(FText::FromString(TEXT("BACK"))).ColorAndOpacity(FSlateColor::UseForeground())]]
     +SHorizontalBox::Slot().AutoWidth()
     [SNew(SButton).ButtonStyle(&MenuActionStyle).ContentPadding(FMargin(18,12))
      .OnClicked_Lambda([this,Meta,Map](){if(!bConfirmRefund){bConfirmRefund=true;Message=TEXT("Refund every skill for its full paid cost? Select CONFIRM REFUND, or select a node to cancel.");}else{if(auto* M=Meta())Message=M->RefundSkills()?TEXT("All skill costs refunded."):TEXT("Could not save refund. Your skills are unchanged.");bConfirmRefund=false;Map->Refresh();}return FReply::Handled();})
      [SNew(STextBlock).Font(MenuFont(13,true)).ColorAndOpacity(FSlateColor::UseForeground()).Text_Lambda([this](){return FText::FromString(bConfirmRefund?TEXT("CONFIRM REFUND"):TEXT("REFUND SKILLS"));})]]
     +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).HAlign(HAlign_Center)
     [SNew(STextBlock).Text(FText::FromString(TEXT("Filled: learned   /   Bright: available   /   Dim: locked"))).Font(MenuFont(11)).ColorAndOpacity(Muted)]
     +SHorizontalBox::Slot().AutoWidth()
     [SNew(SButton).ButtonStyle(&MenuActionStyle).ContentPadding(FMargin(16,12)).OnClicked_Lambda([Map](){Map->ResetView();return FReply::Handled();})[SNew(STextBlock).Font(MenuFont(13,true)).Text(FText::FromString(TEXT("RESET VIEW"))).ColorAndOpacity(FSlateColor::UseForeground())]]]
    +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(28,0,28,14)
    [SNew(STextBlock).Text(FText::FromString(TEXT("Scroll: zoom   /   Drag: pan   /   Arrows or D-pad: select   /   Enter or A: learn"))).Font(MenuFont(9)).ColorAndOpacity(Muted)]
   ]]
  ]
 ];
}

void UMetaSkillTreeWidget::CloseTree()
{
 if(MenuOwner.IsValid())MenuOwner->ShowMainPanel();
 else RemoveFromParent();
}
FReply UMetaSkillTreeWidget::NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
 if(Event.GetKey()==EKeys::Escape || Event.GetKey()==EKeys::Gamepad_FaceButton_Right)
 {
  if(bConfirmRefund){bConfirmRefund=false;Message.Empty();}else CloseTree();
  return FReply::Handled();
 }
 return Super::NativeOnKeyDown(Geometry,Event);
}
