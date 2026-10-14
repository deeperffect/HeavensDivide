#include "RunReportSubsystem.h"
#include "ComboAbilityDefinition.h"
#include "SteamLeaderboardSubsystem.h"
#include "RunChallengeSubsystem.h"
#include "SynergyMetaProgressionSubsystem.h"
#include "TesterBalanceSettings.h"
#include "SurvivorPlayerController.h"
#include "EnemyBase.h"
#include "FinalBossBase.h"
#include "CinderOracleBoss.h"
#include "HealingUrn.h"
#include "EncounterWall.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
    // Combat runs on the game thread. No actor or UObject is retained by the attribution stack.
    thread_local FName CurrentSource;
    thread_local bool bCurrentPlayerDamage = false;
    thread_local FName CurrentSecondarySource;
    thread_local float CurrentSecondaryFraction = 0.f;
    bool IsCombatVictim(const AEnemyBase* Enemy)
    {
        return IsValid(Enemy) && !Enemy->IsStressTestEnemy() && !Enemy->IsA<AHealingUrn>()
            && !Enemy->IsA<AEncounterWall>() && !Enemy->GetClass()->GetName().Contains(TEXT("TestDummy"));
    }
}
const TArray<FRunMapInfo>& RunReports::Maps()
{
    static const TArray<FRunMapInfo> Values = {
        {TEXT("FallenGrove"), TEXT("THE FALLEN GROVE"), TEXT("GROVE"), TEXT("bloodshift_fallen_grove_score_v1")},
        {TEXT("CinderCitadel"), TEXT("CINDER CITADEL"), TEXT("CITADEL"), TEXT("bloodshift_cinder_citadel_score_v1")},
        {TEXT("HollowMarsh"), TEXT("HOLLOW MARSH"), TEXT("MARSH"), TEXT("bloodshift_hollow_marsh_score_v1")}
    };
    return Values;
}
const FRunMapInfo* RunReports::FindMap(FName Id)
{
    return Maps().FindByPredicate([Id](const auto& Map) { return Map.Id == Id; });
}
FString RunReports::DamageName(FName Id)
{
    if (const auto* Entry = ComboCatalog::Find(Id)) return Entry->Name;
    static const TMap<FName, FString> Names = {
        {TEXT("SamuraiAttack"), TEXT("Samurai · sword strikes")}, {TEXT("NinjaAttack"), TEXT("Ninja · kunai")},
        {TEXT("Barrage"), TEXT("Barrage · kunai volleys")}, {TEXT("Iaijutsu"), TEXT("Iaijutsu · dash slash")},
        {TEXT("Crescent"), TEXT("Crescent · blade waves")}, {TEXT("LingeringWake"), TEXT("Lingering Wake · field")},
        {TEXT("ReturningFang"), TEXT("Returning Fang")}, {TEXT("GreatShuriken"), TEXT("Great Shuriken")},
        {TEXT("EmbeddedBlades"), TEXT("Embedded Blades · death scatter")}, {TEXT("Bleed"), TEXT("Blood · bleed ticks")},
        {TEXT("Poison"), TEXT("Poison · ticks")}, {TEXT("BarragePoison"), TEXT("Barrage · poison ticks")},
        {TEXT("BarragePool"), TEXT("Poison pools · poison ticks")}, {TEXT("BloodDetonation"), TEXT("Blood Detonation")},
        {TEXT("ShadowClone"), TEXT("Shadow Clone")}, {TEXT("InactiveCharacterAssist"), TEXT("Inactive character assist")},
        {TEXT("TagTeam"), TEXT("Tag Team")}, {TEXT("ComboAbility"), TEXT("Combo ability")},
        {TEXT("BloodEcho"), TEXT("Blood Echo")}, {TEXT("DoubleCut"), TEXT("Double Cut")}, {TEXT("BarrageBloom"), TEXT("Poison Bloom")}, {TEXT("SplinterWave"), TEXT("Splinter Wave")}
    };
    if (const auto* Name = Names.Find(Id)) return *Name;
    return FName::NameToDisplayString(Id.ToString(), false);
}
void RunReports::ObjectiveCompleted(AActor* Objective)
{
    if (auto* GI = Objective ? Objective->GetGameInstance() : nullptr)
        if (auto* Report = GI->GetSubsystem<URunReportSubsystem>()) Report->RecordObjective(Objective);
}
FRunDamageScope::FRunDamageScope(FName Source, bool bPlayerDamage, bool bOnlyIfUnset, FName SecondarySource, float SecondaryFraction)
    : PreviousSource(CurrentSource), bPreviousPlayerDamage(bCurrentPlayerDamage),
      PreviousSecondarySource(CurrentSecondarySource), PreviousSecondaryFraction(CurrentSecondaryFraction)
{
    if (!bOnlyIfUnset || CurrentSource.IsNone())
    {
        CurrentSource = Source; CurrentSecondarySource = SecondarySource;
        CurrentSecondaryFraction = SecondarySource.IsNone() || !FMath::IsFinite(SecondaryFraction) ? 0.f : FMath::Clamp(SecondaryFraction, 0.f, 1.f);
    }
    bCurrentPlayerDamage |= bPlayerDamage;
}
FRunDamageScope::~FRunDamageScope()
{
    CurrentSource = PreviousSource; bCurrentPlayerDamage = bPreviousPlayerDamage;
    CurrentSecondarySource = PreviousSecondarySource; CurrentSecondaryFraction = PreviousSecondaryFraction;
}
void FRunDamageScope::Record(AActor* Victim, float ActualDamage, bool bKilled)
{
    if (!bCurrentPlayerDamage) return;
    if (auto* Enemy = Cast<AEnemyBase>(Victim))
        if (auto* GI = Enemy->GetGameInstance())
            if (auto* Report = GI->GetSubsystem<URunReportSubsystem>())
            {
                const float SecondaryDamage = ActualDamage * CurrentSecondaryFraction;
                Report->RecordDamage(Enemy, CurrentSource, ActualDamage - SecondaryDamage, bKilled && CurrentSecondaryFraction < .5f);
                if (SecondaryDamage > 0.f)
                    Report->RecordDamage(Enemy, CurrentSecondarySource, SecondaryDamage, bKilled && CurrentSecondaryFraction >= .5f);
            }
}
int32 FRunScoreEntry::SurvivalPoints() const
{
    return FMath::IsFinite(Seconds) ? FMath::FloorToInt(FMath::Clamp(Seconds, 0., 1800.)) * 10 : 0;
}
int32 FRunScoreEntry::BasePoints() const
{
    const int64 Base = SurvivalPoints() + int64(FMath::Max(0, NormalKills)) * 10
        + int64(FMath::Max(0, EliteKills)) * 250 + int64(FMath::Max(0, BossKills)) * 5000
        + int64(FMath::Max(0, Objectives)) * 500 + (bVictory ? 5000 : 0);
    return int32(FMath::Min<int64>(Base, MAX_int32));
}
int32 FRunScoreEntry::CalculateScore() const
{
    // Integer hundredths avoid platform-dependent float rounding on Steam scores.
    const int64 Scaled = int64(BasePoints()) * (100 + 8 * FMath::Clamp(ModifierRanks, 0, 25));
    return int32(FMath::Min<int64>((Scaled + 50) / 100, MAX_int32));
}
FString URunLeaderboardSubsystem::SaveSlot() const
{
    return SlotName;
}
void URunLeaderboardSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    FParse::Value(FCommandLine::Get(), TEXT("LeaderboardSaveSlot="), SlotName);
    bReadOnly = FParse::Param(FCommandLine::Get(), TEXT("RunReportPreview"));
    if (UGameplayStatics::DoesSaveGameExist(SaveSlot(), 0))
    {
        auto* Saved = Cast<URunLeaderboardSave>(UGameplayStatics::LoadGameFromSlot(SaveSlot(), 0));
        if (Saved && Saved->Version == 1) Entries = Saved->Entries;
        else bReadOnly = true; // Preserve an unreadable or newer save.
    }
    RetryHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float)
    {
        if (bPendingSave) Save();
        return true;
    }), 5.f);
}
void URunLeaderboardSubsystem::Deinitialize()
{
    FTSTicker::GetCoreTicker().RemoveTicker(RetryHandle);
    if (bPendingSave) Save();
    Super::Deinitialize();
}
TArray<FRunScoreEntry> URunLeaderboardSubsystem::TopTen(FName MapId) const
{
    TArray<FRunScoreEntry> Result;
    for (const auto& Entry : Entries)
        if (Entry.MapId == MapId && Entry.ScoreVersion == 1 && Entry.Score == Entry.CalculateScore()) Result.Add(Entry);
    Result.Sort([](const auto& A, const auto& B)
    {
        if (A.Score != B.Score) return A.Score > B.Score;
        if (A.Date != B.Date) return A.Date < B.Date;
        return A.RunId.ToString() < B.RunId.ToString();
    });
    if (Result.Num() > 10) Result.SetNum(10);
    return Result;
}
int32 URunLeaderboardSubsystem::PersonalBest(FName MapId) const
{
    const auto Board = TopTen(MapId);
    return Board.IsEmpty() ? 0 : Board[0].Score;
}
bool URunLeaderboardSubsystem::Submit(const FRunScoreEntry& Entry)
{
    if (!RunReports::FindMap(Entry.MapId) || !Entry.RunId.IsValid() || Entry.ScoreVersion != 1
        || !FMath::IsFinite(Entry.Seconds) || Entry.Seconds < 0 || Entry.NormalKills < 0 || Entry.EliteKills < 0
        || Entry.BossKills < 0 || Entry.Objectives < 0 || Entry.ModifierRanks < 0 || Entry.ModifierRanks > 25
        || Entry.Score < 0 || Entry.Score != Entry.CalculateScore()
        || Entries.ContainsByPredicate([&](const auto& E) { return E.RunId == Entry.RunId; })) return false;
    const int32 PreviousBest = PersonalBest(Entry.MapId);
    const auto ExistingBoard = TopTen(Entry.MapId);
    if (ExistingBoard.Num() == 10 && Entry.Score < ExistingBoard.Last().Score) return false;
    Entries.Add(Entry);
    TArray<FRunScoreEntry> Trimmed;
    for (const auto& Map : RunReports::Maps()) Trimmed.Append(TopTen(Map.Id));
    Entries = MoveTemp(Trimmed);
    bPendingSave = !bReadOnly;
    if (bPendingSave) Save();
    if (Entry.Score > PreviousBest && !bReadOnly) OnBestScoreCommitted.Broadcast(Entry);
    return true;
}
bool URunLeaderboardSubsystem::Save()
{
    auto* Saved = NewObject<URunLeaderboardSave>();
    Saved->Entries = Entries;
    bPendingSave = !UGameplayStatics::SaveGameToSlot(Saved, SaveSlot(), 0);
    return !bPendingSave;
}
void URunReportSubsystem::BeginRun(ASurvivorPlayerController* Controller)
{
    Run = {}; Run.RunId = FGuid::NewGuid(); Run.Date = FDateTime::UtcNow();
    Run.MapId = TEXT("FallenGrove");
    RunBuild = {}; Damage.Reset(); ModifierSnapshot.Reset(); CountedKills.Reset(); CountedObjectives.Reset();
    bActive = true;
    const auto* World = Controller ? Controller->GetWorld() : nullptr;
    bRanked = World && World->GetMapName().EndsWith(TEXT("Lvl_B1_Lvl1")) && RunChallenges::CanEarnCurrencies(World);
    bDifficultyEligible=bRanked;
    RankReason = bRanked ? FString() : TEXT("Practice or unsupported map");
    if (GetDefault<UTesterBalanceSettings>()->bEnabled)
    {
        bRanked = false; RankReason = TEXT("Tester balance overrides enabled");
    }
    if (auto* Challenges = GetGameInstance()->GetSubsystem<URunChallengeSubsystem>())
        for (const auto& Modifier : RunChallenges::Modifiers())
        {
            const int32 Rank = Challenges->GetModifierRank(Modifier.Id);
            ModifierSnapshot.Add(Modifier.Id, Rank); Run.ModifierRanks += Rank;
        }
}
void URunReportSubsystem::AdvanceTime(float DeltaSeconds)
{
    if (bActive && FMath::IsFinite(DeltaSeconds) && DeltaSeconds > 0) Run.Seconds += DeltaSeconds;
}
void URunReportSubsystem::RecordDamage(AEnemyBase* Enemy, FName Source, float ActualDamage, bool bKilled)
{
    if (!bActive || !IsCombatVictim(Enemy) || !FMath::IsFinite(ActualDamage) || ActualDamage <= 0) return;
    auto* Row = Damage.FindByPredicate([Source](const auto& D) { return D.Source == Source; });
    if (!Row) { Row = &Damage.AddDefaulted_GetRef(); Row->Source = Source; }
    Row->Damage += ActualDamage; ++Row->Hits;
    if (bKilled && !CountedKills.Contains(Enemy))
    {
        CountedKills.Add(Enemy); ++Row->Kills;
        // Repeated boss summons contribute damage, but cannot farm leaderboard kill points.
        if (Enemy->GetOwner() && Enemy->GetOwner()->IsA<ACinderOracleBoss>()) return;
        if (Enemy->IsA<AFinalBossBase>() || Enemy->GetDropCategory() == EEnemyDropCategory::Boss) ++Run.BossKills;
        else if (Enemy->GetDropCategory() == EEnemyDropCategory::Elite) ++Run.EliteKills;
        else ++Run.NormalKills;
    }
}
void URunReportSubsystem::RecordObjective(AActor* Objective)
{
    if (bActive && IsValid(Objective) && !CountedObjectives.Contains(Objective))
    { CountedObjectives.Add(Objective); ++Run.Objectives; }
}
void URunReportSubsystem::CaptureBuild(UPlayerUpgradeComponent* Upgrades)
{
    if (bActive && Upgrades) Upgrades->CaptureRunState(RunBuild);
}
void URunReportSubsystem::Finish(ASurvivorPlayerController* Controller, bool bVictory)
{
    if (!bActive) return;
    CaptureBuild(Controller ? Controller->GetPlayerUpgrades() : nullptr);
    Run.bVictory = bVictory; Run.Score = Run.CalculateScore(); bActive = false;
    if(bDifficultyEligible && bVictory)if(auto* Meta=GetGameInstance()->GetSubsystem<USynergyMetaProgressionSubsystem>())Meta->RecordDifficultyVictory(Run.MapId,ModifierSnapshot);
    if (bRanked)
    {
        if (auto* Boards = GetGameInstance()->GetSubsystem<URunLeaderboardSubsystem>()) Boards->Submit(Run);
        // Steam bests belong to accounts, while the legacy local board belongs to this device.
        if (auto* Steam = GetGameInstance()->GetSubsystem<USteamLeaderboardSubsystem>()) Steam->SubmitRun(Run);
    }
}
