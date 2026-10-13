#include "RunReportView.h"
#include "SteamLeaderboardSubsystem.h"
#include "BloodshiftMenuStyle.h"
#include "SurvivorPlayerController.h"
#include "SynergyMetaProgressionSubsystem.h"
#include "UpgradeDefinition.h"
#include "RunChallengeSubsystem.h"
#include "PlayerUpgradeRules.h"
#include "BarrageBuild.h"
#include "FangBuild.h"
#include "ShurikenBuild.h"
#include "Engine/Texture2D.h"
#include "Engine/GameInstance.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Notifications/SProgressBar.h"

namespace
{
    const FButtonStyle& Action() { static const auto Style = BloodshiftMenu::ActionStyle(); return Style; }
    const FProgressBarStyle* DamageBar()
    {
        static const auto Style = FProgressBarStyle()
            .SetBackgroundImage(BloodshiftMenu::Solid(BloodshiftMenu::Rule))
            .SetFillImage(BloodshiftMenu::Solid(BloodshiftMenu::Gold));
        return &Style;
    }
    const FSlateBrush* Surface() { static const auto Brush = BloodshiftMenu::Solid(BloodshiftMenu::Surface); return &Brush; }
    TSharedRef<STextBlock> Text(const FString& Value, int32 Size = 17, FLinearColor Color = BloodshiftMenu::Paper)
    {
        return SNew(STextBlock).Text(FText::FromString(Value)).Font(BloodshiftMenu::BodyFont(Size)).ColorAndOpacity(Color).AutoWrapText(true);
    }
    FString Number(double Value) { return FText::AsNumber(FMath::RoundToInt64(Value)).ToString(); }
    FString Time(double Seconds)
    {
        const int32 Whole = FMath::Max(0, int32(Seconds));
        return FString::Printf(TEXT("%02d:%02d"), Whole / 60, Whole % 60);
    }
    void Heading(const TSharedRef<SVerticalBox>& Box, const FString& Title, const FString& Subtitle)
    {
        Box->AddSlot().AutoHeight().Padding(0, 0, 0, 8)[Text(Title, 25)];
        if (!Subtitle.IsEmpty()) Box->AddSlot().AutoHeight().Padding(0, 0, 0, 22)[Text(Subtitle, 15, BloodshiftMenu::Muted)];
    }
    void Stat(const TSharedRef<SVerticalBox>& Box, const FString& Label, const FString& Value, bool bHighlight = false)
    {
        Box->AddSlot().AutoHeight().Padding(0, 8)[SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1)[Text(Label, 18, bHighlight ? BloodshiftMenu::Paper : BloodshiftMenu::Muted)]
            + SHorizontalBox::Slot().AutoWidth().Padding(20, 0)[Text(Value, bHighlight ? 24 : 18, bHighlight ? BloodshiftMenu::Gold : BloodshiftMenu::Paper)]];
    }
    TArray<TPair<FName, int32>> SortedBuild(const FPlayerUpgradeRunState& Build)
    {
        TArray<TPair<FName, int32>> Result;
        for (const auto& Pair : Build.Levels) if (Pair.Value > 0) Result.Add(Pair);
        Result.Sort([&](const auto& A, const auto& B)
        {
            const auto* DA = Build.Definitions.FindRef(A.Key).Get(); const auto* DB = Build.Definitions.FindRef(B.Key).Get();
            if (DA && DB && DA->Role != DB->Role)
            {
                auto Priority = [](EUpgradeRole Role) { return Role == EUpgradeRole::Starter ? 0 : Role == EUpgradeRole::Evolution ? 1 : 2; };
                if (Priority(DA->Role) != Priority(DB->Role)) return Priority(DA->Role) < Priority(DB->Role);
            }
            return (DA ? DA->DisplayName.ToString() : A.Key.ToString()) < (DB ? DB->DisplayName.ToString() : B.Key.ToString());
        });
        return Result;
    }
}
void SRunReportView::Construct(const FArguments& Args)
{
    Report = Args._Report; Controller = Args._Controller; bResults = Args._Results;
    if (Report.IsValid() && Controller.IsValid()) Report->CaptureBuild(Controller->GetPlayerUpgrades());
    auto Navigation = SNew(SVerticalBox);
    const TCHAR* Labels[] = {TEXT("OVERVIEW"), TEXT("RUN BUILD"), TEXT("DAMAGE"), TEXT("SCORE")};
    for (int32 I = 0; I < 4; ++I)
        Navigation->AddSlot().AutoHeight().Padding(0, 0, 0, 12)[SNew(SBorder).Padding(1)
            .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
            .BorderBackgroundColor_Lambda([this, I] { return Tab == I ? BloodshiftMenu::Gold : FLinearColor::Transparent; })
            [SNew(SButton).ButtonStyle(&Action())
            .OnClicked_Lambda([this, I] { SelectTab(I); return FReply::Handled(); })
            [SNew(STextBlock).Text(FText::FromString(Labels[I])).Font(BloodshiftMenu::BodyFont(16))
                .ColorAndOpacity(FSlateColor::UseForeground())]]];
    Navigation->AddSlot().FillHeight(1)[SNew(SBox)];
    ChildSlot[SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 26, 0)[SNew(SBox).WidthOverride(180)[Navigation]]
        + SHorizontalBox::Slot().FillWidth(1)[SAssignNew(Page, SBox)]];
    SelectTab(bResults ? 0 : 1);
}
void SRunReportView::SelectTab(int32 Index)
{
    Tab = FMath::Clamp(Index, 0, 3); IconBrushes.Reset();
    TSharedRef<SWidget> Content = Tab == 0 ? Overview() : Tab == 1 ? BuildPage() : Tab == 2 ? DamagePage() : ScorePage();
    Page->SetContent(SNew(SScrollBox) + SScrollBox::Slot().Padding(0, 0, 12, 0)[Content]);
}
TSharedRef<SWidget> SRunReportView::Overview()
{
    auto Box = SNew(SVerticalBox);
    const FRunScoreEntry Run = Report.IsValid() ? Report->Summary() : FRunScoreEntry();
    const auto* Map = RunReports::FindMap(Run.MapId);
    int64 TotalKills = 0;
    if (Report.IsValid()) for (const auto& Row : Report->DamageRows()) TotalKills += Row.Kills;
    Heading(Box, bResults ? TEXT("RUN SUMMARY") : TEXT("CURRENT RUN"), Map ? Map->Title : TEXT("Practice run"));
    Stat(Box, TEXT("Time survived"), Time(Run.Seconds));
    Stat(Box, TEXT("Enemies defeated"), Number(static_cast<double>(TotalKills)));
    Stat(Box, TEXT("Elites / bosses"), FString::Printf(TEXT("%d / %d"), Run.EliteKills, Run.BossKills));
    Stat(Box, TEXT("Objectives completed"), Number(Run.Objectives));
    Stat(Box, bResults ? TEXT("Final score") : TEXT("Score so far"), Number(Run.CalculateScore()), true);
    if (Controller.IsValid()) if (auto* GI = Controller->GetGameInstance())
        if (auto* Meta = GI->GetSubsystem<USynergyMetaProgressionSubsystem>())
        {
            Stat(Box, TEXT("Run spoils"), FString::Printf(TEXT("%d embers  ·  %d soul sigils"), Meta->GetRunEmbers(), Meta->GetRunSigils()));
            if (Meta->HasPendingSkillReward()) Box->AddSlot().AutoHeight()[Text(TEXT("Saving rewards…"), 14, BloodshiftMenu::Muted)];
        }
    return Box;
}
TSharedRef<SWidget> SRunReportView::BuildPage()
{
    auto Box = SNew(SVerticalBox);
    const FPlayerUpgradeRunState Build = Report.IsValid() ? Report->Build() : FPlayerUpgradeRunState();
    Heading(Box, TEXT("RUN BUILD"), FString::Printf(TEXT("Samurai mastery %d  ·  Ninja mastery %d  ·  Rerolls %d  ·  Banishes %d"), Build.SamuraiMastery, Build.NinjaMastery, Build.RerollsUsed, Build.BanishesUsed));
    auto Columns = SNew(SHorizontalBox);
    const auto Upgrades = SortedBuild(Build);
    for (int32 Group = 0; Group < 3; ++Group)
    {
        auto Cards = SNew(SVerticalBox);
        const TCHAR* Titles[] = {TEXT("SAMURAI"), TEXT("NINJA"), TEXT("SHARED & SYNERGIES")};
        Cards->AddSlot().AutoHeight().Padding(0, 0, 0, 14)[Text(Titles[Group], 16, BloodshiftMenu::Gold)];
        int32 Count = 0;
        for (const auto& Pair : Upgrades)
        {
            const auto* Def = Build.Definitions.FindRef(Pair.Key).Get();
            const bool bSamuraiCard = Def && (Def->InvestmentOwner == EUpgradeInvestmentOwner::Samurai
                || Def->Category == EUpgradeCategory::Samurai || Def->Category == EUpgradeCategory::SamuraiTrial
                || PlayerUpgradeRules::IsBloodUpgrade(Pair.Key) || PlayerUpgradeRules::IsIaijutsuUpgrade(Pair.Key)
                || PlayerUpgradeRules::IsCrescentUpgrade(Pair.Key) || PlayerUpgradeRules::IsSamuraiScalingUpgrade(Pair.Key));
            const bool bNinjaCard = Def && (Def->InvestmentOwner == EUpgradeInvestmentOwner::Ninja
                || Def->Category == EUpgradeCategory::Ninja || Def->Category == EUpgradeCategory::NinjaTrial
                || BarrageBuild::IsUpgrade(Pair.Key) || FangBuild::IsUpgrade(Pair.Key) || ShurikenBuild::IsUpgrade(Pair.Key));
            const int32 DefGroup = bSamuraiCard ? 0 : bNinjaCard ? 1 : 2;
            if (DefGroup != Group) continue;
            ++Count;
            FString Name = Def ? Def->DisplayName.ToString() : FName::NameToDisplayString(Pair.Key.ToString(), false);
            const bool bEvolution = Def && Def->Role == EUpgradeRole::Evolution;
            const FString Rank = bEvolution ? FString::Printf(TEXT("EVOLVED  ·  Rank %d"), Pair.Value)
                : Def && Def->Role == EUpgradeRole::Starter ? TEXT("STANCE / WEAPON") : FString::Printf(TEXT("Rank %d%s"), Pair.Value, Def && Pair.Value >= Def->MaxLevel ? TEXT("  ·  MAX") : TEXT(""));
            auto Card = SNew(SHorizontalBox);
            if (Def && Def->Icon)
            {
                TSharedPtr<FSlateBrush> Brush = MakeShared<FSlateBrush>(); Brush->SetResourceObject(Def->Icon); Brush->ImageSize = FVector2D(40, 40);
                IconBrushes.Add(Brush);
                Card->AddSlot().AutoWidth().Padding(0, 0, 12, 0)[SNew(SBox).WidthOverride(40).HeightOverride(40)[SNew(SImage).Image(Brush.Get())]];
            }
            Card->AddSlot().FillWidth(1)[SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[Text(Name, 16)]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 0)[Text(Rank, 13, bEvolution ? BloodshiftMenu::Gold : BloodshiftMenu::Muted)]];
            Cards->AddSlot().AutoHeight().Padding(0, 0, 0, 10)[SNew(SBorder).BorderImage(Surface()).Padding(12)
                .ToolTipText(Def ? Def->GetBalanceDescription() : FText::FromString(Name))[Card]];
        }
        if (!Count) Cards->AddSlot().AutoHeight()[Text(TEXT("No upgrades yet"), 14, BloodshiftMenu::Muted)];
        Columns->AddSlot().FillWidth(1).Padding(Group == 0 ? 0 : 10, 0, 0, 0)[Cards];
    }
    Box->AddSlot().AutoHeight()[Columns];
    return Box;
}
TSharedRef<SWidget> SRunReportView::DamagePage()
{
    auto Box = SNew(SVerticalBox);
    TArray<FRunDamageRow> Rows = Report.IsValid() ? Report->DamageRows() : TArray<FRunDamageRow>();
    // Equipped direct effects with no hits are still useful evidence for build comparison.
    static const TMap<FName, FName> Sources = {
        {TEXT("BattleStance"), TEXT("SamuraiAttack")}, {TEXT("BladeWave"), TEXT("Crescent")}, {TEXT("Iaijutsu"), TEXT("Iaijutsu")},
        {TEXT("BarrageStance"), TEXT("Barrage")}, {TEXT("ReturningFang"), TEXT("ReturningFang")}, {TEXT("GreatShuriken"), TEXT("GreatShuriken")},
        {TEXT("BloodDetonation"), TEXT("BloodDetonation")}, {TEXT("BloodEcho"), TEXT("BloodEcho")}, {TEXT("DoubleCut"), TEXT("DoubleCut")},
        {TEXT("BarragePool"), TEXT("BarragePool")}, {TEXT("BarrageBloom"), TEXT("BarrageBloom")}, {TEXT("ShadowClone"), TEXT("ShadowClone")},
        {TEXT("TagTeam"), TEXT("TagTeam")}, {TEXT("LingeringWake"), TEXT("LingeringWake")}, {TEXT("EmbeddedBlades"), TEXT("EmbeddedBlades")}
    };
    if (Report.IsValid()) for (const auto& Pair : Report->Build().Levels)
        if (Pair.Value > 0) if (const auto* Source = Sources.Find(Pair.Key))
            if (!Rows.ContainsByPredicate([&](const auto& Row) { return Row.Source == *Source; }))
            { auto& Row = Rows.AddDefaulted_GetRef(); Row.Source = *Source; }
    Rows.Sort([](const auto& A, const auto& B) { return A.Damage == B.Damage ? A.Source.LexicalLess(B.Source) : A.Damage > B.Damage; });
    double Total = 0; for (const auto& Row : Rows) Total += Row.Damage;
    Heading(Box, TEXT("DAMAGE"), TEXT(""));
    Stat(Box, TEXT("Total damage"), Number(Total), true);
    const double Seconds = Report.IsValid() ? Report->Summary().Seconds : 0;
    for (const auto& Row : Rows)
    {
        const float Share = Total > 0 ? float(Row.Damage / Total) : 0.f;
        auto Entry = SNew(SVerticalBox);
        Entry->AddSlot().AutoHeight()[SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1)[Text(RunReports::DamageName(Row.Source), 17)]
            + SHorizontalBox::Slot().AutoWidth()[Text(Number(Row.Damage), 18)]];
        Entry->AddSlot().AutoHeight().Padding(0, 7)[SNew(SBox).HeightOverride(5)[SNew(SProgressBar).Style(DamageBar()).Percent(Share).FillColorAndOpacity(FLinearColor::White)]];
        Entry->AddSlot().AutoHeight()[Text(FString::Printf(TEXT("%.1f%% of damage  ·  %.1f DPS  ·  %d hits  ·  %d kills"), Share * 100, Seconds > 0 ? Row.Damage / Seconds : 0., Row.Hits, Row.Kills), 13, BloodshiftMenu::Muted)];
        Box->AddSlot().AutoHeight().Padding(0, 12)[Entry];
    }
    if (Rows.IsEmpty()) Box->AddSlot().AutoHeight().Padding(0, 20)[Text(TEXT("No damage yet"), 17, BloodshiftMenu::Muted)];
    Box->AddSlot().AutoHeight().Padding(0, 26, 0, 10)[Text(TEXT("SUPPORT & UTILITY"), 16, BloodshiftMenu::Gold)];
    if (Report.IsValid()) for (const auto& Pair : SortedBuild(Report->Build()))
    {
        auto* Def = Report->Build().Definitions.FindRef(Pair.Key).Get();
        if (!Def || Def->Role == EUpgradeRole::Starter) continue;
        const bool bMeasured = Rows.ContainsByPredicate([&](const auto& Row) { return Row.Source == Pair.Key; });
        if (bMeasured) continue;
        Box->AddSlot().AutoHeight().Padding(0, 4)[Text(FString::Printf(TEXT("%s  ·  Rank %d  ·  %s"), *Def->DisplayName.ToString(), Pair.Value,
            Def->Role == EUpgradeRole::Stat || Def->Role == EUpgradeRole::Support ? TEXT("Support") : TEXT("Support")), 14, BloodshiftMenu::Muted)];
    }
    return Box;
}
TSharedRef<SWidget> SRunReportView::ScorePage()
{
    auto Box = SNew(SVerticalBox);
    const FRunScoreEntry Run = Report.IsValid() ? Report->Summary() : FRunScoreEntry();
    Heading(Box, bResults ? TEXT("YOUR FINAL SCORE") : TEXT("SCORE SO FAR"), TEXT(""));
    Stat(Box, TEXT("TOTAL SCORE"), Number(Run.CalculateScore()), true);
    Stat(Box, TEXT("Survival · 10 / second (first 30 minutes)"), Number(Run.SurvivalPoints()));
    Stat(Box, FString::Printf(TEXT("Common enemies · %d × 10"), Run.NormalKills), Number(double(Run.NormalKills) * 10));
    Stat(Box, FString::Printf(TEXT("Elites · %d × 250"), Run.EliteKills), Number(double(Run.EliteKills) * 250));
    Stat(Box, FString::Printf(TEXT("Bosses · %d × 5,000"), Run.BossKills), Number(double(Run.BossKills) * 5000));
    Stat(Box, FString::Printf(TEXT("Objectives · %d × 500"), Run.Objectives), Number(double(Run.Objectives) * 500));
    Stat(Box, TEXT("Victory bonus"), Run.bVictory ? TEXT("5,000") : TEXT("0"));
    Stat(Box, TEXT("Base score"), Number(Run.BasePoints()));
    Stat(Box, FString::Printf(TEXT("Map modifiers · %d ranks (+8%% each)"), Run.ModifierRanks), FString::Printf(TEXT("× %.2f"), Run.Multiplier()));
    if (Report.IsValid())
    {
        FString Active;
        for (const auto& Modifier : RunChallenges::Modifiers())
            if (const int32 Rank = Report->Modifiers().FindRef(Modifier.Id); Rank > 0)
                Active += FString::Printf(TEXT("%s%s %d"), Active.IsEmpty() ? TEXT("") : TEXT("  ·  "), *Modifier.Name, Rank);
        if (!Active.IsEmpty()) Box->AddSlot().AutoHeight().Padding(0, 10)[Text(Active, 13, BloodshiftMenu::Muted)];
    }
    if (Report.IsValid() && !Report->IsRanked())
        Box->AddSlot().AutoHeight().Padding(0,12)[Text(TEXT("UNRANKED"),14,BloodshiftMenu::Muted)];
    return Box;
}
void SRunLeaderboardView::Construct(const FArguments& Args)
{
    Boards = Args._Leaderboards;
    if (Boards.IsValid()) SteamBoards = Boards->GetGameInstance()->GetSubsystem<USteamLeaderboardSubsystem>();
    if (SteamBoards.IsValid())
    {
        SteamBoards->OnBoardsChanged.AddSP(this, &SRunLeaderboardView::Refresh);
        SteamBoards->RequestRefresh(RunReports::Maps()[MapIndex].Id);
    }
    auto Tabs = SNew(SHorizontalBox);
    for (int32 I = 0; I < RunReports::Maps().Num(); ++I)
        Tabs->AddSlot().FillWidth(1).Padding(3, 0)[SNew(SBorder).Padding(1)
            .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
            .BorderBackgroundColor_Lambda([this, I] { return MapIndex == I ? BloodshiftMenu::Gold : FLinearColor::Transparent; })
            [SNew(SButton).ButtonStyle(&Action())
            .ToolTipText(FText::FromString(RunReports::Maps()[I].Title))
            .OnClicked_Lambda([this, I] { SelectMap(I); return FReply::Handled(); })
            [SNew(STextBlock).Text(FText::FromString(RunReports::Maps()[I].ShortTitle)).Font(BloodshiftMenu::BodyFont(12))
                .ColorAndOpacity(FSlateColor::UseForeground())]]];
    ChildSlot[SNew(SOverlay)
        + SOverlay::Slot()[SNew(BloodshiftMenu::SPanelSurface)]
        + SOverlay::Slot().Padding(24)[SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)[Text(TEXT("LEADERBOARD"), 23)]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 16)[Tabs]
            + SVerticalBox::Slot().AutoHeight()[SAssignNew(Rows, SVerticalBox)]]];
    Refresh();
}
void SRunLeaderboardView::SelectMap(int32 Index)
{
    MapIndex = FMath::Clamp(Index, 0, RunReports::Maps().Num() - 1);
    if (SteamBoards.IsValid()) SteamBoards->RequestRefresh(RunReports::Maps()[MapIndex].Id);
    Refresh();
}
void SRunLeaderboardView::Tick(const FGeometry& Geometry, double Time, float Delta)
{
    SCompoundWidget::Tick(Geometry, Time, Delta);
    if (Time - LastRequest > 30)
    {
        LastRequest = Time;
        if (SteamBoards.IsValid()) SteamBoards->RequestRefresh(RunReports::Maps()[MapIndex].Id);
    }
}
void SRunLeaderboardView::Refresh()
{
    if (!Rows) return;
    Rows->ClearChildren();
    const auto& Map = RunReports::Maps()[MapIndex];
    Rows->AddSlot().AutoHeight().Padding(0, 0, 0, 10)[Text(Map.Title, 14, BloodshiftMenu::Gold)];
    const auto Entries = Boards.IsValid() ? Boards->TopTen(Map.Id) : TArray<FRunScoreEntry>();
    const auto* Global = SteamBoards.IsValid() && SteamBoards->IsOnline() ? SteamBoards->Board(Map.Id) : nullptr;
    const bool bGlobal = Global && Global->bReady;
    for (int32 I = 0; I < 10; ++I)
    {
        const bool bPresent = bGlobal ? Global->Rows.IsValidIndex(I) : Entries.IsValidIndex(I);
        const FString Name = bPresent ? (bGlobal ? Global->Rows[I].Name : TEXT("YOU")) : TEXT("\u2014");
        const int32 Score = bPresent ? (bGlobal ? Global->Rows[I].Score : Entries[I].Score) : 0;
        const int32 Rank = bPresent && bGlobal ? Global->Rows[I].Rank : I + 1;
        Rows->AddSlot().AutoHeight().Padding(0, 3)[SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(28)[Text(FString::Printf(TEXT("%02d"), Rank), 14, BloodshiftMenu::Muted)]]
            + SHorizontalBox::Slot().FillWidth(1).Padding(10, 0)[SNew(STextBlock).Text(FText::FromString(Name))
                .ToolTipText(FText::FromString(Name)).Font(BloodshiftMenu::BodyFont(13)).OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                .ColorAndOpacity(bPresent && bGlobal && Global->Rows[I].bSelf ? BloodshiftMenu::Gold : BloodshiftMenu::Paper)]
            + SHorizontalBox::Slot().AutoWidth()[Text(bPresent ? Number(Score) : TEXT("\u2014"), 16, bPresent ? BloodshiftMenu::Paper : BloodshiftMenu::Muted)]];
    }
    Rows->AddSlot().AutoHeight().Padding(0, 16, 0, 8)[SNew(SBox).HeightOverride(1)[SNew(BloodshiftMenu::SOrnamentDivider)]];
    const int32 Best = bGlobal ? Global->PersonalBest : (Entries.IsEmpty() ? 0 : Entries[0].Score);
    Rows->AddSlot().AutoHeight()[Text(FString::Printf(TEXT("YOUR BEST  %s"), Best > 0 ? *Number(Best) : TEXT("\u2014")), 18, BloodshiftMenu::Gold)];
    if (bGlobal && Global->PersonalRank > 0)
        Rows->AddSlot().AutoHeight().Padding(0,4)[Text(FString::Printf(TEXT("RANK  %s"), *Number(Global->PersonalRank)), 12, BloodshiftMenu::Muted)];
    if (bGlobal ? Global->Rows.IsEmpty() : Entries.IsEmpty()) Rows->AddSlot().AutoHeight().Padding(0,10,0,0)[Text(MapIndex==0?TEXT("No scores yet"):TEXT("Coming soon"),12,BloodshiftMenu::Muted)];
}
