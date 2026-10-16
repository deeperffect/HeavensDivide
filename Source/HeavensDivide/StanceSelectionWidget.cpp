#include "StanceSelectionWidget.h"
#include "ComboAbilityDefinition.h"
#include "BloodshiftMenuStyle.h"
#include "MainMenuWidget.h"
#include "PlayerUpgradeComponent.h"
#include "RunChallengeSubsystem.h"
#include "RunLoadoutSubsystem.h"
#include "RunStanceCatalog.h"
#include "RunTravelSubsystem.h"
#include "SurvivorPlayerController.h"
#include "SynergyMetaProgressionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "MenuLockVisual.h"
#include "MenuDropdownStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
struct FRunMapEntry { const TCHAR* Id; const TCHAR* Name; const TCHAR* Description; bool bAvailable; };
const FRunMapEntry RunMaps[] = {
    // Keep the saved progression ID stable when changing the playable environment.
    { TEXT("FallenGrove"), TEXT("OLD VILLAGE"), TEXT("Thatched homes. Restless spirits."), true },
    { TEXT("MoonlitForest"), TEXT("MOONLIT FOREST"), TEXT("Blue moon. Whispering pines."), true },
    { TEXT("HollowMarsh"), TEXT("SUNKEN SANCTUARY"), TEXT("Broken columns. Still waters."), true },
    { TEXT("CinderCitadel"), TEXT("EMBER QUARRY"), TEXT("Red cliffs. Forgotten workings."), true }
};

// Ink illustrations keep the destination cards in the same engraved style as the menus.
class SRunMapIllustration final : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SRunMapIllustration) : _MapIndex(0) {}
        SLATE_ARGUMENT(int32, MapIndex)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args) { MapIndex = Args._MapIndex; }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(180, 65); }
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&,
        FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool) const override
    {
        using namespace BloodshiftMenu;
        const auto Size = G.GetLocalSize();
        const FOrnamentPainter P{Out, G, Layer, Style.GetColorAndOpacityTint().A};
        const auto Accent = Gold;
        auto At = [&](float X, float Y) { return FVector2D(X * Size.X, Y * Size.Y); };
        P.Arc(At(.64f,.32f), Size.Y * .21f, 0, 2 * PI, Accent.CopyWithNewOpacity(.5f));
        P.Line({At(.05f,.9f), At(.23f,.66f), At(.41f,.8f), At(.58f,.55f), At(.81f,.76f), At(.97f,.66f)}, Rule, 1.5f);
        if (MapIndex == 0)
        {
            // Thatched homes and a village lane beneath the moon.
            for (float X : {.27f, .73f})
            {
                P.Line({At(X-.18f,.57f), At(X,.25f), At(X+.18f,.57f), At(X-.18f,.57f)}, Accent, 1.5f);
                P.Line({At(X-.13f,.58f), At(X-.13f,.84f), At(X+.13f,.84f), At(X+.13f,.58f)}, Accent);
                P.Line({At(X-.035f,.84f), At(X-.035f,.66f), At(X+.035f,.66f), At(X+.035f,.84f)}, Accent);
            }
            P.Line({At(.40f,.98f), At(.52f,.80f), At(.49f,.62f)}, Gold.CopyWithNewOpacity(.75f), 2);
        }
        else if (MapIndex == 2)
        {
            for (float Level : {.38f, .59f, .8f})
            {
                P.Line({At(.24f,Level), At(.5f,Level-.19f), At(.76f,Level)}, Accent, 1.5f);
                P.Line({At(.33f,Level+.02f), At(.33f,Level+.13f), At(.67f,Level+.13f), At(.67f,Level+.02f)}, Accent);
            }
        }
        else if (MapIndex == 1)
        {
            for (float X : {.16f,.34f,.78f,.90f})
            {
                const float Top = X < .5f ? .23f : .37f;
                P.Line({At(X,.87f), At(X,Top)}, Accent, 1.5f);
                P.Line({At(X-.065f,Top+.23f), At(X,Top+.08f), At(X+.06f,Top+.23f)}, Accent);
                P.Line({At(X-.085f,Top+.39f), At(X,Top+.18f), At(X+.085f,Top+.39f)}, Accent);
            }
            for(float Y : {.76f,.86f,.96f}) P.Line({At(.15f,Y),At(.82f,Y-.015f)}, Muted.CopyWithNewOpacity(.35f));
        }
        else
        {
            P.Line({At(.08f,.85f),At(.2f,.35f),At(.35f,.49f),At(.4f,.8f)},Accent,2);
            P.Line({At(.6f,.8f),At(.68f,.28f),At(.85f,.4f),At(.95f,.87f)},Accent,2);
            for(float Y : {.62f,.77f,.92f}) P.Line({At(.40f,Y),At(.60f,Y)},Accent);
            P.Line({At(.42f,.52f),At(.42f,.98f)},Accent);
            P.Line({At(.58f,.52f),At(.58f,.98f)},Accent);
        }
        return Layer;
    }
private:
    int32 MapIndex = 0;
};
}

UStanceSelectionWidget::UStanceSelectionWidget(const FObjectInitializer& Initializer) : Super(Initializer)
{
    using namespace BloodshiftMenu;
    SetIsFocusable(true);
    HoverScale = 1.015f;
    PressScale = 1.025f;
    static ConstructorHelpers::FObjectFinder<UObject> Heading(TEXT("/Game/Assets/Fonts/Cinzel-Medium_Font.Cinzel-Medium_Font"));
    HeadingFont = FSlateFontInfo(Heading.Object, 24, TEXT("Default"));
    ActionStyle = BloodshiftMenu::ActionStyle();
    CardStyle = ActionStyle;
    CardStyle.SetNormal(Solid(BloodshiftMenu::Surface)).SetHovered(Solid(FLinearColor(.035f,.065f,.070f)))
        .SetPressed(Solid(FLinearColor(.07f,.08f,.065f)))
        .SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
    RankStyle = ActionStyle;
    RankStyle.SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
    SelectedBrush = Solid(BloodshiftMenu::Gold);
    EmptyBrush = Solid(Rule);
}

FSlateFontInfo UStanceSelectionWidget::Font(int32 Size, bool bHeading) const
{
    using namespace BloodshiftMenu;
    auto Result = bHeading ? HeadingFont : BodyFont(Size);
    Result.Size = Size;
    return Result;
}

void UStanceSelectionWidget::Open()
{
    bStarting = false;
    if (auto* Challenges = GetGameInstance() ? GetGameInstance()->GetSubsystem<URunChallengeSubsystem>() : nullptr)
        SelectedMap = Challenges->GetMapId();
    SetupPage = ERunSetupPage::ArenaAndStances;
    if (auto* Loadout = GetGameInstance() ? GetGameInstance()->GetSubsystem<URunLoadoutSubsystem>() : nullptr)
    {
        Loadout->SanitizeSelections();
        SelectedSamurai = Loadout->GetSamuraiStance();
        SelectedNinja = Loadout->GetNinjaStance();
    }
    // The menu keeps this page alive. Rebuild its content after purchases or a profile reset.
    if (PageContent) PageContent->SetContent(BuildPage());
    else TakeWidget();
    FocusButton(0);
}

void UStanceSelectionWidget::SelectSetupPage(ERunSetupPage Page)
{
    if (bStarting || SetupPage == Page) return;
    SetupPage = Page;
    if (PageContent) PageContent->SetContent(BuildPage());
    FocusButton(0);
}

bool UStanceSelectionWidget::IsSynergySelected(FName Id) const
{
    const auto* Loadout = GetGameInstance() ? GetGameInstance()->GetSubsystem<URunLoadoutSubsystem>() : nullptr;
    return Loadout && Loadout->GetSelectedSynergies().Contains(Id);
}

bool UStanceSelectionWidget::SetSynergySelected(FName Id, bool bSelected)
{
    auto* Loadout = GetGameInstance() ? GetGameInstance()->GetSubsystem<URunLoadoutSubsystem>() : nullptr;
    return !bStarting && Loadout && SynergyDefinitions.ContainsByPredicate([Id](const auto& D) { return D && D->UpgradeId == Id; }) &&
        Loadout->SetSynergySelected(Id,bSelected);
}

bool UStanceSelectionWidget::SelectComboAbility(FName Id)
{
    auto* Loadout = GetGameInstance() ? GetGameInstance()->GetSubsystem<URunLoadoutSubsystem>() : nullptr;
    return !bStarting && Loadout && Loadout->SetSelectedComboAbility(Id);
}

bool UStanceSelectionWidget::IsStanceAvailable(FName Id) const
{
    const auto* Stance = RunStances::Find(Id);
    if (!Stance) return false;
    const auto* Meta = GetGameInstance() ? GetGameInstance()->GetSubsystem<USynergyMetaProgressionSubsystem>() : nullptr;
    return (Meta ? Meta->IsStanceUnlocked(Id) : RunStances::IsDefaultUnlocked(Id)) &&
        Definitions.ContainsByPredicate([Id](const auto& D) { return D && D->UpgradeId == Id; });
}

int32 UStanceSelectionWidget::GetVisibleStanceCount() const
{
    return UE_ARRAY_COUNT(RunStances::All);
}

bool UStanceSelectionWidget::SelectStance(FName Id)
{
    const auto* Stance = RunStances::Find(Id);
    if (!Stance || bStarting || !IsStanceAvailable(Id)) return false;
    (Stance->bSamurai ? SelectedSamurai : SelectedNinja) = Id;
    return true;
}

bool UStanceSelectionWidget::SelectMap(FName Id)
{
    if (bStarting) return false;
    for (const auto& Map : RunMaps)
        if (Id == Map.Id && Map.bAvailable) { if(auto* C=GetGameInstance()?GetGameInstance()->GetSubsystem<URunChallengeSubsystem>():nullptr)C->SetMap(Id);SelectedMap = Id; return true; }
    return false;
}

bool UStanceSelectionWidget::CanStartRun() const
{
    const auto* Steel = RunStances::Find(SelectedSamurai);
    const auto* Shadow = RunStances::Find(SelectedNinja);
    bool bMapAvailable = false;
    for (const auto& Map : RunMaps) if (SelectedMap == Map.Id && Map.bAvailable) bMapAvailable = true;
    return !bStarting && bMapAvailable && Steel && Steel->bSamurai && Shadow && !Shadow->bSamurai &&
        IsStanceAvailable(SelectedSamurai) && IsStanceAvailable(SelectedNinja);
}

bool UStanceSelectionWidget::IsSelected(int32 Index) const
{
    const auto& Stance = RunStances::All[Index];
    return (Stance.bSamurai ? SelectedSamurai : SelectedNinja) == Stance.Id;
}

void UStanceSelectionWidget::AddNavigation(const TSharedRef<SWidget>& Button, FVector2D Position)
{
    Buttons.Add(Button);
    NavigationPositions.Add(Position);
}

TSharedRef<SWidget> UStanceSelectionWidget::MakeMapCard(int32 Index)
{
    using namespace BloodshiftMenu;
    const auto& Map = RunMaps[Index];
    auto Button = SNew(SButton).ButtonStyle(&CardStyle).ContentPadding(0).IsEnabled(Map.bAvailable)
        .ToolTipText(FText::FromString(Map.bAvailable ? TEXT("The first arena. Choose your challenges below.") : TEXT("A future destination. Not available yet.")))
        .OnClicked_Lambda([this,Index]() { SelectMap(RunMaps[Index].Id); return FReply::Handled(); })
        [SNew(SBorder).Padding(1)
         .BorderImage_Lambda([this,Index]() { return SelectedMap == RunMaps[Index].Id ? &SelectedBrush : &EmptyBrush; })
         [SNew(SBorder).Padding(10).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Ink)
          [SNew(SVerticalBox)
           +SVerticalBox::Slot().AutoHeight()[SNew(SRunMapIllustration).MapIndex(Index)]
           +SVerticalBox::Slot().AutoHeight().Padding(0,3,0,0)
           [SNew(SBox).HeightOverride(24)
            [SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
             [SNew(STextBlock).Text(FText::FromString(Map.Name)).Font(Font(14,true)).ColorAndOpacity(Map.bAvailable ? Paper : Muted).Justification(ETextJustify::Center)]]]
           +SVerticalBox::Slot().AutoHeight().Padding(0,3,0,0)
           [SNew(SBox).HeightOverride(18)
            [SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
             [SNew(STextBlock).Text(FText::FromString(Map.Description)).Font(Font(11)).ColorAndOpacity(Muted).Justification(ETextJustify::Center)]]]
           +SVerticalBox::Slot().AutoHeight().Padding(0,6,0,0)
           [SNew(STextBlock).Text_Lambda([this,Index]() { return FText::FromString(SelectedMap == RunMaps[Index].Id ? TEXT("SELECTED") : TEXT("SELECT")); })
            .Font(Font(11)).ColorAndOpacity(Map.bAvailable ? BloodshiftMenu::Gold : Muted).Justification(ETextJustify::Center)]]]];
    if (Map.bAvailable) AddNavigation(Button,{float(Index%2)*2,float(Index/2)*2});
    return SNew(SBox).WidthOverride(206).HeightOverride(157)[Button];
}

TSharedRef<SWidget> UStanceSelectionWidget::MakeModifierRow(int32 Index)
{
    using namespace BloodshiftMenu;
    const auto& Modifier = RunChallenges::Modifiers()[Index];
    const FName Id = Modifier.Id;
    const FString InlineDescription = Id == TEXT("Count")
        ? FString::Printf(TEXT("Enemy population +%d%% per rank (cap 200)."),FMath::RoundToInt(100.f*Modifier.PerRank))
        : Modifier.Description;
    auto* Challenge = GetGameInstance() ? GetGameInstance()->GetSubsystem<URunChallengeSubsystem>() : nullptr;
    auto RankButtons = SNew(SHorizontalBox);
    for (int32 Rank=0; Rank<=5; ++Rank)
    {
        auto Button = SNew(SButton).ButtonStyle(&RankStyle).ContentPadding(0)
            .IsEnabled_Lambda([this,Challenge,Id,Rank]() { return !bStarting && Challenge && Challenge->CanEditModifiers() && Rank<=Challenge->GetUnlockedRank(Id); })
            .ToolTipText(FText::FromString(Challenge && Rank>Challenge->GetUnlockedRank(Id) ? FString::Printf(TEXT("Win with rank %d to unlock."),Rank-1) : Rank == 0 ? TEXT("Standard difficulty") : FString::Printf(TEXT("Rank %d: +%d%%"),Rank,FMath::RoundToInt(100.f*Rank*Modifier.PerRank))))
            .OnClicked_Lambda([Challenge,Id,Rank]() { if(Challenge) Challenge->SetModifierRank(Id,Rank); return FReply::Handled(); })
            [SNew(SBorder).Padding(1)
             .BorderImage_Lambda([this,Challenge,Id,Rank]() { return (Challenge ? Challenge->GetModifierRank(Id) : 0) == Rank ? &SelectedBrush : &EmptyBrush; })
             [SNew(SBorder).Padding(3).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Ink)
              [SNew(SOverlay)
              +SOverlay::Slot()[SNew(STextBlock).Text(FText::AsNumber(Rank)).Font(Font(15)).Justification(ETextJustify::Center)
               .ColorAndOpacity_Lambda([Challenge,Id,Rank]() { return (Challenge ? Challenge->GetModifierRank(Id) : 0) == Rank ? BloodshiftMenu::Gold : Paper; })]
              +SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom)[SNew(SBox).WidthOverride(14).HeightOverride(14)[SNew(SMenuLockVisual).Chains(false).Visibility_Lambda([Challenge,Id,Rank](){return !Challenge || Rank>Challenge->GetUnlockedRank(Id)?EVisibility::HitTestInvisible:EVisibility::Collapsed;})]]]]];
        AddNavigation(Button,{float(Rank),float(Index+2)});
        RankButtons->AddSlot().AutoWidth().Padding(Rank ? 5 : 0,0,0,0)[SNew(SBox).WidthOverride(30).HeightOverride(30)[Button]];
    }
    return SNew(SBox).HeightOverride(49)
        [SNew(SHorizontalBox)
         +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
         [SNew(SVerticalBox)
          +SVerticalBox::Slot().AutoHeight()
          [SNew(STextBlock).Text(FText::FromString(Modifier.Name)).Font(Font(16)).ColorAndOpacity(Paper)]
          +SVerticalBox::Slot().AutoHeight().Padding(0,2,0,0)
          [SNew(STextBlock).Text(FText::FromString(InlineDescription)).ToolTipText(FText::FromString(Modifier.Description))
           .Font(Font(11)).ColorAndOpacity(Muted).WrapTextAt(335)]]
         +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[RankButtons]
         +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8,0,0,0)
         [SNew(SBox).WidthOverride(65)
          [SNew(STextBlock).Font(Font(16)).ColorAndOpacity(BloodshiftMenu::Gold).Justification(ETextJustify::Right)
           .Text_Lambda([Challenge,Id,PerRank=Modifier.PerRank]() { return FText::FromString(FString::Printf(TEXT("+%d%%"),FMath::RoundToInt(100.f*PerRank*(Challenge ? Challenge->GetModifierRank(Id) : 0)))); })]]];
}

TSharedRef<SWidget> UStanceSelectionWidget::RebuildWidget()
{
    return SAssignNew(PageContent,SBox)[BuildPage()];
}

TSharedRef<SWidget> UStanceSelectionWidget::MakeSetupTab(ERunSetupPage Page)
{
    using namespace BloodshiftMenu;
    auto Button = SNew(SButton).ButtonStyle(&RankStyle).ContentPadding(FMargin(16,12))
        .OnClicked_Lambda([this,Page]() {
            SelectSetupPage(Page);
            return FReply::Handled().SetUserFocus(Buttons[Page == ERunSetupPage::ArenaAndStances ? 0 : 1].ToSharedRef(),EFocusCause::Navigation);
        })
        [SNew(STextBlock).Font(Font(15)).ColorAndOpacity(SetupPage == Page ? BloodshiftMenu::Gold : Muted)
         .Text(FText::FromString(Page == ERunSetupPage::ArenaAndStances ? TEXT("ARENA") : TEXT("LOADOUT")))];
    AddNavigation(Button,{Page == ERunSetupPage::ArenaAndStances ? 1.f : 7.f,-2});
    return SNew(SBorder).Padding(1).BorderImage(SetupPage == Page ? &SelectedBrush : &EmptyBrush)[Button];
}

TSharedRef<SWidget> UStanceSelectionWidget::BuildArenaPage()
{
    using namespace BloodshiftMenu;
    auto Maps=SNew(SUniformGridPanel).SlotPadding(FMargin(0,0,11,11));
    for(int32 I=0;I<UE_ARRAY_COUNT(RunMaps);++I)
        Maps->AddSlot(I%2,I/2)[MakeMapCard(I)];
    auto Modifiers=SNew(SVerticalBox);
    for(int32 I=0;I<RunChallenges::Modifiers().Num();++I)
        Modifiers->AddSlot().AutoHeight()[MakeModifierRow(I)];
    return SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,40,0)
        [SNew(SVerticalBox)
         +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)[SNew(STextBlock).Text(FText::FromString(TEXT("DESTINATION"))).Font(Font(21,true)).ColorAndOpacity(Paper)]
         +SVerticalBox::Slot().AutoHeight()[Maps]]
        +SHorizontalBox::Slot().FillWidth(1)
        [SNew(SVerticalBox)
         +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)[SNew(STextBlock).Text(FText::FromString(TEXT("CHALLENGES"))).Font(Font(21,true)).ColorAndOpacity(Paper)]
         +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,16)[SNew(STextBlock).Text(FText::FromString(TEXT("Win to unlock the next rank."))).Font(Font(15)).ColorAndOpacity(Muted)]
         +SVerticalBox::Slot().AutoHeight()[Modifiers]
         +SVerticalBox::Slot().AutoHeight().Padding(0,16,0,0)
         [SNew(STextBlock).Font(Font(17,true)).ColorAndOpacity(BloodshiftMenu::Gold)
          .Text_Lambda([this](){const auto* C=GetGameInstance()?GetGameInstance()->GetSubsystem<URunChallengeSubsystem>():nullptr;
              return FText::FromString(FString::Printf(TEXT("CURRENCY REWARDS   +%d%%"),FMath::RoundToInt(100.f*((C?C->GetRewardMultiplier():1.f)-1.f))));})]];
}

TSharedRef<SWidget> UStanceSelectionWidget::MakeSynergyCard(int32 Index)
{
    using namespace BloodshiftMenu;
    auto* Definition = SynergyDefinitions[Index].Get();
    const FName Id = Definition->UpgradeId;
    auto* Loadout = GetGameInstance() ? GetGameInstance()->GetSubsystem<URunLoadoutSubsystem>() : nullptr;
    auto Button = SNew(SButton).ButtonStyle(&CardStyle).ContentPadding(0)
        .IsEnabled_Lambda([this,Loadout,Id]() { return !bStarting && Loadout &&
            (IsSynergySelected(Id) || Loadout->GetSelectedSynergies().Num() < URunLoadoutSubsystem::MaxSelectedSynergies); })
        .ToolTipText(Definition->GetBalanceDescription())
        .OnClicked_Lambda([this,Id]() { SetSynergySelected(Id,!IsSynergySelected(Id)); return FReply::Handled(); })
        [SNew(SBorder).Padding(1).BorderImage_Lambda([this,Id]() { return IsSynergySelected(Id) ? &SelectedBrush : &EmptyBrush; })
         [SNew(SBorder).Padding(14).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Ink)
          [SNew(SHorizontalBox)
           +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0,4,12,0)
           [SNew(SBox).WidthOverride(60).HeightOverride(84)
            [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SImage).Image(&SynergyArtwork[Index])]]]
           +SHorizontalBox::Slot().FillWidth(1)
           [SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight()
            [SNew(STextBlock).Text(Definition->DisplayName).Font(Font(17,true)).ColorAndOpacity(Paper).WrapTextAt(305)]
            +SVerticalBox::Slot().AutoHeight().Padding(0,7,0,0)
            [SNew(STextBlock).Text(Definition->GetBalanceDescription()).Font(Font(13)).ColorAndOpacity(Muted).WrapTextAt(305)]
            +SVerticalBox::Slot().AutoHeight().Padding(0,10,0,0)
            [SNew(STextBlock).Font(Font(11))
             .ColorAndOpacity_Lambda([this,Id]() { return IsSynergySelected(Id) ? BloodshiftMenu::Gold : Muted; })
             .Text_Lambda([this,Id]() { return FText::FromString(IsSynergySelected(Id) ? TEXT("ACTIVE AT RUN START  /  REMOVE") : TEXT("ADD TO RUN")); })]]]]];
    ScrollNavigationIndices.Add(Buttons.Num());
    AddNavigation(Button,{float(1+(Index%2)*3),float(1+(Index/2)*2)});
    return SNew(SBox).MinDesiredHeight(146)[Button];
}

TSharedRef<SWidget> UStanceSelectionWidget::BuildSynergyPage()
{
    using namespace BloodshiftMenu;
    auto* Loadout = GetGameInstance() ? GetGameInstance()->GetSubsystem<URunLoadoutSubsystem>() : nullptr;
    auto Synergies = SNew(SVerticalBox);
    for (int32 I=0; I<SynergyDefinitions.Num(); I+=2)
    {
        auto Row = SNew(SHorizontalBox);
        for (int32 Column=0; Column<2; ++Column)
            Row->AddSlot().FillWidth(1).Padding(Column ? 10 : 0,0,0,0)
                [I+Column < SynergyDefinitions.Num() ? MakeSynergyCard(I+Column) : SNullWidget::NullWidget];
        Synergies->AddSlot().AutoHeight().Padding(0,0,0,10)[Row];
    }
    return SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,32,0)
        [SNew(SVerticalBox)
         +SVerticalBox::Slot().AutoHeight()
         [SNew(SHorizontalBox)
          +SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Text(FText::FromString(TEXT("STARTING SYNERGIES"))).Font(Font(21,true)).ColorAndOpacity(Paper)]
          +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(STextBlock).Font(Font(17)).ColorAndOpacity(BloodshiftMenu::Gold)
           .Text_Lambda([Loadout]() { return FText::FromString(FString::Printf(TEXT("%d / %d ACTIVE"),Loadout ? Loadout->GetSelectedSynergies().Num() : 0,URunLoadoutSubsystem::MaxSelectedSynergies)); })]]
         +SVerticalBox::Slot().AutoHeight().Padding(0,8,0,12)
         [SNew(STextBlock).Text(FText::FromString(TEXT("Equip up to five synergies."))).Font(Font(14)).ColorAndOpacity(Muted).WrapTextAt(825)]
         +SVerticalBox::Slot().FillHeight(1)
         [SAssignNew(SynergyScroll,SScrollBox).ScrollBarAlwaysVisible(true)+SScrollBox::Slot()[Synergies]]
         +SVerticalBox::Slot().AutoHeight().Padding(0,10,0,0)
         [SNew(STextBlock).Text(FText::FromString(TEXT("Unlock more in the Skill Tree."))).Font(Font(13)).ColorAndOpacity(Muted).WrapTextAt(825)]]
        +SHorizontalBox::Slot().AutoWidth()
        [SNew(SBox).WidthOverride(460)
         [SNew(SVerticalBox)
          +SVerticalBox::Slot().AutoHeight()[MakeLoadoutPicker(0)]
          +SVerticalBox::Slot().AutoHeight().Padding(0,16,0,0)[MakeLoadoutPicker(1)]
          +SVerticalBox::Slot().AutoHeight().Padding(0,14,0,0)[MakeLoadoutPicker(2)]]];
}

TSharedRef<SWidget> UStanceSelectionWidget::MakeLoadoutPicker(int32 Kind)
{
    using namespace BloodshiftMenu;
    auto Label=[this,Kind]()
    {
        if(Kind<2){const auto* Stance=RunStances::Find(Kind==0?SelectedSamurai:SelectedNinja);return FText::FromString(Stance?Stance->Name:TEXT("Choose stance"));}
        const auto* L=GetGameInstance()?GetGameInstance()->GetSubsystem<URunLoadoutSubsystem>():nullptr;
        const FName Id = L ? L->GetSelectedComboAbility() : NAME_None;
        if (const auto* Entry = ComboCatalog::Find(Id)) return FText::FromString(Entry->Name);
        return FText::FromString(Id.IsNone()?TEXT("No combo"):TEXT("No combo"));
    };
    SAssignNew(LoadoutPickers[Kind],SComboButton).ComboButtonStyle(&MenuDropdown::ButtonStyle()).ContentPadding(FMargin(16,14)).HasDownArrow(false)
        .OnGetMenuContent_Lambda([this,Kind](){return MakeLoadoutChoices(Kind);})
        .ButtonContent()[SNew(SHorizontalBox)
         +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(STextBlock).Text_Lambda(Label).Font(Font(20,true)).ColorAndOpacity(FSlateColor::UseForeground())]
         +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(16,0,0,0)[SNew(SMenuDropdownArrow)]];
    const TSharedRef<SWidget> PickerWidget=LoadoutPickers[Kind].ToSharedRef();
    AddNavigation(PickerWidget->GetChildren()->GetChildAt(0),{8,float(1+Kind*2)});
    return SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)
         [SNew(STextBlock).Text(FText::FromString(Kind==0?TEXT("SAMURAI STANCE"):Kind==1?TEXT("NINJA STANCE"):TEXT("COMBINED ABILITY"))).Font(Font(19,true)).ColorAndOpacity(Paper)]
        +SVerticalBox::Slot().AutoHeight()[LoadoutPickers[Kind].ToSharedRef()];
}

TSharedRef<SWidget> UStanceSelectionWidget::MakeLoadoutChoices(int32 Kind)
{
    using namespace BloodshiftMenu;
    auto Choices=SNew(SVerticalBox);
    TSharedPtr<SButton> FirstAvailable;
    auto AddChoice=[this,Kind,Choices,&FirstAvailable](FName Id,const FString& Name,bool bAvailable,int32 ArtIndex)
    {
        auto Row=SNew(SHorizontalBox);
        if(ArtIndex>=0)Row->AddSlot().AutoWidth().Padding(0,0,14,0)
            [SNew(SBox).WidthOverride(64).HeightOverride(64)
             [SNew(SOverlay)+SOverlay::Slot()[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SImage).Image(&Artwork[ArtIndex])]]
              +SOverlay::Slot()[SNew(SMenuLockVisual).Visibility(bAvailable?EVisibility::Collapsed:EVisibility::HitTestInvisible)]]];
        Row->AddSlot().FillWidth(1).VAlign(VAlign_Center)
            [SNew(STextBlock).Text(FText::FromString(Name)).Font(Font(18,true)).ColorAndOpacity(Paper)];
        if(!bAvailable)Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(12,0)
            [SNew(SBox).WidthOverride(28).HeightOverride(28)[SNew(SMenuLockVisual).Chains(false)]];
        TSharedPtr<SButton> Choice;
        Choices->AddSlot().AutoHeight()[SAssignNew(Choice,SButton).ButtonStyle(&CardStyle).ContentPadding(12).IsEnabled(bAvailable)
            .ToolTipText(FText::FromString(ComboCatalog::Find(Id)?FString::Printf(TEXT("%s\nFits: %s"),ComboCatalog::Find(Id)->Description,ComboCatalog::Find(Id)->Stance):bAvailable?TEXT(""):TEXT("Unlock in the Skill Tree.")))
            .OnClicked_Lambda([this,Kind,Id](){if(Kind<2)SelectStance(Id);else if(auto* L=GetGameInstance()?GetGameInstance()->GetSubsystem<URunLoadoutSubsystem>():nullptr)L->SetSelectedComboAbility(Id);LoadoutPickers[Kind]->SetIsOpen(false);return FReply::Handled();})[Row]];
        if(bAvailable && !FirstAvailable)FirstAvailable=Choice;
    };
    if(Kind<2)for(int32 I=0;I<UE_ARRAY_COUNT(RunStances::All);++I)
    {
        const auto& Stance=RunStances::All[I];
        if(Stance.bSamurai==(Kind==0))AddChoice(Stance.Id,Stance.Name,IsStanceAvailable(Stance.Id),I);
    }
    else
    {
        const auto* L=GetGameInstance()?GetGameInstance()->GetSubsystem<URunLoadoutSubsystem>():nullptr;
        AddChoice(NAME_None,TEXT("No combo"),true,-1);
        for (const auto& Entry : ComboCatalog::Entries())
            AddChoice(Entry.Id,Entry.Name,L&&L->IsComboAbilityUnlocked(Entry.Id),-1);
    }
    LoadoutPickers[Kind]->SetMenuContentWidgetToFocus(FirstAvailable);
    return SNew(SBorder).BorderImage(&MenuDropdown::ButtonStyle().MenuBorderBrush).Padding(2)
        [SNew(SBox).WidthOverride(456).MaxDesiredHeight(480)[SNew(SScrollBox).ScrollBarAlwaysVisible(true)+SScrollBox::Slot()[Choices]]];
}

TSharedRef<SWidget> UStanceSelectionWidget::BuildPage()
{
    using namespace BloodshiftMenu;
    Definitions.Reset(); Artwork.Reset(); SynergyDefinitions.Reset(); SynergyArtwork.Reset();
    Buttons.Reset(); NavigationPositions.Reset(); ScrollNavigationIndices.Reset(); SynergyScroll.Reset();
    for(auto& Picker:LoadoutPickers)Picker.Reset();
    const auto* PCClass = LoadClass<ASurvivorPlayerController>(nullptr, TEXT("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController.BP_SurvivorPlayerController_C"));
    const auto* Upgrades = PCClass ? PCClass->GetDefaultObject<ASurvivorPlayerController>()->GetPlayerUpgrades() : nullptr;
    auto MakeArtwork = [](UUpgradeDefinition* Definition)
    {
        auto* Texture = Definition ? (Definition->CardArtwork ? Definition->CardArtwork.Get() : Definition->Icon.Get()) : nullptr;
        FSlateBrush Brush; Brush.ImageSize = Texture ? FVector2D(Texture->GetSizeX(),Texture->GetSizeY()) : FVector2D(256,256);
        Brush.SetResourceObject(Texture); return Brush;
    };
    for (const auto& Stance : RunStances::All)
    {
        auto* Definition = Upgrades ? Upgrades->FindUpgradeDefinition(Stance.Id) : nullptr;
        Definitions.Add(Definition); Artwork.Add(MakeArtwork(Definition));
    }
    if(auto* Loadout = GetGameInstance() ? GetGameInstance()->GetSubsystem<URunLoadoutSubsystem>() : nullptr)
    {
        Loadout->SanitizeSelections();
        for (auto* Definition : Loadout->GetAvailableSynergies())
        { SynergyDefinitions.Add(Definition); SynergyArtwork.Add(MakeArtwork(Definition)); }
    }
    if(!IsStanceAvailable(SelectedSamurai)) SelectedSamurai=TEXT("BattleStance");
    if(!IsStanceAvailable(SelectedNinja)) SelectedNinja=TEXT("BarrageStance");
    auto Content = SetupPage == ERunSetupPage::ArenaAndStances ? BuildArenaPage() : BuildSynergyPage();
    auto Back = SNew(SButton).ButtonStyle(&ActionStyle)
        .OnClicked_Lambda([this]() { if (SetupPage == ERunSetupPage::SynergiesAndCombo) SelectSetupPage(ERunSetupPage::ArenaAndStances); else if (MenuOwner) MenuOwner->ShowMainPanel(); return FReply::Handled(); })
        [SNew(STextBlock).Text(FText::FromString(TEXT("BACK"))).Font(Font(17)).ColorAndOpacity(FSlateColor::UseForeground())];
    auto Start = SNew(SButton).ButtonStyle(&ActionStyle)
        .IsEnabled_Lambda([this]() { return CanStartRun(); })
        .OnClicked_UObject(this, &UStanceSelectionWidget::ConfirmRun)
        [SNew(STextBlock).Text(FText::FromString(SetupPage == ERunSetupPage::ArenaAndStances ? TEXT("NEXT") : TEXT("BEGIN RUN"))).Font(Font(20,true)).ColorAndOpacity(FSlateColor::UseForeground())];
    const float FooterNavigationY = SetupPage == ERunSetupPage::SynergiesAndCombo
        ? FMath::Max(8.f,2.f + 2.f*FMath::DivideAndRoundUp(SynergyDefinitions.Num(),2)) : 8.f;
    AddNavigation(Back,{0,FooterNavigationY}); AddNavigation(Start,{9,FooterNavigationY});
    return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("NoBrush")).Padding(20)
        [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
         [SNew(SBox).WidthOverride(1440).HeightOverride(850)
          [SNew(SOverlay)
           +SOverlay::Slot()[SNew(SPanelSurface)]
           +SOverlay::Slot().Padding(44,32)
           [SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight()
            [SNew(SHorizontalBox)
             +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
             [SNew(STextBlock).Text(FText::FromString(TEXT("PREPARE YOUR RUN"))).Font(Font(31,true)).ColorAndOpacity(Paper)]
             +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(STextBlock).Font(Font(16)).ColorAndOpacity(Muted).Text(FText::FromString(SetupPage == ERunSetupPage::ArenaAndStances ? TEXT("STEP 1 / 2") : TEXT("STEP 2 / 2")))]]
            +SVerticalBox::Slot().AutoHeight().Padding(0,7,0,0)
            [SNew(STextBlock).Text(FText::FromString(SetupPage == ERunSetupPage::ArenaAndStances
                ? TEXT("Choose your map and difficulty.")
                : TEXT("Choose your loadout."))).Font(Font(16)).ColorAndOpacity(Muted)]
            +SVerticalBox::Slot().AutoHeight().Padding(0,18,0,18)[SNew(SOrnamentDivider)]
            +SVerticalBox::Slot().FillHeight(1)[Content]
            +SVerticalBox::Slot().AutoHeight().Padding(0,17,0,16)[SNew(SOrnamentDivider)]
            +SVerticalBox::Slot().AutoHeight()
            [SNew(SHorizontalBox)
             +SHorizontalBox::Slot().AutoWidth()[Back]
             +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(24,0)
             [SNew(STextBlock).Font(Font(16)).ColorAndOpacity(Muted)
              .Text_Lambda([this]() { const auto* A = RunStances::Find(SelectedSamurai); const auto* B = RunStances::Find(SelectedNinja);
                  return FText::FromString(A && B ? FString::Printf(TEXT("%s   +   %s"), A->Name, B->Name) : TEXT("Select one stance for each hero")); })]
             +SHorizontalBox::Slot().AutoWidth()[Start]]]]]];
}

FReply UStanceSelectionWidget::ConfirmRun()
{
    if (!CanStartRun()) return FReply::Handled();
    if (SetupPage == ERunSetupPage::ArenaAndStances)
    { SelectSetupPage(ERunSetupPage::SynergiesAndCombo); return FReply::Handled(); }
    auto* GI = GetGameInstance();
    auto* Loadout = GI ? GI->GetSubsystem<URunLoadoutSubsystem>() : nullptr;
    if (!Loadout || !Loadout->SetStances(SelectedSamurai, SelectedNinja)) return FReply::Handled();
    if (auto* Travel = GI->GetSubsystem<URunTravelSubsystem>()) Travel->ClearSnapshot();
    bStarting = true;
    UGameplayStatics::OpenLevel(this, URunTravelSubsystem::GetStartingLevel(this));
    return FReply::Handled();
}

void UStanceSelectionWidget::FocusButton(int32 Index)
{
    if(Buttons.IsEmpty()) return;
    FocusedButton = FMath::Clamp(Index,0,Buttons.Num()-1);
    if (Buttons[FocusedButton] && Buttons[FocusedButton]->IsEnabled())
    {
        FSlateApplication::Get().SetKeyboardFocus(Buttons[FocusedButton], EFocusCause::Navigation);
        if (SynergyScroll && ScrollNavigationIndices.Contains(FocusedButton))
            SynergyScroll->ScrollDescendantIntoView(Buttons[FocusedButton],false,EDescendantScrollDestination::IntoView);
    }
}

void UStanceSelectionWidget::MoveFocus(FVector2D Direction)
{
    if(!NavigationPositions.IsValidIndex(FocusedButton)) return;
    int32 Best=INDEX_NONE; float BestScore=MAX_flt;
    for(int32 I=0;I<Buttons.Num();++I)
    {
        if(!Buttons[I] || !Buttons[I]->IsEnabled() || I==FocusedButton) continue;
        const FVector2D Delta=NavigationPositions[I]-NavigationPositions[FocusedButton];
        const float Forward=FVector2D::DotProduct(Delta,Direction);
        if(Forward<=0) continue;
        const float Across=FMath::Abs(FVector2D::CrossProduct(Delta,Direction));
        const float Score=Forward+Across*4;
        if(Score<BestScore){BestScore=Score;Best=I;}
    }
    if(Best!=INDEX_NONE) FocusButton(Best);
}

FReply UStanceSelectionWidget::NativeOnFocusReceived(const FGeometry& Geometry, const FFocusEvent& Event)
{
    if (Buttons.IsValidIndex(FocusedButton) && Buttons[FocusedButton])
        return FReply::Handled().SetUserFocus(Buttons[FocusedButton].ToSharedRef(), EFocusCause::Navigation);
    return Super::NativeOnFocusReceived(Geometry, Event);
}

FReply UStanceSelectionWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    const FKey Key = Event.GetKey();
    for(auto& Picker:LoadoutPickers)if(Picker && Picker->IsOpen())
    {
        if(Key==EKeys::Escape || Key==EKeys::Gamepad_FaceButton_Right)
        {Picker->SetIsOpen(false);return FReply::Handled();}
        return Super::NativeOnPreviewKeyDown(Geometry,Event);
    }
    if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
    { if (SetupPage == ERunSetupPage::SynergiesAndCombo) SelectSetupPage(ERunSetupPage::ArenaAndStances); else if (MenuOwner) MenuOwner->ShowMainPanel(); return FReply::Handled(); }
    for (int32 I=0; I<Buttons.Num(); ++I)
        if (Buttons[I] && (Buttons[I]->HasKeyboardFocus() || Buttons[I]->HasAnyUserFocus())) FocusedButton=I;
    if(Key==EKeys::Tab && !Buttons.IsEmpty())
    {
        for(int32 Step=1;Step<=Buttons.Num();++Step)
        {
            const int32 I=(FocusedButton+Buttons.Num()+(Event.IsShiftDown() ? -Step : Step))%Buttons.Num();
            if(Buttons[I] && Buttons[I]->IsEnabled()){FocusButton(I);break;}
        }
        return FReply::Handled();
    }
    if (Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right) {MoveFocus({1,0});return FReply::Handled();}
    if (Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left) {MoveFocus({-1,0});return FReply::Handled();}
    if (Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down) {MoveFocus({0,1});return FReply::Handled();}
    if (Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up) {MoveFocus({0,-1});return FReply::Handled();}
    return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

void UStanceSelectionWidget::ReleaseSlateResources(bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    Buttons.Reset(); NavigationPositions.Reset(); ScrollNavigationIndices.Reset(); SynergyScroll.Reset();
    for(auto& Picker:LoadoutPickers)Picker.Reset(); PageContent.Reset();
}
