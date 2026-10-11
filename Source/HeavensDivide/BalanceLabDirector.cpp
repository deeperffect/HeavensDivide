#include "BalanceLabDirector.h"
#include "SurvivorPlayerController.h"
#include "PlayerUpgradeComponent.h"
#include "CharacterManagerComponent.h"
#include "CharacterBase.h"
#include "SwapPresentationComponent.h"
#include "SamuraiCharacter.h"
#include "NinjaCharacter.h"
#include "NinjaBuildComponent.h"
#include "EnemyBase.h"
#include "HealthComponent.h"
#include "ComboAbilityComponent.h"
#include "TesterBalanceSettings.h"
#include "AttackProjectileBase.h"
#include "NinjaBuildProjectile.h"
#include "SamuraiBladeWave.h"
#include "SamuraiIaijutsu.h"
#include "SamuraiWaveField.h"
#include "BarragePoisonPool.h"
#include "ShadowClone.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Components/SceneComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformMisc.h"

ABalanceLabDirector::ABalanceLabDirector()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = .1f;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}
ABalanceLabDirector* ABalanceLabDirector::Find(const UWorld* World)
{
    if (World) for (TActorIterator<ABalanceLabDirector> It(const_cast<UWorld*>(World)); It; ++It) return *It;
    return nullptr;
}
void ASurvivorPlayerController::BalanceLab(const FString& Action, const FString& Value, int32 Count)
{
#if !UE_BUILD_SHIPPING
    if (auto* Lab = ABalanceLabDirector::Find(GetWorld())) ClientMessage(Lab->Command(this, Action, Value, Count));
    else ClientMessage(TEXT("Open /Game/Maps/Lvl_BalanceLab first."));
#endif
}
void ABalanceLabDirector::Tick(float Delta)
{
    Super::Tick(Delta);
#if !UE_BUILD_SHIPPING
    if (!Controller.IsValid()) Controller = Cast<ASurvivorPlayerController>(GetWorld()->GetFirstPlayerController());
    auto* PC = Controller.Get();
    if (!PC || !PC->GetCharacterManager() || !PC->GetCharacterManager()->GetActiveCharacter()) return;
    if (!bInitialized)
    {
        bInitialized = true;
        PC->GetPlayerUpgrades()->BalancePreviewMetaProfile = 0;
        PC->GetPlayerHealthComponent()->SetDamageEnabled(false);
        SetBuild(TEXT("Blood"), 0);
        SpawnTargets(TEXT("Dummy"), 1);
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("BalanceLabSmoke")))
    {
        static const TCHAR* Routes[] = {TEXT("Blood"),TEXT("Crescent"),TEXT("Iaijutsu"),TEXT("Fang"),TEXT("Barrage"),TEXT("Shuriken")};
        if (SmokeRemaining <= 0.f)
        {
            if (SmokeSample >= 0)
            {
                WriteReport();
                if (TotalDamage <= 0.f) { bSmokeFailed = true; UE_LOG(LogTemp, Error, TEXT("BALANCE_LAB_SMOKE_NO_DAMAGE %s"), *BuildName); }
            }
            if (++SmokeSample >= 12)
            {
                UE_LOG(LogTemp, Display, TEXT("BALANCE_LAB_SMOKE_%s"), bSmokeFailed ? TEXT("FAIL") : TEXT("PASS"));
                FPlatformMisc::RequestExitWithStatus(false, bSmokeFailed ? 1 : 0); return;
            }
            SetBuild(Routes[SmokeSample % 6], SmokeSample / 6 * 3);
            SpawnTargets(TEXT("Dummy"), 1);
            auto* Active = PC->GetCharacterManager()->GetActiveCharacter();
            if (Targets.Num() && Targets[0].IsValid()) Targets[0]->SetActorLocation(Active->GetActorLocation() + Active->GetVisualForwardVector().GetSafeNormal2D() * 180.f);
            SmokeRemaining = 9.f;
        }
        SmokeRemaining -= Delta;
    }
    if (GEngine) GEngine->AddOnScreenDebugMessage(0xBA1A, .2f, FColor::Cyan,
        TEXT("BALANCE LAB | Editor: Tools > Balance Lab | ~ console: BalanceLab build Blood 0 | target Grunt 12 | grant BloodCritical 1 | meta full | god off | report | clear\n") + Report());
#endif
}
void ABalanceLabDirector::ResetMeter()
{
    TotalDamage = 0; Hits = Kills = 0; FirstHitTime = LastHitTime = -1;
}
void ABalanceLabDirector::OnDamage(float Amount, float)
{
    if (FirstHitTime < 0) FirstHitTime = GetWorld()->GetTimeSeconds();
    LastHitTime = GetWorld()->GetTimeSeconds(); TotalDamage += Amount; ++Hits;
}
void ABalanceLabDirector::OnTargetDied(AEnemyBase*) { ++Kills; }
FString ABalanceLabDirector::Report() const
{
    int32 Alive = 0; for (auto E : Targets) if (E.IsValid() && !E->IsDead()) ++Alive;
    const float End = Alive > 0 ? GetWorld()->GetTimeSeconds() : LastHitTime;
    const float Seconds = FirstHitTime >= 0 ? FMath::Max(0.f, End - FirstHitTime) : 0.f;
    auto* PC = Controller.Get(); const auto* U = PC ? PC->GetPlayerUpgrades() : nullptr;
    return FString::Printf(TEXT("%s / %s | meta %s | %d alive | %.1f actual damage / %.1fs = %.1f aggregate DPS | %d damage events, %d kills"),
        *BuildName, *TargetName, U && U->BalancePreviewMetaProfile == 1 ? TEXT("full") : TEXT("fresh"), Alive,
        TotalDamage, Seconds, Seconds >= 1.f ? TotalDamage / Seconds : 0.f, Hits, Kills);
}
bool ABalanceLabDirector::WriteReport() const
{
    FString Text = FDateTime::Now().ToString() + TEXT(" | ") + Report() + TEXT("\n");
    if (Controller.IsValid())
    {
        FPlayerUpgradeRunState State; Controller->GetPlayerUpgrades()->CaptureRunState(State);
        TArray<FName> Ids; State.Levels.GetKeys(Ids); Ids.Sort(FNameLexicalLess());
        for (FName Id : Ids) Text += FString::Printf(TEXT("  %s rank %d magnitude %.3f\n"), *Id.ToString(), State.Levels[Id], State.AccumulatedMagnitudes.FindRef(Id));
    }
    const FString Dir = FPaths::ProjectSavedDir() / TEXT("BalanceLab");
    IFileManager::Get().MakeDirectory(*Dir, true);
    const bool bSaved = FFileHelper::SaveStringToFile(Text, *(Dir / TEXT("Measurements.txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
    UE_LOG(LogTemp, Display, TEXT("[BalanceLab] %s"), *Text);
    return bSaved;
}
void ABalanceLabDirector::ClearTargets()
{
    for (auto E : Targets) if (E.IsValid()) E->Destroy(); Targets.Reset();
    // Flush effects from the previous sample before replacing its build/targets.
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        if (It->IsA<AAttackProjectileBase>() || It->IsA<ANinjaBuildProjectile>() || It->IsA<ASamuraiBladeWave>()
            || It->IsA<ASamuraiIaijutsu>() || It->IsA<ASamuraiWaveField>() || It->IsA<ABarragePoisonPool>() || It->IsA<AShadowClone>()) It->Destroy();
    if (Controller.IsValid()) if (auto* N = Controller->GetCharacterManager()->GetNinja())
        if (auto* B = N->FindComponentByClass<UNinjaBuildComponent>()) B->ClearProjectiles();
    ResetMeter();
}
bool ABalanceLabDirector::SpawnTargets(const FString& Name, int32 Count)
{
    UClass* EnemyClass = nullptr;
    const bool Dummy = Name.Equals(TEXT("Dummy"), ESearchCase::IgnoreCase);
    for (const auto& Row : UTesterBalanceSettings::Roster())
    {
        FString Key = Row.Key.Replace(TEXT(" "), TEXT(""));
        if (Key.Equals(Name, ESearchCase::IgnoreCase) || (Dummy && Key == TEXT("TestDummy")))
        { EnemyClass = LoadClass<AEnemyBase>(nullptr, *Row.Value); break; }
    }
    if (!EnemyClass) { Controller->ClientMessage(TEXT("Unknown target. Examples: Dummy, Grunt, Bear, OgreWarden, StormGorilla, CinderOracleBoss.")); return false; }
    ClearTargets(); TargetName = Name;
    auto* Active = Controller->GetCharacterManager()->GetActiveCharacter();
    const FVector Forward = Active->GetVisualForwardVector().GetSafeNormal2D();
    const FVector Side = FVector::CrossProduct(FVector::UpVector, Forward);
    Count = FMath::Clamp(Count, 1, 40);
    for (int32 I = 0; I < Count; ++I)
    {
        FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        const FVector P = Active->GetActorLocation() + Forward * (350.f + (I / 5) * 160.f) + Side * ((I % 5) - FMath::Min(Count - 1, 4) * .5f) * 140.f;
        auto* E = GetWorld()->SpawnActor<AEnemyBase>(EnemyClass, P, FRotator::ZeroRotator, Params);
        if (!E) continue;
        E->ConfigureObjectiveEnemy(Dummy ? 100000.f : E->GetHealthComponent()->GetMaxHealth(), EPlayerAttackSource::Other, nullptr, FLinearColor::White);
        E->GetHealthComponent()->RestoreCurrentHealth(E->GetHealthComponent()->GetMaxHealth());
        if (Dummy) E->SetGameplaySuspended(true);
        E->GetHealthComponent()->OnDamaged.AddDynamic(this, &ABalanceLabDirector::OnDamage);
        E->OnEnemyDied.AddDynamic(this, &ABalanceLabDirector::OnTargetDied); Targets.Add(E);
    }
    return !Targets.IsEmpty();
}
bool ABalanceLabDirector::SetBuild(const FString& Name, int32 Ranks)
{
    static const TCHAR* Names[] = {TEXT("Blood"), TEXT("Crescent"), TEXT("Iaijutsu"), TEXT("Fang"), TEXT("Barrage"), TEXT("Shuriken")};
    static const TCHAR* Cards[][3] = {
        {TEXT("BattleStance"), TEXT("SamuraiHeavyBlade"), TEXT("SamuraiTempo")},
        {TEXT("BladeWave"), TEXT("CrescentDamage"), TEXT("CrescentSpeed")},
        {TEXT("Iaijutsu"), TEXT("IaijutsuDamage"), TEXT("IaijutsuChargeSpeed")},
        {TEXT("ReturningFang"), TEXT("FangDamage"), TEXT("FangSpeed")},
        {TEXT("BarrageStance"), TEXT("BarrageDamage"), TEXT("BarrageSpeed")},
        {TEXT("GreatShuriken"), TEXT("ShurikenDamage"), TEXT("ShurikenSpeed")}};
    int32 Route = INDEX_NONE; for (int32 I = 0; I < 6; ++I) if (Name.Equals(Names[I], ESearchCase::IgnoreCase)) Route = I;
    if (Route == INDEX_NONE) { Controller->ClientMessage(TEXT("Build: Blood, Crescent, Iaijutsu, Fang, Barrage, Shuriken. Count = damage/speed ranks (0-5).")); return false; }
    ClearTargets(); BuildName = Names[Route];
    auto* U = Controller->GetPlayerUpgrades(); U->RestoreRunState(FPlayerUpgradeRunState());
    auto Grant = [U](FName Id) { if (auto* Card = U->FindUpgradeDefinition(Id)) U->AcquireUpgrade(Card); };
    Grant(Route < 3 ? FName(TEXT("BarrageStance")) : FName(TEXT("BattleStance")));
    Grant(Cards[Route][0]);
    for (int32 I = 0; I < FMath::Clamp(Ranks, 0, 5); ++I) { Grant(Cards[Route][1]); Grant(Cards[Route][2]); }
    auto* Party = Controller->GetCharacterManager();
    if (Party->GetActiveCharacter()->IsA<ANinjaCharacter>() != (Route >= 3)) Party->SwapCharacter();
    Controller->GetPlayerHealthComponent()->RestoreCurrentHealth(Controller->GetPlayerHealthComponent()->GetMaxHealth());
    if (auto* Combo = Controller->FindComponentByClass<UComboAbilityComponent>()) Combo->RestoreCombo(0);
    return true;
}
bool ABalanceLabDirector::IsReady() const
{
    const auto* PC = Controller.Get();
    return bInitialized && PC && PC->GetCharacterManager() && PC->GetCharacterManager()->GetActiveCharacter()
        && PC->GetPlayerUpgrades() && PC->GetPlayerHealthComponent() && !PC->GetPlayerHealthComponent()->IsDead();
}
FString ABalanceLabDirector::Command(ASurvivorPlayerController* PC, const FString& Action, const FString& Value, int32 Count)
{
    if (!IsReady() || PC != Controller.Get()) return TEXT("Wait for the lab player to spawn, or restart the test after dying.");
    if (auto* Combo = PC->FindComponentByClass<UComboAbilityComponent>(); Combo && Combo->IsAbilityActive()
        && !Action.Equals(TEXT("report"), ESearchCase::IgnoreCase) && !Action.Equals(TEXT("god"), ESearchCase::IgnoreCase))
        return TEXT("Let the current combo finish before changing the sample.");
    if (Action.Equals(TEXT("build"), ESearchCase::IgnoreCase))
    {
        const auto* Active = PC->GetCharacterManager()->GetActiveCharacter();
        if (Active->SwapPresentation && Active->SwapPresentation->IsBlockingAttacks())
            return TEXT("Let the character arrival animation finish before changing builds.");
        if (!SetBuild(Value, Count)) return TEXT("Unknown build. Choose Blood, Crescent, Iaijutsu, Fang, Barrage or Shuriken.");
        if (!SpawnTargets(TEXT("Dummy"), 1)) return TEXT("Build applied, but the dummy could not be spawned.");
        return FString::Printf(TEXT("%s: %d common damage and speed ranks. Run upgrades reset; fresh dummy and measurement ready."), *BuildName, FMath::Clamp(Count, 0, 5));
    }
    if (Action.Equals(TEXT("target"), ESearchCase::IgnoreCase))
        return SpawnTargets(Value, Count) ? FString::Printf(TEXT("Spawned %d %s targets. Measurement reset; build preserved."), Targets.Num(), *Value)
            : TEXT("Could not spawn targets. Check the enemy selection and Output Log.");
    if (Action.Equals(TEXT("grant"), ESearchCase::IgnoreCase))
    {
        auto* U = PC->GetPlayerUpgrades(); auto* Card = U->FindUpgradeDefinition(FName(*Value));
        int32 Granted = 0;
        if (Card) for (int32 I = 0; I < FMath::Clamp(Count, 1, 10); ++I) { if (!U->AcquireUpgrade(Card)) break; ++Granted; }
        if (Granted > 0) ResetMeter();
        return FString::Printf(TEXT("Granted %d ranks of %s. Prerequisites, exclusions and rank caps apply. Spawn fresh targets for a clean sample."), Granted, *Value);
    }
    if (Action.Equals(TEXT("meta"), ESearchCase::IgnoreCase))
    {
        if (!Value.Equals(TEXT("fresh"), ESearchCase::IgnoreCase) && !Value.Equals(TEXT("full"), ESearchCase::IgnoreCase)) return TEXT("Choose fresh or full permanent combat stats.");
        PC->GetPlayerUpgrades()->BalancePreviewMetaProfile = Value.Equals(TEXT("full"), ESearchCase::IgnoreCase) ? 1 : 0;
        PC->GetPlayerUpgrades()->RebuildAllUpgradeModifiers(); SpawnTargets(TEXT("Dummy"), 1);
        return TEXT("Permanent combat preview: ") + Value + TEXT(". Saved skills unchanged; fresh dummy and measurement ready.");
    }
    if (Action.Equals(TEXT("god"), ESearchCase::IgnoreCase))
    {
        if (!Value.Equals(TEXT("on"), ESearchCase::IgnoreCase) && !Value.Equals(TEXT("off"), ESearchCase::IgnoreCase)) return TEXT("Choose god on or off.");
        const bool bDamageEnabled = Value.Equals(TEXT("off"), ESearchCase::IgnoreCase);
        PC->GetPlayerHealthComponent()->SetDamageEnabled(bDamageEnabled);
        return bDamageEnabled ? TEXT("Incoming damage enabled. Enemies can now hurt you.") : TEXT("Invulnerability enabled.");
    }
    if (Action.Equals(TEXT("combo"), ESearchCase::IgnoreCase))
    {
        if (auto* C = PC->FindComponentByClass<UComboAbilityComponent>()) { if (!C->IsComboUnlocked()) return TEXT("Combo burst locked. Use BalanceLab meta full to preview the unlocked ability."); C->AddCombo(C->GetMaxCombo()); return TEXT("Combo meter filled. Activate the ability with your normal game input."); }
        return TEXT("Combo component unavailable.");
    }
    if (Action.Equals(TEXT("report"), ESearchCase::IgnoreCase))
        return WriteReport() ? TEXT("Measurement appended to Saved/BalanceLab/Measurements.txt.") : TEXT("Could not write Measurements.txt. Check folder permissions.");
    if (Action.Equals(TEXT("clear"), ESearchCase::IgnoreCase)) { ClearTargets(); return TEXT("Targets, lingering effects and measurement cleared. Build preserved."); }
    return TEXT("BalanceLab build Blood 0 | target Dummy 1 | grant UpgradeId 1 | meta fresh/full | god on/off | combo | report | clear");
}
