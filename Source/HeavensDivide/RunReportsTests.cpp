#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "RunReportSubsystem.h"
#include "RunReportView.h"
#include "RunChallengeSubsystem.h"
#include "SynergyMetaProgressionSubsystem.h"
#include "SurvivorPlayerController.h"
#include "EnemyBase.h"
#include "CinderOracleBoss.h"
#include "HealingUrn.h"
#include "HealthComponent.h"
#include "PauseMenuWidget.h"
#include "GameOverWidget.h"
#include "VictoryWidget.h"
#include "MainMenuWidget.h"
#include "UpgradeDefinition.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/Button.h"
#include "Components/NativeWidgetHost.h"
#include "Blueprint/WidgetTree.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/SWidget.h"
#include "Widgets/Layout/SBox.h"
#include "Layout/ArrangedChildren.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#if WITH_EDITOR
#include "AssetCompilingManager.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunReportsTest, "HeavensDivide.Progression.RunReports",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)
bool FRunReportsTest::RunTest(const FString&)
{
    const FString Slot = TEXT("Automation_RunReports_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString OldCommandLine = FCommandLine::Get();
    FCommandLine::Set(*(TEXT("-LeaderboardSaveSlot=") + Slot + TEXT(" -MetaSaveSlot=") + Slot + TEXT(" ") + OldCommandLine));
    auto* GI = NewObject<UGameInstance>(GEngine); GI->InitializeStandalone();
    FCommandLine::Set(*OldCommandLine);
    auto* World = GI->GetWorld(); World->InitializeActorsForPlay(FURL());
    auto* Report = GI->GetSubsystem<URunReportSubsystem>();
    auto* Boards = GI->GetSubsystem<URunLeaderboardSubsystem>();
    auto* Challenges = GI->GetSubsystem<URunChallengeSubsystem>();
    for(int32 Rank=0;Rank<2;++Rank)
    {TMap<FName,int32> Completed;for(const auto& M:RunChallenges::Modifiers())Completed.Add(M.Id,Rank);GI->GetSubsystem<USynergyMetaProgressionSubsystem>()->RecordDifficultyVictory(TEXT("FallenGrove"),Completed);}
    Challenges->SetModifierRank(TEXT("Health"), 2); Challenges->SetModifierRank(TEXT("Speed"), 1);
    auto* ControllerClass = LoadClass<ASurvivorPlayerController>(nullptr, TEXT("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController.BP_SurvivorPlayerController_C"));
    TestNotNull(TEXT("Shipped controller and upgrade pool load"), ControllerClass);
    auto* PC = World->SpawnActor<ASurvivorPlayerController>(ControllerClass ? ControllerClass : ASurvivorPlayerController::StaticClass()); PC->SetPlayer(NewObject<ULocalPlayer>(GEngine));
    Report->BeginRun(PC);
    TestEqual(TEXT("Modifier snapshot totals three ranks"), Report->Summary().ModifierRanks, 3);
    TestFalse(TEXT("A test map never submits ranked runs"), Report->IsRanked());
    Report->AdvanceTime(125.7f); Report->AdvanceTime(-10); Report->AdvanceTime(NAN);
    TestTrue(TEXT("Invalid time cannot change survival duration"), FMath::IsNearlyEqual(Report->Summary().Seconds, 125.7, .001));
    auto Enemy = [&]()
    {
        FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* E = World->SpawnActor<AEnemyBase>(AEnemyBase::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
        E->DispatchBeginPlay(); E->GetHealthComponent()->SetMaxHealthPreservePercent(100); E->GetHealthComponent()->RestoreCurrentHealth(100);
        return E;
    };
    auto* A = Enemy();
    A->GetHealthComponent()->SetDamageEnabled(false); A->ApplyPlayerDamage(100, EPlayerAttackSource::Ninja);
    TestEqual(TEXT("Blocked hits produce no analytics row"), Report->DamageRows().Num(), 0);
    A->GetHealthComponent()->SetDamageEnabled(true);
    {
        FRunDamageScope Outer(TEXT("ReturningFang"));
        A->ApplyPlayerDamage(25, EPlayerAttackSource::Ninja);
        { FRunDamageScope Inner(TEXT("Bleed")); A->ApplyStatusDamage(20, EPlayerAttackSource::Samurai); }
        A->ApplyPlayerDamage(1000, EPlayerAttackSource::Ninja);
    }
    TestEqual(TEXT("Nested scope restores projectile source"), Report->DamageRows().Num(), 2);
    if (Report->DamageRows().Num() == 2)
    {
        TestEqual(TEXT("Overkill counts only health removed"), Report->DamageRows()[0].Damage, 80.);
        TestEqual(TEXT("Delayed effect has separate damage"), Report->DamageRows()[1].Damage, 20.);
    }
    TestEqual(TEXT("A lethal hit counts once"), Report->Summary().NormalKills, 1);
    A->ApplyPlayerDamage(50, EPlayerAttackSource::Ninja);
    TestEqual(TEXT("Dead enemies cannot inflate kill totals"), Report->Summary().NormalKills, 1);
    auto* B = Enemy(); B->GetHealthComponent()->ApplyDamage(1000);
    TestEqual(TEXT("Enemy self-damage is not credited to player"), Report->Summary().NormalKills, 1);
    auto* Mixed = Enemy();
    {
        FRunDamageScope Mix(TEXT("BarragePoison"), false, false, TEXT("BarragePool"), .25f);
        Mixed->ApplyStatusDamage(1000, EPlayerAttackSource::Ninja);
    }
    const auto* PoolRow = Report->DamageRows().FindByPredicate([](const auto& Row) { return Row.Source == TEXT("BarragePool"); });
    if (TestNotNull(TEXT("Mixed poison tracks additive pool contribution"), PoolRow))
        TestEqual(TEXT("Overkill-clamped damage keeps exact pool fraction"), PoolRow->Damage, 25.);
    TestEqual(TEXT("Mixed tick credits exactly one killing blow"), Report->Summary().NormalKills, 2);
    auto* Summoner = World->SpawnActor<ACinderOracleBoss>();
    auto* Summon = Enemy(); Summon->SetOwner(Summoner); Summon->ApplyPlayerDamage(1000, EPlayerAttackSource::Ninja);
    TestEqual(TEXT("Boss summon damage recorded without farming score kills"), Report->Summary().NormalKills, 2);
    Report->RecordObjective(PC); Report->RecordObjective(PC);
    TestEqual(TEXT("Objective completion counts once"), Report->Summary().Objectives, 1);
    FRunScoreEntry Score; Score.RunId = FGuid::NewGuid(); Score.Date = FDateTime::UtcNow(); Score.MapId = TEXT("FallenGrove");
    Score.Seconds = 125.7; Score.NormalKills = 100; Score.EliteKills = 2; Score.BossKills = 1; Score.Objectives = 3;
    Score.ModifierRanks = 3; Score.bVictory = true; Score.Score = Score.CalculateScore();
    TestEqual(TEXT("Exact component base score"), Score.BasePoints(), 14250);
    TestEqual(TEXT("Three modifier ranks give exact 24 percent bonus"), Score.Score, 17670);
    auto LongRun = Score; LongRun.Seconds = 100000;
    TestEqual(TEXT("Survival points stop at thirty minutes"), LongRun.SurvivalPoints(), 18000);
    int32 BestEvents = 0; Boards->OnBestScoreCommitted.AddLambda([&](const auto&) { ++BestEvents; });
    TestTrue(TEXT("Valid score submitted"), Boards->Submit(Score));
    TestFalse(TEXT("Duplicate run rejected"), Boards->Submit(Score));
    for (int32 I = 0; I < 12; ++I)
    {
        auto Entry = Score; Entry.RunId = FGuid::NewGuid(); Entry.NormalKills += I + 1; Entry.Score = Entry.CalculateScore();
        TestTrue(TEXT("Another valid run accepted"), Boards->Submit(Entry));
    }
    TestEqual(TEXT("Per-map board keeps precisely ten best runs"), Boards->TopTen(TEXT("FallenGrove")).Num(), 10);
    TestEqual(TEXT("Other maps stay independent"), Boards->TopTen(TEXT("CinderCitadel")).Num(), 0);
    TestEqual(TEXT("Personal best uses maximum score"), Boards->PersonalBest(TEXT("FallenGrove")), 17819);
    TestFalse(TEXT("An evicted lower-score run cannot re-enter the board"), Boards->Submit(Score));
    auto Forged = Score; Forged.RunId = FGuid::NewGuid(); ++Forged.Score;
    TestFalse(TEXT("Inconsistent totals rejected"), Boards->Submit(Forged));
    auto* Saved = Cast<URunLeaderboardSave>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
    if (TestNotNull(TEXT("Board persisted to the isolated save slot"), Saved)) TestEqual(TEXT("All top entries survive save serialization"), Saved->Entries.Num(), 10);
    TestEqual(TEXT("Steam handoff fires only for new best scores"), BestEvents, 13);
    Boards->OnBestScoreCommitted.Clear();
    Report->Finish(PC, false);
    const auto Finished = Report->Summary(); Report->AdvanceTime(10); Report->Finish(PC, true);
    TestEqual(TEXT("Final report cannot advance or change outcome"), Report->Summary().Score, Finished.Score);
    TestFalse(TEXT("Finishing twice does not turn defeat into victory"), Report->Summary().bVictory);

    auto* Meta=GI->GetSubsystem<USynergyMetaProgressionSubsystem>();
    const int32 BeforeUnlock=Meta->GetUnlockedDifficultyRank(TEXT("FallenGrove"),TEXT("Health"));
    Report->BeginRun(PC);Report->bDifficultyEligible=true;Report->Finish(PC,false);
    TestEqual(TEXT("Defeat does not unlock difficulty"),Meta->GetUnlockedDifficultyRank(TEXT("FallenGrove"),TEXT("Health")),BeforeUnlock);
    Report->BeginRun(PC);Report->bDifficultyEligible=true;Report->Finish(PC,true);
    TestEqual(TEXT("Completed victory advances only the played health rank"),Meta->GetUnlockedDifficultyRank(TEXT("FallenGrove"),TEXT("Health")),3);
    Report->Finish(PC,true);
    TestEqual(TEXT("Duplicate victory notification cannot advance twice"),Meta->GetUnlockedDifficultyRank(TEXT("FallenGrove"),TEXT("Health")),3);

    // Use real saved card assets and shipped widgets for visual review, without changing player saves.
    Report->Run = Score; Report->bRanked = true; Report->RunBuild = {};
    const FName CardIds[] = {TEXT("BattleStance"), TEXT("BloodDetonation"), TEXT("BloodRush"), TEXT("DoubleCut"), TEXT("BloodDamage"),
        TEXT("BarrageStance"), TEXT("BarrageBloom"), TEXT("BarrageDamage"), TEXT("BarragePoison"), TEXT("TagTeam"), TEXT("MoveSpeed"), TEXT("MaxHealth")};
    for (FName Id : CardIds) if (auto* Def = PC->GetPlayerUpgrades()->FindUpgradeDefinition(Id))
    {
        Report->RunBuild.Definitions.Add(Id, Def); Report->RunBuild.Levels.Add(Id, Def->MaxLevel > 1 ? 3 : 1);
    }
    Report->RunBuild.SamuraiMastery = 9; Report->RunBuild.NinjaMastery = 7;
    Report->Damage = {{TEXT("SamuraiAttack"), 218430., 820, 270}, {TEXT("Barrage"), 184720., 1740, 190},
        {TEXT("Bleed"), 142800., 3160, 184}, {TEXT("BloodDetonation"), 67120., 120, 91}, {TEXT("BarragePoison"), 38000., 830, 40}, {TEXT("BarrageBloom"), 9100., 46, 18}, {TEXT("TagTeam"), 4400., 22, 3}};
#if WITH_EDITOR
    FAssetCompilingManager::Get().FinishAllCompilation();
#endif
    const FString Dir = FPaths::ProjectSavedDir() / TEXT("RunReports/Previews"); IFileManager::Get().MakeDirectory(*Dir, true);
    auto Render = [&](TSharedRef<SWidget> Slate, FVector2D Size, const FString& Name)
    {
        FWidgetRenderer Renderer(false); auto* Target = Renderer.DrawWidget(Slate, Size);
        if (!TestNotNull(TEXT("UI render target"), Target)) return;
        for (int32 Pass = 0; Pass < 4; ++Pass) { ++GFrameCounter; Slate->Invalidate(EInvalidateWidgetReason::Prepass); Renderer.DrawWidget(Target, Slate, Size, .016f); }
        TArray<FColor> Pixels; FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
        if (TestTrue(TEXT("Rendered UI pixels readable"), Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags)))
        {
            TArray64<uint8> PNG; FImageUtils::PNGCompressImageArray(Target->SizeX, Target->SizeY, Pixels, PNG);
            TestTrue(TEXT("Review PNG saved"), FFileHelper::SaveArrayToFile(PNG, *(Dir / Name)));
        }
    };
    auto* DeathClass = LoadClass<UGameOverWidget>(nullptr, TEXT("/Game/HeavensDivide/Blueprints/UI/WBP_GameOver.WBP_GameOver_C"));
    TArray<UUserWidget*> Widgets = {CreateWidget<UPauseMenuWidget>(PC), CreateWidget<UGameOverWidget>(PC, DeathClass), CreateWidget<UVictoryWidget>(PC)};
    Cast<UGameOverWidget>(Widgets[1])->InitializeGameOver(PC, static_cast<float>(Score.Seconds));
    const TCHAR* Prefix[] = {TEXT("Pause"), TEXT("Defeat"), TEXT("Victory")};
    for (int32 I = 0; I < Widgets.Num(); ++I)
    {
        Report->Run.bVictory = I == 2; Report->Run.Score = Report->Run.CalculateScore();
        auto* Widget = Widgets[I]; if (!TestNotNull(TEXT("Run screen constructed"), Widget)) continue;
        auto Slate = Widget->TakeWidget();
        TestNotNull(TEXT("Run screen uses shared menu surface"), Widget->GetWidgetFromName(TEXT("RunPromptSurface")));
        auto* Host = Cast<UNativeWidgetHost>(Widget->GetWidgetFromName(TEXT("RunReportHost")));
        if (TestNotNull(TEXT("Report tabs available"), Host))
        {
            auto View = StaticCastSharedRef<SRunReportView>(Host->GetContent().ToSharedRef());
            for (int32 Tab = 0; Tab < 4; ++Tab)
            {
                View->SelectTab(Tab); TestEqual(TEXT("Each tab selects its page"), View->SelectedTab(), Tab);
                Render(Slate, {1600, 900}, FString::Printf(TEXT("%s_Tab%d_1600.png"), Prefix[I], Tab));
            }
            View->SelectTab(1); Render(Slate, {1280, 720}, FString::Printf(TEXT("%s_Build_1280.png"), Prefix[I]));
            Render(Slate, {960, 540}, FString::Printf(TEXT("%s_Build_960.png"), Prefix[I]));
        }
    }
    auto* MainClass = LoadClass<UMainMenuWidget>(nullptr, TEXT("/Game/HeavensDivide/Blueprints/UI/MainMenu/WBP_MainMenu.WBP_MainMenu_C"));
    auto* Menu = CreateWidget<UMainMenuWidget>(PC, MainClass); auto MenuSlate = Menu->TakeWidget();
    auto* BoardHost = Cast<UNativeWidgetHost>(Menu->GetWidgetFromName(TEXT("LeaderboardHost")));
    if (TestNotNull(TEXT("Main menu includes leaderboard"), BoardHost))
    {
        auto Board = StaticCastSharedRef<SRunLeaderboardView>(BoardHost->GetContent().ToSharedRef());
        for (int32 I = 0; I < 3; ++I) { Board->SelectMap(I); Render(MenuSlate, {1600, 900}, FString::Printf(TEXT("MainMenu_Map%d_1600.png"), I)); }
        Board->SelectMap(0); Render(MenuSlate, {1280, 720}, TEXT("MainMenu_1280.png"));
        Menu->ShowSettingsPanel(); Render(MenuSlate,{1600,900},TEXT("Settings_Audio_1600.png")); Render(MenuSlate,{960,540},TEXT("Settings_Audio_960.png"));
    }
    for (auto* Widget : Widgets) if (Widget) Widget->ReleaseSlateResources(true);
    Menu->ReleaseSlateResources(true);
    GI->Shutdown(); World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
    UGameplayStatics::DeleteGameInSlot(Slot, 0);
    return true;
}
#endif
