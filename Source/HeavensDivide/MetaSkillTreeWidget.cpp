#include "MetaSkillTreeWidget.h"
#include "BloodshiftMenuStyle.h"
#include "MetaSkillTree.h"
#include "SMetaSkillMap.h"
#include "MainMenuWidget.h"
#include "SynergyMetaProgressionSubsystem.h"
#include "RunStanceCatalog.h"
#include "PlayerUpgradeComponent.h"
#include "SurvivorPlayerController.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "InputCoreTypes.h"

namespace
{
 const FLinearColor Paper=BloodshiftMenu::Paper;
 const FLinearColor Muted=BloodshiftMenu::Muted;
 const FLinearColor Ember=BloodshiftMenu::Gold;
 const TCHAR* PageName(EMetaTreePage Page)
 {
  switch(Page)
  {
   case EMetaTreePage::Stances:return TEXT("STANCES");
   case EMetaTreePage::Upgrades:return TEXT("UPGRADES");
   case EMetaTreePage::Abilities:return TEXT("ABILITIES");
   default:return TEXT("STATS");
  }
 }
 FName SynergyId(const UUpgradeDefinition* Definition)
 {return Definition->MetaUnlockId.IsNone()?Definition->UpgradeId:Definition->MetaUnlockId;}
 FSlateBrush Artwork(UUpgradeDefinition* Definition)
 {
  auto* Texture=Definition?(Definition->CardArtwork?Definition->CardArtwork.Get():Definition->Icon.Get()):nullptr;
  FSlateBrush Brush;Brush.ImageSize=Texture?FVector2D(Texture->GetSizeX(),Texture->GetSizeY()):FVector2D(256,384);
  Brush.SetResourceObject(Texture);return Brush;
 }
}

UMetaSkillTreeWidget::UMetaSkillTreeWidget(const FObjectInitializer& Initializer):Super(Initializer)
{
 static ConstructorHelpers::FObjectFinder<UObject> Heading(TEXT("/Game/Assets/Fonts/Cinzel-Medium_Font.Cinzel-Medium_Font"));
 HeadingFont=FSlateFontInfo(Heading.Object,24,TEXT("Default"));
 BodyFont=BloodshiftMenu::BodyFont(20);
 MenuActionStyle=BloodshiftMenu::ActionStyle();
 SetIsFocusable(true);
}
FSlateFontInfo UMetaSkillTreeWidget::MenuFont(int32 Size,bool bHeading) const
{
 auto Font=bHeading?HeadingFont:BodyFont;Font.Size=Size;return Font;
}
USynergyMetaProgressionSubsystem* UMetaSkillTreeWidget::Progression() const
{return GetGameInstance()?GetGameInstance()->GetSubsystem<USynergyMetaProgressionSubsystem>():nullptr;}

TArray<FName> UMetaSkillTreeWidget::VisibleSkills() const
{
 TArray<FName> Result;
 for(const auto& Node:MetaSkillTree::Nodes())
 {
  const bool bVisible=CurrentPage==EMetaTreePage::Stances ? !OpenStance.IsNone()&&Node.RequiredStance==OpenStance :
   OpenStance.IsNone()&&Node.RequiredStance.IsNone()&&
   Node.Page==(CurrentPage==EMetaTreePage::Abilities?EMetaSkillPage::Abilities:EMetaSkillPage::Stats);
  if(bVisible)Result.Add(Node.Id);
 }
 return Result;
}
void UMetaSkillTreeWidget::SelectPage(EMetaTreePage Page)
{
 CurrentPage=Page;OpenStance=NAME_None;Message.Empty();bConfirmRefund=false;RefreshPage();
}
FReply UMetaSkillTreeWidget::OpenStanceTree(FName Id)
{
 auto* Meta=Progression();
 if(!Meta||(!Meta->IsStanceUnlocked(Id)&&!Meta->PurchaseStance(Id)))
 {
  Message=TEXT("Could not save the unlock. No Sigils were spent; try again.");
  return FReply::Handled();
 }
 CurrentPage=EMetaTreePage::Stances;OpenStance=Id;Message.Empty();bConfirmRefund=false;RefreshPage();
 return SkillMap.IsValid()?FReply::Handled().SetUserFocus(SkillMap.ToSharedRef(),EFocusCause::Navigation):FReply::Handled();
}
void UMetaSkillTreeWidget::RefreshPage()
{
 if(!PageHost)return;
 SkillMap.Reset();
 if(CurrentPage==EMetaTreePage::Stances&&OpenStance.IsNone())PageHost->SetContent(BuildStancePage());
 else if(CurrentPage==EMetaTreePage::Upgrades)PageHost->SetContent(BuildUpgradePage());
 else
 {
  const auto Visible=VisibleSkills();
  if(!Visible.Contains(Selected))Selected=Visible.IsEmpty()?NAME_None:Visible[0];
  PageHost->SetContent(BuildSkillPage());
 }
}

TSharedRef<SWidget> UMetaSkillTreeWidget::BuildStancePage()
{
 auto Grid=SNew(SUniformGridPanel).SlotPadding(FMargin(0,0,16,12));
 const TCHAR* Summaries[]={TEXT("Wide melee strikes that stack bleeding wounds."),TEXT("Charged slashes pull and mark foes for follow-up damage."),
  TEXT("Piercing blade waves damage and slow groups of foes."),TEXT("Seeking blades return. Fight close to attack faster."),
  TEXT("Rapid kunai volleys stack poison across groups."),TEXT("Heavy piercing blades hit enemies repeatedly.")};
 for(int32 I=0;I<UE_ARRAY_COUNT(RunStances::All);++I)
 {
  const auto& Stance=RunStances::All[I];const FName Id=Stance.Id;
  const FLinearColor Accent=Stance.bSamurai?FLinearColor(.85f,.26f,.16f):FLinearColor(.48f,.72f,.45f);
  Grid->AddSlot(I%3,I/3)
  [SNew(SBox).HeightOverride(240)
   [SNew(SOverlay)
    +SOverlay::Slot()[SNew(BloodshiftMenu::SInsetSurface).Visibility(EVisibility::HitTestInvisible)]
    +SOverlay::Slot().Padding(18)
    [SNew(SHorizontalBox)
     +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,18,0)
     [SNew(SBox).WidthOverride(110).HeightOverride(166)
      [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SImage).Image(&StanceArtwork[I])
       .ColorAndOpacity_Lambda([this,Id](){return Progression()&&Progression()->IsStanceUnlocked(Id)?FLinearColor::White:FLinearColor(.5f,.5f,.5f,1);})]]]
     +SHorizontalBox::Slot().FillWidth(1)
     [SNew(SVerticalBox)
      +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Stance.bSamurai?TEXT("SAMURAI"):TEXT("NINJA"))).Font(MenuFont(12)).ColorAndOpacity(Accent)]
      +SVerticalBox::Slot().AutoHeight().Padding(0,7,0,6)[SNew(STextBlock).Text(FText::FromString(Stance.Name)).Font(MenuFont(21,true)).ColorAndOpacity(Paper).WrapTextAt(285)]
      +SVerticalBox::Slot().FillHeight(1)[SNew(STextBlock).Text(FText::FromString(Summaries[I])).Font(MenuFont(15)).ColorAndOpacity(Muted).WrapTextAt(310)]
      +SVerticalBox::Slot().AutoHeight().Padding(0,4,0,8)
      [SNew(STextBlock).Font(MenuFont(13)).ColorAndOpacity(Ember).WrapTextAt(285)
       .Text_Lambda([this,Id](){const auto* M=Progression();return FText::FromString(!M?TEXT("Progression unavailable."):M->IsStanceUnlocked(Id)?TEXT("UNLOCKED / PERMANENT") : M->GetSoulSigils()<M->GetStanceCost(Id)?FString::Printf(TEXT("NEED %d MORE SIGILS"),M->GetStanceCost(Id)-M->GetSoulSigils()):TEXT("PERMANENT STANCE UNLOCK"));})]
      +SVerticalBox::Slot().AutoHeight()
      [SNew(SButton).ButtonStyle(&MenuActionStyle).ContentPadding(FMargin(8,2))
       .IsEnabled_Lambda([this,Id](){const auto* M=Progression();return M&&(M->IsStanceUnlocked(Id)||M->GetStancePurchaseBlock(Id).IsEmpty());})
       .OnClicked_Lambda([this,Id](){return OpenStanceTree(Id);})
       [SNew(STextBlock).Font(MenuFont(14)).ColorAndOpacity(FSlateColor::UseForeground())
        .Text_Lambda([this,Id](){const auto* M=Progression();return FText::FromString(M&&M->IsStanceUnlocked(Id)?TEXT("OPEN SKILL TREE"):FString::Printf(TEXT("UNLOCK / %d SIGILS"),M?M->GetStanceCost(Id):0));})]]]]]];
 }
 return SNew(SVerticalBox)
  +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,20)
  [SNew(STextBlock).Text(FText::FromString(TEXT("Blood and Barrage are yours from the start. Unlock new styles with Soul Sigils, then strengthen each stance with Soul Embers."))).Font(MenuFont(17)).ColorAndOpacity(Muted).AutoWrapText(true)]
  +SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox)+SScrollBox::Slot()[Grid]];
}

TSharedRef<SWidget> UMetaSkillTreeWidget::BuildUpgradePage()
{
 auto Grid=SNew(SUniformGridPanel).SlotPadding(FMargin(0,0,18,16));
 for(int32 I=0;I<SynergyDefinitions.Num();++I)
 {
  const auto* Definition=SynergyDefinitions[I].Get();if(!Definition)continue;
  const FName Id=SynergyId(Definition);
  Grid->AddSlot(I%2,I/2)
  [SNew(SBox).MinDesiredHeight(235)
   [SNew(SOverlay)
    +SOverlay::Slot()[SNew(BloodshiftMenu::SInsetSurface).Visibility(EVisibility::HitTestInvisible)]
    +SOverlay::Slot().Padding(20)
    [SNew(SHorizontalBox)
     +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,20,0)
     [SNew(SBox).WidthOverride(105).HeightOverride(158)[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SImage).Image(&SynergyArtwork[I])]]]
     +SHorizontalBox::Slot().FillWidth(1)
     [SNew(SVerticalBox)
      +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)[SNew(STextBlock).Text(Definition->DisplayName).Font(MenuFont(23,true)).ColorAndOpacity(Paper).WrapTextAt(570)]
      +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(Definition->GetBalanceDescription()).Font(MenuFont(16)).ColorAndOpacity(Muted).WrapTextAt(540)]
      +SVerticalBox::Slot().FillHeight(1)[SNew(SBox).MinDesiredHeight(10)]
      +SVerticalBox::Slot().AutoHeight().Padding(0,8,0,8)
      [SNew(STextBlock).Font(MenuFont(13)).ColorAndOpacity(Ember).WrapTextAt(530)
       .Text_Lambda([this,Id](){const auto* M=Progression();return FText::FromString(!M?TEXT("Progression unavailable."):M->IsSynergyUpgradeUnlocked(Id)?TEXT("UNLOCKED / CAN APPEAR IN RUNS"):M->GetSoulSigils()<M->GetSynergyCost(Id)?FString::Printf(TEXT("NEED %d MORE SIGILS"),M->GetSynergyCost(Id)-M->GetSoulSigils()):TEXT("PERMANENT SYNERGY UNLOCK"));})]
      +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
      [SNew(SButton).ButtonStyle(&MenuActionStyle).ContentPadding(FMargin(12,2))
       .IsEnabled_Lambda([this,Id](){const auto* M=Progression();return M&&!M->IsSynergyUpgradeUnlocked(Id)&&M->GetSynergyPurchaseBlock(Id).IsEmpty();})
       .OnClicked_Lambda([this,Id](){if(auto* M=Progression())Message=M->PurchaseSynergy(Id)?TEXT("Synergy unlocked. It can now appear among your run upgrades."):TEXT("Could not save the unlock. No Sigils were spent; try again.");return FReply::Handled();})
       [SNew(STextBlock).Font(MenuFont(15)).ColorAndOpacity(FSlateColor::UseForeground())
        .Text_Lambda([this,Id](){const auto* M=Progression();return FText::FromString(M&&M->IsSynergyUpgradeUnlocked(Id)?TEXT("UNLOCKED"):FString::Printf(TEXT("UNLOCK / %d SIGILS"),M?M->GetSynergyCost(Id):0));})]]]]]];
 }
 return SNew(SVerticalBox)
  +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,20)
  [SNew(STextBlock).Text(FText::FromString(TEXT("Tag Team is available from the start. Spend Soul Sigils to add more synergies to your run upgrade pool. Unlocking a synergy does not equip it."))).Font(MenuFont(17)).ColorAndOpacity(Muted).AutoWrapText(true)]
  +SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox)+SScrollBox::Slot()[Grid]];
}

TSharedRef<SWidget> UMetaSkillTreeWidget::BuildSkillPage()
{
 const auto Meta=[this](){return Progression();};
 const auto Node=[this](){return MetaSkillTree::Find(Selected);};
 SAssignNew(SkillMap,SMetaSkillMap).Progression(Progression()).Selected_Lambda([this](){return Selected;})
  .OnSelected_Lambda([this](FName Id){Selected=Id;Message.Empty();bConfirmRefund=false;});
 SkillMap->SetVisibleNodes(VisibleSkills(),OpenStance.IsNone()?FString(PageName(CurrentPage)):MetaSkillTree::StanceName(OpenStance));
 SkillMap->SetLabelFont(MenuFont(12,true));
 TSharedPtr<SButton> LearnButton;
 SAssignNew(LearnButton,SButton).ButtonStyle(&MenuActionStyle).ContentPadding(FMargin(8,12)).HAlign(HAlign_Center)
  .IsEnabled_Lambda([this,Meta](){return Meta()&&Meta()->GetSkillPurchaseBlock(Selected).IsEmpty();})
  .OnClicked_Lambda([this,Meta](){if(auto* M=Meta())Message=M->PurchaseSkill(Selected)?TEXT("Skill learned. Your permanent bonus is saved."):TEXT("Could not save. No Soul Embers were spent; try again.");bConfirmRefund=false;if(SkillMap)SkillMap->Refresh();return FReply::Handled();})
  [SNew(STextBlock).Font(MenuFont(15)).ColorAndOpacity(FSlateColor::UseForeground())
   .Text_Lambda([this,Meta](){const int32 Cost=Meta()?Meta()->GetSkillCost(Selected):0;return FText::FromString(Cost>0?FString::Printf(TEXT("LEARN / %d EMBERS"),Cost):TEXT("FULLY LEARNED"));})];
 SkillMap->SetInspectTarget(LearnButton);
 auto Inspector=SNew(SVerticalBox)
  +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,12)
  [SNew(STextBlock).Font(MenuFont(14)).ColorAndOpacity(Ember).Text_Lambda([this](){return FText::FromString(OpenStance.IsNone()?FString(PageName(CurrentPage)):MetaSkillTree::StanceName(OpenStance).ToUpper()+TEXT(" / STANCE SKILLS"));})]
  +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,16)
  [SNew(STextBlock).Font(MenuFont(25,true)).ColorAndOpacity(Paper).WrapTextAt(300).Text_Lambda([Node](){return FText::FromString(Node()?Node()->Name:TEXT("Choose a skill"));})]
  +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,18)[SNew(BloodshiftMenu::SOrnamentDivider)]
  +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,18)
  [SNew(STextBlock).Font(MenuFont(16)).ColorAndOpacity(Ember).Text_Lambda([this,Meta,Node](){return FText::FromString(Node()&&Meta()?FString::Printf(TEXT("RANK %d / %d"),Meta()->GetSkillRank(Selected),Node()->MaxRank):TEXT(""));})]
  +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,24)
  [SNew(STextBlock).Font(MenuFont(17)).ColorAndOpacity(Paper).WrapTextAt(300).Text_Lambda([Node](){return FText::FromString(Node()?MetaSkillTree::Description(*Node()):TEXT("Inspect a skill on the map."));})]
  +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)
  [SNew(STextBlock).Font(MenuFont(15)).ColorAndOpacity(Ember).WrapTextAt(300).Text_Lambda([Node](){return FText::FromString(Node()?MetaSkillTree::Scope(*Node()):TEXT("PERMANENT PASSIVE"));})]
  +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,18)
  [SNew(STextBlock).Font(MenuFont(14)).ColorAndOpacity(Muted).WrapTextAt(300).Text_Lambda([Node](){return FText::FromString(Node()&&!Node()->RequiredStance.IsNone()?TEXT("Active when this stance is selected, including while the hero assists."):TEXT("Applies automatically in every new run."));})]
  +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,18)
  [SNew(STextBlock).Font(MenuFont(14)).ColorAndOpacity(Muted).WrapTextAt(300)
   .Text_Lambda([Meta,Node](){if(!Node()||!Meta())return FText::GetEmpty();FString Text;if(Node()->Prerequisites.IsEmpty())Text=TEXT("Starting skill / No prerequisite.");else{Text=TEXT("REQUIRES ONE RANK IN\n");for(FName Id:Node()->Prerequisites)Text+=(Meta()->GetSkillRank(Id)>0?TEXT("Learned: "):TEXT("Required: "))+(MetaSkillTree::Find(Id)?MetaSkillTree::Find(Id)->Name:Id.ToString())+TEXT("\n");}return FText::FromString(Text);})]
  +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(MenuFont(14)).ColorAndOpacity(Ember).WrapTextAt(300).Text_Lambda([this,Meta](){return FText::FromString(Meta()?Meta()->GetSkillPurchaseBlock(Selected):TEXT("Progression unavailable."));})];
 auto Header=SNew(SHorizontalBox);
 if(!OpenStance.IsNone())Header->AddSlot().AutoWidth().Padding(0,0,20,0)
  [SNew(SButton).ButtonStyle(&MenuActionStyle).ContentPadding(FMargin(16,8)).OnClicked_Lambda([this](){return CloseTree();})[SNew(STextBlock).Text(FText::FromString(TEXT("ALL STANCES"))).Font(MenuFont(14)).ColorAndOpacity(FSlateColor::UseForeground())]];
 Header->AddSlot().FillWidth(1).VAlign(VAlign_Center)
  [SNew(STextBlock).Text(FText::FromString(CurrentPage==EMetaTreePage::Abilities?TEXT("Unlock your combo burst and improve its power, charge and your swap rhythm."):CurrentPage==EMetaTreePage::Stats?TEXT("Permanent strength for every stance. Speed and area adapt to each fighting style."):TEXT("Strengthen this stance with Soul Embers. Each rank is permanent."))).Font(MenuFont(17)).ColorAndOpacity(Muted).AutoWrapText(true)];
 return SNew(SVerticalBox)
  +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,12)[Header]
  +SVerticalBox::Slot().FillHeight(1)
  [SNew(SHorizontalBox)
   +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,24,0)[SkillMap.ToSharedRef()]
   +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(370)
    [SNew(SOverlay)
     +SOverlay::Slot()[SNew(BloodshiftMenu::SInsetSurface).Visibility(EVisibility::HitTestInvisible)]
     +SOverlay::Slot().Padding(25)
     [SNew(SVerticalBox)
      +SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox)+SScrollBox::Slot()[Inspector]]
      +SVerticalBox::Slot().AutoHeight().Padding(0,14,0,0)[LearnButton.ToSharedRef()]]]]]
  +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0,12,0,0)
  [SNew(STextBlock).Text(FText::FromString(TEXT("Scroll: zoom / Drag: pan / Arrows or D-pad: select / Enter: focus Learn"))).Font(MenuFont(13)).ColorAndOpacity(Muted)];
}

TSharedRef<SWidget> UMetaSkillTreeWidget::RebuildWidget()
{
 StanceArtwork.Reset();SynergyDefinitions.Reset();SynergyArtwork.Reset();
 const auto* PCClass=LoadClass<ASurvivorPlayerController>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController.BP_SurvivorPlayerController_C"));
 const auto* Upgrades=PCClass?PCClass->GetDefaultObject<ASurvivorPlayerController>()->GetPlayerUpgrades():nullptr;
 for(const auto& Stance:RunStances::All)StanceArtwork.Add(Artwork(Upgrades?Upgrades->FindUpgradeDefinition(Stance.Id):nullptr));
 if(auto* M=Progression())for(auto* Definition:M->GetSynergyUpgradeDefinitions())if(Definition){SynergyDefinitions.Add(Definition);SynergyArtwork.Add(Artwork(Definition));}
 auto Tabs=SNew(SHorizontalBox);
 for(EMetaTreePage Page:{EMetaTreePage::Stances,EMetaTreePage::Upgrades,EMetaTreePage::Abilities,EMetaTreePage::Stats})
 {
  const auto Tab=SNew(SButton).ButtonStyle(&MenuActionStyle).HAlign(HAlign_Center)
   .OnClicked_Lambda([this,Page](){SelectPage(Page);return FReply::Handled();})
   [SNew(SVerticalBox)
    +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(MenuFont(18,true)).Text(FText::FromString(PageName(Page))).ColorAndOpacity(FSlateColor::UseForeground())]
    +SVerticalBox::Slot().AutoHeight().Padding(0,5,0,0)
    [SNew(SBox).HeightOverride(2)[SNew(SBorder).Padding(0).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
     .BorderBackgroundColor_Lambda([this,Page](){return CurrentPage==Page?Ember:FLinearColor::Transparent;})]]];
  if(Page==EMetaTreePage::Stances)StancesTab=Tab;
  Tabs->AddSlot().FillWidth(1).Padding(0,0,12,0)[Tab];
 }
 SAssignNew(PageHost,SBox);RefreshPage();
 return SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
 [SNew(SBox).WidthOverride(1680).HeightOverride(1000)
  [SNew(SOverlay)
   +SOverlay::Slot()[SNew(BloodshiftMenu::SPanelSurface)]
   +SOverlay::Slot().Padding(36)
   [SNew(SVerticalBox)
    +SVerticalBox::Slot().AutoHeight()
    [SNew(SHorizontalBox)
     +SHorizontalBox::Slot().FillWidth(1)
     [SNew(SVerticalBox)
      +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(TEXT("TWIN SOUL"))).Font(MenuFont(34,true)).ColorAndOpacity(Paper)]
      +SVerticalBox::Slot().AutoHeight().Padding(0,7,0,0)[SNew(STextBlock).Text(FText::FromString(TEXT("Unlock new paths. Build a lasting legacy."))).Font(MenuFont(18)).ColorAndOpacity(Muted)]]
     +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
     [SNew(SVerticalBox)
      +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
      [SNew(STextBlock).Font(MenuFont(23,true)).ColorAndOpacity(Ember).Text_Lambda([this](){auto* M=Progression();return FText::FromString(FString::Printf(TEXT("%d SOUL EMBERS   /   %d SOUL SIGILS"),M?M->GetSoulEmbers():0,M?M->GetSoulSigils():0));})]
      +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0,7,0,0)
      [SNew(STextBlock).Text(FText::FromString(TEXT("Embers improve skills / Sigils unlock stances and synergies"))).Font(MenuFont(14)).ColorAndOpacity(Muted)]]]
    +SVerticalBox::Slot().AutoHeight().Padding(0,20,0,18)[SNew(BloodshiftMenu::SOrnamentDivider)]
    +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,18)[Tabs]
    +SVerticalBox::Slot().FillHeight(1)[PageHost.ToSharedRef()]
    +SVerticalBox::Slot().AutoHeight().Padding(0,10,0,0)
    [SNew(SBox).MinDesiredHeight(36)[SNew(STextBlock).Font(MenuFont(14)).ColorAndOpacity(Ember).WrapTextAt(1560).Text_Lambda([this](){return FText::FromString(Message.IsEmpty()?TEXT("Embers: enemies, chests, objectives and survival. Sigils: elites, chests, objectives and boss victories."):Message);})]]
    +SVerticalBox::Slot().AutoHeight().Padding(0,12,0,18)[SNew(BloodshiftMenu::SOrnamentDivider)]
    +SVerticalBox::Slot().AutoHeight()
    [SNew(SHorizontalBox)
     +SHorizontalBox::Slot().AutoWidth()
     [SNew(SButton).ButtonStyle(&MenuActionStyle).IsEnabled_Lambda([this](){return SkillMap.IsValid();})
      .OnClicked_Lambda([this](){if(SkillMap)SkillMap->ResetView();return FReply::Handled();})
      [SNew(STextBlock).Font(MenuFont(16)).Text(FText::FromString(TEXT("RESET VIEW"))).ColorAndOpacity(FSlateColor::UseForeground())]]
     +SHorizontalBox::Slot().FillWidth(1)[SNew(SBox)]
     +SHorizontalBox::Slot().AutoWidth().Padding(0,0,12,0)
     [SNew(SButton).ButtonStyle(&MenuActionStyle)
      .OnClicked_Lambda([this](){if(!bConfirmRefund){bConfirmRefund=true;Message=TEXT("Refund all skill ranks for their full Ember cost? Permanent stance and synergy unlocks are retained. Select CONFIRM REFUND to proceed.");}else{if(auto* M=Progression())Message=M->RefundSkills()?TEXT("Skill ranks refunded. All stance and synergy unlocks are retained."):TEXT("Could not save the refund. Your skills are unchanged.");bConfirmRefund=false;if(SkillMap)SkillMap->Refresh();}return FReply::Handled();})
      [SNew(STextBlock).Font(MenuFont(16)).ColorAndOpacity(FSlateColor::UseForeground()).Text_Lambda([this](){return FText::FromString(bConfirmRefund?TEXT("CONFIRM REFUND"):TEXT("REFUND SKILL RANKS"));})]]
     +SHorizontalBox::Slot().AutoWidth()
     [SNew(SButton).ButtonStyle(&MenuActionStyle).OnClicked_Lambda([this](){return CloseTree();})
      [SNew(STextBlock).Font(MenuFont(16)).Text(FText::FromString(TEXT("BACK"))).ColorAndOpacity(FSlateColor::UseForeground())]]]]]];
}
void UMetaSkillTreeWidget::ReleaseSlateResources(bool bReleaseChildren)
{
 Super::ReleaseSlateResources(bReleaseChildren);PageHost.Reset();SkillMap.Reset();StancesTab.Reset();
}
FReply UMetaSkillTreeWidget::CloseTree()
{
 if(!OpenStance.IsNone())
 {
  SelectPage(EMetaTreePage::Stances);
  return StancesTab.IsValid()?FReply::Handled().SetUserFocus(StancesTab.ToSharedRef(),EFocusCause::Navigation):FReply::Handled();
 }
 if(MenuOwner.IsValid())MenuOwner->ShowMainPanel();else RemoveFromParent();
 return FReply::Handled();
}
FReply UMetaSkillTreeWidget::NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
 if(Event.GetKey()==EKeys::Escape||Event.GetKey()==EKeys::Gamepad_FaceButton_Right)
 {
  if(bConfirmRefund){bConfirmRefund=false;Message.Empty();}else return CloseTree();
  return FReply::Handled();
 }
 return Super::NativeOnKeyDown(Geometry,Event);
}
