#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "RunReportSubsystem.h"

class SBox;
class SVerticalBox;
class USteamLeaderboardSubsystem;
/** Shared pause/results report. Each page has its own scroll region; navigation stays visible. */
class SRunReportView : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SRunReportView) : _Results(false) {}
        SLATE_ARGUMENT(URunReportSubsystem*, Report)
        SLATE_ARGUMENT(ASurvivorPlayerController*, Controller)
        SLATE_ARGUMENT(bool, Results)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    void SelectTab(int32 Index);
    int32 SelectedTab() const { return Tab; }
private:
    TSharedRef<SWidget> Overview();
    TSharedRef<SWidget> BuildPage();
    TSharedRef<SWidget> DamagePage();
    TSharedRef<SWidget> ScorePage();
    TWeakObjectPtr<URunReportSubsystem> Report;
    TWeakObjectPtr<ASurvivorPlayerController> Controller;
    TSharedPtr<SBox> Page;
    TArray<TSharedPtr<FSlateBrush>> IconBrushes;
    int32 Tab = 0;
    bool bResults = false;
};

class SRunLeaderboardView : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SRunLeaderboardView) {}
        SLATE_ARGUMENT(URunLeaderboardSubsystem*, Leaderboards)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    void Refresh();
    void SelectMap(int32 Index);
    virtual void Tick(const FGeometry& Geometry, double Time, float Delta) override;
private:
    TWeakObjectPtr<URunLeaderboardSubsystem> Boards;
    TWeakObjectPtr<USteamLeaderboardSubsystem> SteamBoards;
    TSharedPtr<SVerticalBox> Rows;
    int32 MapIndex = 0;
    double LastRequest = 0;
};
