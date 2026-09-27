#include "TesterBalanceSettings.h"
#include "EnemyBase.h"
#include "EnemySpawner.h"

namespace { float Safe(float Value, float Min, float Max) { return FMath::IsFinite(Value) ? FMath::Clamp(Value, Min, Max) : 1.f; } }
void UTesterBalanceSettings::Reset()
{
    bEnabled = bDisableEvents = false;
    MaxAlive = 0;
    Population = SpawnInterval = HealthGrowth = 1.f;
    Enemies.Reset();
}
void UTesterBalanceSettings::CopyFrom(const UTesterBalanceSettings& Other)
{
    bEnabled = Other.bEnabled; MaxAlive = Other.MaxAlive;
    Population = Other.Population; SpawnInterval = Other.SpawnInterval;
    HealthGrowth = Other.HealthGrowth; bDisableEvents = Other.bDisableEvents;
    Enemies = Other.Enemies;
    Sanitize();
}
void UTesterBalanceSettings::Sanitize()
{
    MaxAlive = FMath::Clamp(MaxAlive, 0, 500);
    Population = Safe(Population, .1f, 5.f);
    SpawnInterval = Safe(SpawnInterval, .1f, 5.f);
    HealthGrowth = Safe(HealthGrowth, 0.f, 5.f);
    for (auto& Pair : Enemies)
    {
        Pair.Value.Health = Safe(Pair.Value.Health, .1f, 10.f);
        Pair.Value.Speed = Safe(Pair.Value.Speed, .1f, 3.f);
    }
}
void UTesterBalanceSettings::ApplyEnemy(AEnemyBase* Enemy) const
{
    if (!bEnabled || !Enemy) return;
    if (const auto* Values = Enemies.Find(Enemy->GetClass()->GetPathName()))
        Enemy->ApplySpawnInstanceModifiers(Safe(Values->Health, .1f, 10.f), 1.f, Safe(Values->Speed, .1f, 3.f));
}
float UTesterBalanceSettings::GetHealthMultiplier(const AEnemyBase* Enemy) const
{
    if (bEnabled && Enemy)
        if (const auto* Values = Enemies.Find(Enemy->GetClass()->GetPathName())) return Safe(Values->Health, .1f, 10.f);
    return 1.f;
}
void UTesterBalanceSettings::ApplySpawner(AEnemySpawner* Spawner) const
{
    if (!bEnabled || !Spawner) return;
    const int32 Cap = FMath::Clamp(MaxAlive, 0, 500);
    const float Density = Safe(Population, .1f, 5.f);
    if (Cap > 0) Spawner->AbsoluteHardAliveCap = Cap;
    else Spawner->AbsoluteHardAliveCap = FMath::Clamp(FMath::RoundToInt(Spawner->AbsoluteHardAliveCap * Density), 1, 500);
    for (auto& Phase : Spawner->PressurePhases)
    {
        Phase.GlobalMaxAlive = Cap > 0 ? Cap : FMath::Clamp(FMath::RoundToInt(Phase.GlobalMaxAlive * Density), 0, 500);
        for (auto& Entry : Phase.EnemyPopulationEntries)
        {
            Entry.MaxPopulation = FMath::Clamp(FMath::RoundToInt(Entry.MaxPopulation * Density), 0, 500);
            Entry.DesiredPopulation = FMath::Clamp(FMath::RoundToInt(Entry.DesiredPopulation * Density), 0, Entry.MaxPopulation);
        }
        const float Interval = Safe(SpawnInterval, .1f, 5.f);
        Phase.NormalSpawnInterval *= Interval;
        Phase.AcceleratedSpawnInterval *= Interval;
        Phase.EmergencySpawnInterval *= Interval;
        if (bDisableEvents) Phase.bEventsEnabled = false;
    }
    for (auto& Entry : Spawner->EnemySpawnEntries) Entry.HealthScalingPerMinute *= Safe(HealthGrowth, 0.f, 5.f);
    UE_LOG(LogTemp, Display, TEXT("[TesterBalance] %s"), *Report());
}
TArray<TPair<FString, FString>> UTesterBalanceSettings::Roster()
{
    TArray<TPair<FString, FString>> Result;
    auto Add = [&Result](const TCHAR* Name, const TCHAR* Folder, const TCHAR* Asset)
    {
        const FString Path = FString(TEXT("/Game/HeavensDivide/Blueprints/")) + Folder + TEXT("/") + Asset;
        Result.Emplace(Name, Path + TEXT(".") + Asset + TEXT("_C"));
    };
    Add(TEXT("Grunt"), TEXT("EnemyCharacters/Mobs"), TEXT("BP_EnemyGrunt"));
    Add(TEXT("Goblin Crawler"), TEXT("EnemyCharacters/Mobs"), TEXT("BP_EnemyGoblinCrawler"));
    Add(TEXT("Creature"), TEXT("EnemyCharacters/Mobs"), TEXT("BP_EnemyCreature"));
    Add(TEXT("Fish"), TEXT("EnemyCharacters/Mobs"), TEXT("BP_EnemyFish"));
    Add(TEXT("Floating Skeleton"), TEXT("EnemyCharacters/Mobs"), TEXT("BP_EnemyFloatingSkeleton"));
    Add(TEXT("Bear"), TEXT("EnemyCharacters/Mobs"), TEXT("BP_EnemyBear"));
    Add(TEXT("Devil Ranged"), TEXT("EnemyCharacters/Mobs"), TEXT("BP_EnemyDevilRanged"));
    Add(TEXT("Goblin Bomb"), TEXT("EnemyCharacters/Mobs"), TEXT("BP_EnemyGoblinBomb"));
    Add(TEXT("Ogre"), TEXT("EnemyCharacters/Elites"), TEXT("BP_EnemyOgre"));
    Add(TEXT("Gorilla"), TEXT("EnemyCharacters/Elites"), TEXT("BP_EnemyGorilla"));
    Add(TEXT("Samurai Boss"), TEXT("EnemyCharacters/Bosses/SamuraiBoss"), TEXT("BP_SamuraiBoss"));
    Add(TEXT("Twin Soul Crimson"), TEXT("Objectives/TwinSoulTrial"), TEXT("BP_TwinSoulCrimson"));
    Add(TEXT("Twin Soul Violet"), TEXT("Objectives/TwinSoulTrial"), TEXT("BP_TwinSoulViolet"));
    Add(TEXT("Test Dummy"), TEXT("EnemyCharacters/Mobs"), TEXT("BP_TestDummy"));
    return Result;
}
FString UTesterBalanceSettings::Report() const
{
    FString Text = FString::Printf(TEXT("Heavens Divide tester balance v1\nEnabled: %s\nMax alive: %d (0 = authored phases)\nPopulation: %.2fx\nSpawn interval: %.2fx\nHealth growth: %.2fx\nPressure events disabled: %s\n"),
        bEnabled ? TEXT("yes") : TEXT("no"), MaxAlive, Population, SpawnInterval, HealthGrowth, bDisableEvents ? TEXT("yes") : TEXT("no"));
    for (const auto& Row : Roster())
    {
        const auto Value = Enemies.FindRef(Row.Value);
        Text += FString::Printf(TEXT("%s: health %.2fx, speed %.2fx\n"), *Row.Key, Value.Health, Value.Speed);
    }
    return Text;
}
