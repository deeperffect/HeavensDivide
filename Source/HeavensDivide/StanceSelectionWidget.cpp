#include "StanceSelectionWidget.h"
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
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

using namespace BloodshiftMenu;

namespace
{
struct FRunMapEntry { const TCHAR* Id; const TCHAR* Name; const TCHAR* Description; bool bAvailable; };
const FRunMapEntry RunMaps[] = {
    { TEXT("FallenGrove"), TEXT("THE FALLEN GROVE"), TEXT("Stone paths. Restless spirits."), true },
    { TEXT("CinderCitadel"), TEXT("CINDER CITADEL"), TEXT("Beyond the burning gates."), false },
    { TEXT("HollowMarsh"), TEXT("HOLLOW MARSH"), TEXT("Where the mist remembers."), false }
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
        const auto Size = G.GetLocalSize();
        const FOrnamentPainter P{Out, G, Layer, Style.GetColorAndOpacityTint().A};
        const auto Accent = MapIndex == 0 ? Gold : Muted.CopyWithNewOpacity(.55f);
        auto At = [&](float X, float Y) { return FVector2D(X * Size.X, Y * Size.Y); };
        P.Arc(At(.64f,.32f), Size.Y * .21f, 0, 2 * PI, Accent.CopyWithNewOpacity(.5f));
        P.Line({At(.05f,.9f), At(.23f,.66f), At(.41f,.8f), At(.58f,.55f), At(.81f,.76f), At(.97f,.66f)}, Rule, 1.5f);
        if (MapIndex == 1)
        {
            for (float Level : {.38f, .59f, .8f})
            {
                P.Line({At(.24f,Level), At(.5f,Level-.19f), At(.76f,Level)}, Accent, 1.5f);
                P.Line({At(.33f,Level+.02f), At(.33f,Level+.13f), At(.67f,Level+.13f), At(.67f,Level+.02f)}, Accent);
            }
        }
        else
        {
            for (float X : {.16f,.34f,.78f,.90f})
            {
                const float Top = X < .5f ? .23f : .37f;
                P.Line({At(X,.87f), At(X,Top)}, Accent, 1.5f);
                P.Line({At(X-.065f,Top+.23f), At(X,Top+.08f), At(X+.06f,Top+.23f)}, Accent);
                P.Line({At(X-.085f,Top+.39f), At(X,Top+.18f), At(X+.085f,Top+.39f)}, Accent);
            }
            if (MapIndex == 0) P.Line({At(.40f,.95f),At(.56f,.75f),At(.50f,.61f)}, Gold.CopyWithNewOpacity(.75f), 2);
            else for(float Y : {.76f,.86f,.96f}) P.Line({At(.15f,Y),At(.82f,Y-.015f)}, Muted.CopyWithNewOpacity(.35f));
        }
        return Layer;
    }
private:
    int32 MapIndex = 0;
};
}

UStanceSelectionWidget::UStanceSelectionWidget(const FObjectInitializer& Initializer) : Super(Initializer)
{
    SetIsFocusable(true);
    HoverScale = 1.015f;
    PressScale = 1.025f;
    static ConstructorHelpers::FObjectFinder<UObject> Heading(TEXT("/Game/Assets/Fonts/Cinzel-Medium_Font.Cinzel-Medium_Font"));
    HeadingFont = FSlateFontInfo(Heading.Object, 24, TEXT("Default"));
    ActionStyle = BloodshiftMenu::ActionStyle();
    CardStyle = ActionStyle;
    CardStyle.SetNormal(Solid(Surface)).SetHovered(Solid(FLinearColor(.035f,.065f,.070f)))
        .SetPressed(Solid(FLinearColor(.07f,.08f,.065f)))
        .SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
    RankStyle = ActionStyle;
    RankStyle.SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
    SelectedBrush = Solid(BloodshiftMenu::Gold);
    EmptyBrush = Solid(Rule);
}

FSlateFontInfo UStanceSelectionWidget::Font(int32 Size, bool bHeading) const
{
    auto Result = bHeading ? HeadingFont : BodyFont(Size);
    Result.Size = Size;
    return Result;
}

void UStanceSelectionWidget::Open()
{
    bStarting = false;
    if (auto* Loadout = GetGameInstance() ? GetGameInstance()->GetSubsystem<URunLoadoutSubsystem>() : nullptr)
    {
        SelectedSamurai = Loadout->GetSamuraiStance();
        SelectedNinja = Loadout->GetNinjaStance();
    }
    // The menu keeps this page alive. Rebuild its content after purchases or a profile reset.
    if (PageContent) PageContent->SetContent(BuildPage());
    else TakeWidget();
    FocusButton(0);
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
    int32 Count = 0;
    for (const auto& Stance : RunStances::All) if (IsStanceAvailable(Stance.Id)) ++Count;
    return Count;
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
        if (Id == Map.Id && Map.bAvailable) { SelectedMap = Id; return true; }
    return false;
}

bool UStanceSelectionWidget::CanStartRun() const
{
    const auto* Steel = RunStances::Find(SelectedSamurai);
    const auto* Shadow = RunStances::Find(SelectedNinja);
    return !bStarting && SelectedMap == TEXT("FallenGrove") && Steel && Steel->bSamurai && Shadow && !Shadow->bSamurai &&
        IsStanceAvailable(SelectedSamurai) && IsStanceAvailable(SelectedNinja);
}

bool UStanceSelectionWidget::IsSelected(int32 Index) const
{
    const auto& Stance = RunStances::All[Index];
    return (Stance.bSamurai ? SelectedSamurai : SelectedNinja) == Stance.Id;
}

void UStanceSelectionWidget::AddNavigation(const TSharedRef<SButton>& Button, FVector2D Position)
{
    Buttons.Add(Button);
    NavigationPositions.Add(Position);
}

TSharedRef<SWidget> UStanceSelectionWidget::MakeMapCard(int32 Index)
{
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
           [SNew(STextBlock).Text(FText::FromString(Map.bAvailable ? TEXT("SELECTED") : TEXT("COMING LATER")))
            .Font(Font(11)).ColorAndOpacity(Map.bAvailable ? BloodshiftMenu::Gold : Muted).Justification(ETextJustify::Center)]]]];
    if (Map.bAvailable) AddNavigation(Button,{float(Index)*2,0});
    return SNew(SBox).WidthOverride(206).HeightOverride(157)[Button];
}

TSharedRef<SWidget> UStanceSelectionWidget::MakeModifierRow(int32 Index)
{
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
            .IsEnabled_Lambda([this,Challenge]() { return !bStarting && Challenge && Challenge->CanEditModifiers(); })
            .ToolTipText(FText::FromString(Rank == 0 ? TEXT("Standard difficulty") : FString::Printf(TEXT("Rank %d: +%d%%"),Rank,FMath::RoundToInt(100.f*Rank*Modifier.PerRank))))
            .OnClicked_Lambda([Challenge,Id,Rank]() { if(Challenge) Challenge->SetModifierRank(Id,Rank); return FReply::Handled(); })
            [SNew(SBorder).Padding(1)
             .BorderImage_Lambda([this,Challenge,Id,Rank]() { return (Challenge ? Challenge->GetModifierRank(Id) : 0) == Rank ? &SelectedBrush : &EmptyBrush; })
             [SNew(SBorder).Padding(3).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Ink)
              [SNew(STextBlock).Text(FText::AsNumber(Rank)).Font(Font(15)).Justification(ETextJustify::Center)
               .ColorAndOpacity_Lambda([Challenge,Id,Rank]() { return (Challenge ? Challenge->GetModifierRank(Id) : 0) == Rank ? BloodshiftMenu::Gold : Paper; })]]];
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

TSharedRef<SWidget> UStanceSelectionWidget::MakeStanceCard(int32 Index, int32 Count, int32 Column)
{
    const auto& Stance = RunStances::All[Index];
    const FLinearColor Accent = Stance.bSamurai ? FLinearColor(.85f,.26f,.16f) : FLinearColor(.59f,.43f,.87f);
    const bool bWide = Count == 1;
    auto Labels = SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight()
        [SNew(STextBlock).Text(FText::FromString(Stance.Name)).Font(Font(bWide ? 25 : 17,true)).ColorAndOpacity(Paper)
         .Justification(bWide ? ETextJustify::Left : ETextJustify::Center).WrapTextAt(bWide ? 390 : 182)]
        +SVerticalBox::Slot().AutoHeight().Padding(0,7,0,0)
        [SNew(STextBlock).Text(FText::FromString(Stance.Tagline)).Font(Font(10)).ColorAndOpacity(Accent)
         .Justification(bWide ? ETextJustify::Left : ETextJustify::Center).WrapTextAt(bWide ? 390 : 182)]
        +SVerticalBox::Slot().AutoHeight().Padding(0,10,0,0)
        [SNew(STextBlock).Text(FText::FromString(Stance.Summary)).Font(Font(15)).ColorAndOpacity(Paper.CopyWithNewOpacity(.85f))
         .WrapTextAt(380).Visibility(bWide ? EVisibility::Visible : EVisibility::Collapsed)]
        +SVerticalBox::Slot().AutoHeight().Padding(0,bWide ? 17 : 10,0,0)
        [SNew(STextBlock).Font(Font(11)).Justification(bWide ? ETextJustify::Left : ETextJustify::Center)
         .ColorAndOpacity_Lambda([this,Index]() { return IsSelected(Index) ? BloodshiftMenu::Gold : Muted; })
         .Text_Lambda([this,Index]() { return FText::FromString(IsSelected(Index) ? TEXT("SELECTED") : TEXT("CHOOSE STANCE")); })];
    auto Art = SNew(SBox).WidthOverride(bWide ? 150 : 105).HeightOverride(bWide ? 162 : 82)
        [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SImage).Image(&Artwork[Index])]];
    TSharedRef<SWidget> Content = SNullWidget::NullWidget;
    if (bWide)
        Content = SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Art]
            +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(22,0,0,0)[Labels];
    else
        Content = SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Art]
            +SVerticalBox::Slot().AutoHeight().Padding(0,6,0,0)[Labels];
    auto Button = SNew(SButton).ButtonStyle(&CardStyle).ContentPadding(0)
        .ToolTipText(Definitions[Index]->GetBalanceDescription())
        .OnClicked_Lambda([this, Index]() { SelectStance(RunStances::All[Index].Id); return FReply::Handled(); })
        [SNew(SBorder).Padding(2)
         .BorderImage_Lambda([this,Index]() { return IsSelected(Index) ? &SelectedBrush : &EmptyBrush; })
         [SNew(SOverlay)
          +SOverlay::Slot()[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Ink)]
          +SOverlay::Slot()[SNew(SInsetSurface).Compact(true).Fill(false).Visibility(EVisibility::HitTestInvisible)]
          +SOverlay::Slot().Padding(bWide ? 18 : 10)[Content]]];
    AddNavigation(Button,{float(7+Column),Stance.bSamurai ? 2.f : 5.f});
    return SNew(SBox).WidthOverride((660.f - (Count-1)*10.f)/Count).HeightOverride(214)[Button];
}

TSharedRef<SWidget> UStanceSelectionWidget::MakeCharacterGroup(bool bSamurai)
{
    TArray<int32> Available;
    for(int32 I=0;I<UE_ARRAY_COUNT(RunStances::All);++I)
        if(RunStances::All[I].bSamurai == bSamurai && IsStanceAvailable(RunStances::All[I].Id)) Available.Add(I);
    auto Cards = SNew(SHorizontalBox);
    for(int32 I=0;I<Available.Num();++I)
        Cards->AddSlot().AutoWidth().Padding(I ? 10 : 0,0,0,0)[MakeStanceCard(Available[I],Available.Num(),I)];
    return SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)
        [SNew(SHorizontalBox)
         +SHorizontalBox::Slot().FillWidth(1)
         [SNew(STextBlock).Text(FText::FromString(bSamurai ? TEXT("SAMURAI") : TEXT("NINJA"))).Font(Font(21,true)).ColorAndOpacity(Paper)]
         +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
         [SNew(STextBlock).Text(FText::FromString(TEXT("CHOOSE A STANCE"))).Font(Font(11)).ColorAndOpacity(Muted)]]
        +SVerticalBox::Slot().AutoHeight()[Cards];
}

TSharedRef<SWidget> UStanceSelectionWidget::RebuildWidget()
{
    return SAssignNew(PageContent,SBox)[BuildPage()];
}

TSharedRef<SWidget> UStanceSelectionWidget::BuildPage()
{
    Definitions.Reset(); Artwork.Reset(); Buttons.Reset(); NavigationPositions.Reset();
    const auto* PCClass = LoadClass<ASurvivorPlayerController>(nullptr, TEXT("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController.BP_SurvivorPlayerController_C"));
    const auto* Upgrades = PCClass ? PCClass->GetDefaultObject<ASurvivorPlayerController>()->GetPlayerUpgrades() : nullptr;
    for (const auto& Stance : RunStances::All)
    {
        auto* Definition = Upgrades ? Upgrades->FindUpgradeDefinition(Stance.Id) : nullptr;
        Definitions.Add(Definition);
        auto* Texture = Definition ? (Definition->CardArtwork ? Definition->CardArtwork.Get() : Definition->Icon.Get()) : nullptr;
        FSlateBrush Brush; Brush.ImageSize = Texture ? FVector2D(Texture->GetSizeX(),Texture->GetSizeY()) : FVector2D(256,256);
        Brush.SetResourceObject(Texture);
        Artwork.Add(Brush);
    }
    if(!IsStanceAvailable(SelectedSamurai)) SelectedSamurai=TEXT("BattleStance");
    if(!IsStanceAvailable(SelectedNinja)) SelectedNinja=TEXT("BarrageStance");
    auto Maps = SNew(SHorizontalBox);
    for(int32 I=0;I<UE_ARRAY_COUNT(RunMaps);++I)
        Maps->AddSlot().AutoWidth().Padding(I ? 11 : 0,0,0,0)[MakeMapCard(I)];
    auto Modifiers = SNew(SVerticalBox);
    for(int32 I=0;I<RunChallenges::Modifiers().Num();++I)
        Modifiers->AddSlot().AutoHeight()[MakeModifierRow(I)];
    auto Samurai = MakeCharacterGroup(true);
    auto Ninja = MakeCharacterGroup(false);
    auto Back = SNew(SButton).ButtonStyle(&ActionStyle)
        .OnClicked_Lambda([this]() { if (MenuOwner) MenuOwner->ShowMainPanel(); return FReply::Handled(); })
        [SNew(STextBlock).Text(FText::FromString(TEXT("BACK"))).Font(Font(17)).ColorAndOpacity(FSlateColor::UseForeground())];
    auto Start = SNew(SButton).ButtonStyle(&ActionStyle)
        .IsEnabled_Lambda([this]() { return CanStartRun(); })
        .OnClicked_UObject(this, &UStanceSelectionWidget::ConfirmRun)
        [SNew(STextBlock).Text(FText::FromString(TEXT("BEGIN RUN"))).Font(Font(20,true)).ColorAndOpacity(FSlateColor::UseForeground())];
    AddNavigation(Back,{0,8}); AddNavigation(Start,{9,8});
    return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("NoBrush")).Padding(20)
        [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
         [SNew(SBox).WidthOverride(1440).HeightOverride(850)
          [SNew(SOverlay)
           +SOverlay::Slot()[SNew(SPanelSurface)]
           +SOverlay::Slot().Padding(44,32)
           [SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight()
            [SNew(STextBlock).Text(FText::FromString(TEXT("PREPARE YOUR RUN"))).Font(Font(31,true)).ColorAndOpacity(Paper)]
            +SVerticalBox::Slot().AutoHeight().Padding(0,7,0,0)
            [SNew(STextBlock).Text(FText::FromString(TEXT("Choose your destination, raise the stakes, and bring a stance for each soul."))).Font(Font(16)).ColorAndOpacity(Muted)]
            +SVerticalBox::Slot().AutoHeight().Padding(0,18,0,18)[SNew(SOrnamentDivider)]
            +SVerticalBox::Slot().FillHeight(1)
            [SNew(SHorizontalBox)
             +SHorizontalBox::Slot().AutoWidth()
             [SNew(SBox).WidthOverride(640)
              [SNew(SVerticalBox)
               +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)
               [SNew(STextBlock).Text(FText::FromString(TEXT("DESTINATION"))).Font(Font(21,true)).ColorAndOpacity(Paper)]
               +SVerticalBox::Slot().AutoHeight()[Maps]
               +SVerticalBox::Slot().AutoHeight().Padding(0,14,0,0)
               [SNew(STextBlock).Text(FText::FromString(TEXT("CHALLENGES"))).Font(Font(21,true)).ColorAndOpacity(Paper)]
               +SVerticalBox::Slot().AutoHeight().Padding(0,5,0,8)
               [SNew(STextBlock).Text(FText::FromString(TEXT("0 is standard. Each rank increases currency rewards by 8%."))).Font(Font(13)).ColorAndOpacity(Muted)]
               +SVerticalBox::Slot().AutoHeight()[Modifiers]
               +SVerticalBox::Slot().AutoHeight().Padding(0,7,0,0)
               [SNew(STextBlock).Font(Font(17,true)).ColorAndOpacity(BloodshiftMenu::Gold)
                .Text_Lambda([this]() { const auto* C=GetGameInstance() ? GetGameInstance()->GetSubsystem<URunChallengeSubsystem>() : nullptr;
                    return FText::FromString(FString::Printf(TEXT("CURRENCY REWARDS   +%d%%"),FMath::RoundToInt(100.f*((C ? C->GetRewardMultiplier() : 1.f)-1.f)))); })]]]
             +SHorizontalBox::Slot().FillWidth(1)[SNew(SBox)]
             +SHorizontalBox::Slot().AutoWidth()
             [SNew(SBox).WidthOverride(660)
              [SNew(SVerticalBox)
               +SVerticalBox::Slot().AutoHeight()[Samurai]
               +SVerticalBox::Slot().AutoHeight().Padding(0,18,0,0)[Ninja]
               +SVerticalBox::Slot().AutoHeight().Padding(0,7,0,0)
               [SNew(STextBlock).Text(FText::FromString(TEXT("Unlock more stances in the skill tree."))).Font(Font(13)).ColorAndOpacity(Muted)]]]]
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
    auto* GI = GetGameInstance();
    auto* Loadout = GI ? GI->GetSubsystem<URunLoadoutSubsystem>() : nullptr;
    if (!Loadout || !Loadout->SetStances(SelectedSamurai, SelectedNinja)) return FReply::Handled();
    if (auto* Travel = GI->GetSubsystem<URunTravelSubsystem>()) Travel->ClearSnapshot();
    bStarting = true;
    UGameplayStatics::OpenLevel(this, TEXT("/Game/Maps/Lvl_B1_Lvl1"));
    return FReply::Handled();
}

void UStanceSelectionWidget::FocusButton(int32 Index)
{
    if(Buttons.IsEmpty()) return;
    FocusedButton = FMath::Clamp(Index,0,Buttons.Num()-1);
    if (Buttons[FocusedButton] && Buttons[FocusedButton]->IsEnabled())
        FSlateApplication::Get().SetKeyboardFocus(Buttons[FocusedButton], EFocusCause::Navigation);
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
    if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
    { if (MenuOwner) MenuOwner->ShowMainPanel(); return FReply::Handled(); }
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
    Buttons.Reset(); NavigationPositions.Reset(); PageContent.Reset();
}
