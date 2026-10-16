#include "SCollectionJournal.h"
#include "CollectionProgressSubsystem.h"
#include "SynergyMetaProgressionSubsystem.h"
#include "UpgradeDefinition.h"
#include "PlayerUpgradeRules.h"
#include "PlayerUpgradeComponent.h"
#include "SurvivorPlayerController.h"
#include "RunStanceCatalog.h"
#include "BloodshiftMenuStyle.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/Application/SlateApplication.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"

namespace
{
TSharedRef<STextBlock> JournalText(const FString& Text,int32 Size=16,FLinearColor Color=BloodshiftMenu::Paper)
{ return SNew(STextBlock).Text(FText::FromString(Text)).Font(BloodshiftMenu::BodyFont(Size)).ColorAndOpacity(Color).AutoWrapText(true); }
}
void SCollectionJournal::Construct(const FArguments& Args)
{
    using namespace BloodshiftMenu;
    ButtonStyle=ActionStyle();
    ButtonStyle.SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
    const FSlateBrush Hot=FSlateRoundedBoxBrush(FLinearColor(.035f,.09f,.10f),2.f,Gold,1.f);
    ButtonStyle.SetHovered(Hot).SetPressed(Hot).SetHoveredForeground(Paper).SetPressedForeground(Paper);
    ProgressStyle.SetBackgroundImage(FSlateColorBrush(Rule)).SetFillImage(FSlateColorBrush(FLinearColor::White));
    if(Args._GameInstance)
    {
        Progress=Args._GameInstance->GetSubsystem<UCollectionProgressSubsystem>();
        Meta=Args._GameInstance->GetSubsystem<USynergyMetaProgressionSubsystem>();
    }
    const auto* PCClass=LoadClass<ASurvivorPlayerController>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController.BP_SurvivorPlayerController_C"));
    const auto* Pool=PCClass?PCClass->GetDefaultObject<ASurvivorPlayerController>()->GetPlayerUpgrades():nullptr;
    if(Meta.IsValid()) for(auto C:{EUpgradeCategory::Samurai,EUpgradeCategory::Ninja,EUpgradeCategory::Synergy,EUpgradeCategory::Global,EUpgradeCategory::Cursed})
        for(auto* D:Meta->GetCollectionUpgradeDefinitions(C))
            if(!PlayerUpgradeRules::IsRetiredUpgrade(D->UpgradeId) && (!Pool || Pool->FindUpgradeDefinition(D->UpgradeId)==D)) Definitions.Emplace(D);
    for(const auto& D:Definitions)
    {
        FSlateBrush B; auto* Texture=D->Icon?D->Icon.Get():D->CardArtwork.Get();
        B.SetResourceObject(Texture); B.ImageSize=FVector2D(76,76); Artwork.Add(B);
    }
    auto Tabs=SNew(SHorizontalBox);
    const TCHAR* Names[]={TEXT("ALL"),TEXT("SAMURAI"),TEXT("NINJA"),TEXT("SYNERGIES"),TEXT("SHARED / PACTS"),TEXT("ACHIEVEMENTS")};
    for(int32 I=0;I<6;++I) Tabs->AddSlot().AutoWidth().Padding(0,0,8,0)
        [SNew(SButton).ButtonStyle(&ButtonStyle).ContentPadding(FMargin(16,10))
         .OnClicked_Lambda([this,I](){Category=I;Filter=0;Selected=INDEX_NONE;RebuildEntries();return FReply::Handled();})
         [SNew(STextBlock).Text(FText::FromString(Names[I])).Font(BodyFont(15)).ColorAndOpacity_Lambda([this,I](){return Category==I?Gold:Paper;})]];
    ChildSlot[SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight()[JournalText(TEXT("COLLECTION"),32)]
        +SVerticalBox::Slot().AutoHeight().Padding(0,7,0,16)[JournalText(TEXT("Discover your arsenal. Master its upgrades. Complete your achievements."),16,Muted)]
        +SVerticalBox::Slot().AutoHeight()[Tabs]
        +SVerticalBox::Slot().AutoHeight().Padding(0,16,0,14)
        [SNew(SHorizontalBox)
         +SHorizontalBox::Slot().AutoWidth()
         [SNew(SButton).ButtonStyle(&ButtonStyle).ContentPadding(FMargin(16,6))
          .OnClicked_Lambda([this](){Filter=(Filter+1)%(Category==5?3:4);Selected=INDEX_NONE;RebuildEntries();return FReply::Handled();})
          [SNew(STextBlock).Font(BodyFont(14)).ColorAndOpacity(FSlateColor::UseForeground()).Text_Lambda([this](){const TCHAR* Labels[]={TEXT("SHOW: ALL"),TEXT("SHOW: UNDISCOVERED"),TEXT("SHOW: DISCOVERED"),TEXT("SHOW: MASTERED")};return FText::FromString(Category==5?(Filter==0?TEXT("SHOW: ALL"):Filter==1?TEXT("SHOW: INCOMPLETE"):TEXT("SHOW: COMPLETED")):Labels[Filter]);})]]]
        +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,14)
        [SNew(STextBlock).Font(BodyFont(15)).ColorAndOpacity(Gold).Text_Lambda([this](){return FText::FromString(Summary);})]
        +SVerticalBox::Slot().FillHeight(1)
        [SNew(SHorizontalBox)
         +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,22,0)
         [SAssignNew(Scroll,SScrollBox).ScrollBarAlwaysVisible(true)+SScrollBox::Slot()[SAssignNew(Entries,SBox)]]
         +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(300)
          [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Surface).Padding(22)
           [SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(Details,SBox)]]]]]
        +SVerticalBox::Slot().AutoHeight().Padding(0,16,0,0)
        [SNew(SHorizontalBox)
         +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(0,0,16,0)
         [SNew(STextBlock).Font(BodyFont(13)).ColorAndOpacity(Muted).AutoWrapText(true)
          .Text_Lambda([this](){return FText::FromString(Progress.IsValid()?Progress->SteamStatus():TEXT("Collection unavailable."));})]
         +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).ButtonStyle(&ButtonStyle).ContentPadding(FMargin(12,8))
          .OnClicked_Lambda([this](){if(Progress.IsValid()){Progress->RefreshSteam();Progress->OpenSteamAchievements();}return FReply::Handled();})
          [JournalText(TEXT("STEAM ACHIEVEMENTS"),13)]]]];
    RebuildEntries();
}
void SCollectionJournal::Refresh() { if(Progress.IsValid())Progress->RefreshSteam();RebuildEntries(); }
void SCollectionJournal::FocusFirst() { if(!Tiles.IsEmpty())FSlateApplication::Get().SetKeyboardFocus(Tiles[0]); }
void SCollectionJournal::RebuildEntries()
{
    using namespace BloodshiftMenu;
    if(!Entries || !Details) return;
    auto Grid=SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(10,12));
    Tiles.Reset(); int32 Total=0,Found=0,Mastered=0; bool HasSelection=false;
    auto AddTile=[&](int32 Id,TSharedRef<SWidget> Content,float Width,float Height)
    {
        TSharedPtr<SButton> Button;
        Grid->AddSlot()[SNew(SBox).WidthOverride(Width).HeightOverride(Height)
            [SAssignNew(Button,SButton).ButtonStyle(&ButtonStyle).ContentPadding(10)
             .OnHovered_Lambda([this,Id](){Select(Id);})
             .OnClicked_Lambda([this,Id](){Select(Id);return FReply::Handled();})
             [Content]]];
        Tiles.Add(Button); if(Selected==Id)HasSelection=true;
        if(Selected==INDEX_NONE){Selected=Id;HasSelection=true;}
    };
    if(Category==5)
    {
        const auto& Achievements=CollectionJournal::Achievements();
        for(int32 I=0;I<Achievements.Num();++I)
        {
            const auto& A=Achievements[I]; const bool Done=Progress.IsValid()&&Progress->IsEarned(A.Id);
            ++Total;Found+=Done;
            if((Filter==1&&Done)||(Filter==2&&!Done))continue;
            const int32 Value=Progress.IsValid()?Progress->Progress(A):0;
            AddTile(I,SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight()[JournalText(A.Title,18,Done?Gold:Paper)]
                +SVerticalBox::Slot().FillHeight(1).Padding(0,7)[JournalText(A.Description,14,Muted)]
                +SVerticalBox::Slot().AutoHeight()[JournalText(Done?TEXT("COMPLETED"):FString::Printf(TEXT("%d / %d"),Value,A.Goal),13,Gold)]
                +SVerticalBox::Slot().AutoHeight().Padding(0,6,0,0)[SNew(SBox).HeightOverride(4)[SNew(SProgressBar).Style(&ProgressStyle).BarFillStyle(EProgressBarFillStyle::Scale).Percent(float(Value)/A.Goal).FillColorAndOpacity(Gold)]],292,174);
        }
        Summary=FString::Printf(TEXT("%d / %d ACHIEVEMENTS COMPLETED"),Found,Total);
    }
    else
    {
        for(int32 I=0;I<Definitions.Num();++I)
        {
            const auto* D=Definitions[I].Get();
            const bool Matches=Category==0 || (Category==1&&D->Category==EUpgradeCategory::Samurai)
                || (Category==2&&D->Category==EUpgradeCategory::Ninja) || (Category==3&&D->Category==EUpgradeCategory::Synergy)
                || (Category==4&&(D->Category==EUpgradeCategory::Global||D->Category==EUpgradeCategory::Cursed));
            if(!Matches)continue;
            const bool Seen=Progress.IsValid()&&Progress->BestRank(D->UpgradeId)>0;
            const bool Master=Progress.IsValid()&&Progress->IsMastered(D->UpgradeId);
            ++Total;Found+=Seen;Mastered+=Master;
            if((Filter==1&&Seen)||(Filter==2&&!Seen)||(Filter==3&&!Master))continue;
            AddTile(I,SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SNew(SBox).WidthOverride(70).HeightOverride(70)[SNew(SImage).Image(&Artwork[I]).ColorAndOpacity(Seen?FLinearColor::White:FLinearColor(.25f,.25f,.25f,1))]]
                +SVerticalBox::Slot().FillHeight(1).Padding(0,7,0,0)[JournalText(D->DisplayName.ToString(),13)]
                +SVerticalBox::Slot().AutoHeight()[JournalText(Master?TEXT("MASTERED"):Seen?TEXT("DISCOVERED"):TEXT("UNDISCOVERED"),10,Master?Gold:Muted)],138,174);
        }
        Summary=FString::Printf(TEXT("%d / %d DISCOVERED     /     %d MASTERED     /     %d SHOWN"),Found,Total,Mastered,Tiles.Num());
    }
    if(Tiles.IsEmpty()) { Entries->SetContent(JournalText(TEXT("No entries match these filters."),18,Muted));Selected=INDEX_NONE; }
    else Entries->SetContent(Grid);
    if(!HasSelection) Selected=INDEX_NONE;
    Select(Selected);
}
FString SCollectionJournal::Requirement(const UUpgradeDefinition* D) const
{
    if(!D) return FString();
    FString Hint;
    if(Meta.IsValid() && !Meta->IsCollectionUpgradeUnlocked(D))
        Hint=TEXT("Unlock in Skill Tree / ")+FString(D->Category==EUpgradeCategory::Synergy?TEXT("Synergies."):TEXT("Stances."));
    else if(RunStances::Find(D->UpgradeId)) Hint=TEXT("Equip this stance in New Run / Loadout, then begin a run.");
    else if(D->Category==EUpgradeCategory::Synergy) Hint=TEXT("Equip this synergy in your loadout or acquire it during a run.");
    else if(PlayerUpgradeRules::IsBloodShrineUpgrade(D)) Hint=TEXT("Claim this pact from a Blood Shrine during a run with a matching stance.");
    else Hint=D->Category==EUpgradeCategory::Global?TEXT("Choose this shared upgrade during a run."):TEXT("Choose this upgrade during a run with a matching stance.");
    TArray<FName> Prerequisites=D->PrerequisiteUpgradeIds;
    for(const auto& P:D->PrerequisiteRequirements) Prerequisites.AddUnique(P.UpgradeId);
    for(FName Id:Prerequisites)
    {
        FString Name=Id.ToString(); for(const auto& Entry:Definitions)if(Entry->UpgradeId==Id){Name=Entry->DisplayName.ToString();break;}
        int32 Rank=1;for(const auto& P:D->PrerequisiteRequirements)if(P.UpgradeId==Id)Rank=FMath::Max(1,P.MinimumLevel);
        Hint+=FString::Printf(TEXT("\nRequires %s (rank %d)."),*Name,Rank);
    }
    return Hint;
}
void SCollectionJournal::Select(int32 Index)
{
    using namespace BloodshiftMenu;
    Selected=Index;
    if(Index==INDEX_NONE){Details->SetContent(JournalText(TEXT("Select an entry to inspect it."),16,Muted));return;}
    if(Category==5)
    {
        const auto& A=CollectionJournal::Achievements()[Index]; const bool Done=Progress.IsValid()&&Progress->IsEarned(A.Id);
        Details->SetContent(SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight()[JournalText(TEXT("ACHIEVEMENT"),13,Gold)]
            +SVerticalBox::Slot().AutoHeight().Padding(0,14)[JournalText(A.Title,25)]
            +SVerticalBox::Slot().AutoHeight()[SNew(SOrnamentDivider)]
            +SVerticalBox::Slot().AutoHeight().Padding(0,18)[JournalText(A.Description,17)]
            +SVerticalBox::Slot().AutoHeight()[JournalText(FString::Printf(TEXT("PROGRESS  %d / %d"),Progress.IsValid()?Progress->Progress(A):0,A.Goal),15,Gold)]
            +SVerticalBox::Slot().AutoHeight().Padding(0,18)[JournalText(Done?(Progress->IsSteamConfirmed(A.Id)?TEXT("Completed / synced to Steam"):TEXT("Completed / Steam sync pending")):TEXT("Not completed yet."),14,Muted)]);
        return;
    }
    if(!Definitions.IsValidIndex(Index))return;
    const auto* D=Definitions[Index].Get(); const int32 Rank=Progress.IsValid()?Progress->BestRank(D->UpgradeId):0;
    Details->SetContent(SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SNew(SBox).WidthOverride(110).HeightOverride(110)[SNew(SImage).Image(&Artwork[Index])]]
        +SVerticalBox::Slot().AutoHeight().Padding(0,18)[JournalText(D->DisplayName.ToString(),24)]
        +SVerticalBox::Slot().AutoHeight()[SNew(SOrnamentDivider)]
        +SVerticalBox::Slot().AutoHeight().Padding(0,18)[JournalText(D->GetBalanceDescription().ToString(),16)]
        +SVerticalBox::Slot().AutoHeight()[JournalText(FString::Printf(TEXT("BEST RANK  %d / %d"),Rank,PlayerUpgradeRules::UpgradeRankLimit(D)),15,Gold)]
        +SVerticalBox::Slot().AutoHeight().Padding(0,14)[JournalText(Rank>0?TEXT("Discovered in a run. Reach the maximum rank in one run to master multi-rank upgrades."):TEXT("Not yet acquired in a run."),14,Muted)]
        +SVerticalBox::Slot().AutoHeight()[JournalText(Requirement(D),14,Muted)]);
}
FReply SCollectionJournal::OnKeyDown(const FGeometry& G,const FKeyEvent& E)
{
    const bool Forward=E.GetKey()==EKeys::Right||E.GetKey()==EKeys::Down||E.GetKey()==EKeys::Gamepad_DPad_Right||E.GetKey()==EKeys::Gamepad_DPad_Down;
    const bool Back=E.GetKey()==EKeys::Left||E.GetKey()==EKeys::Up||E.GetKey()==EKeys::Gamepad_DPad_Left||E.GetKey()==EKeys::Gamepad_DPad_Up;
    if(Forward||Back)for(int32 I=0;I<Tiles.Num();++I)if(Tiles[I]->HasKeyboardFocus())
    {
        const int32 Next=FMath::Clamp(I+(Forward?1:-1),0,Tiles.Num()-1);
        Scroll->ScrollDescendantIntoView(Tiles[Next],false);return FReply::Handled().SetUserFocus(Tiles[Next].ToSharedRef(),EFocusCause::Navigation);
    }
    return SCompoundWidget::OnKeyDown(G,E);
}
