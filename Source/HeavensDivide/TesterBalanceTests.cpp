#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "TesterBalanceSettings.h"
#include "TesterBalanceWidget.h"
#include "MainMenuWidget.h"
#include "EnemyBase.h"
#include "EnemySpawner.h"
#include "HealthComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#if WITH_EDITOR
#include "AssetCompilingManager.h"
#endif
#include "Engine/World.h"
#include "UObject/UnrealType.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Slate/WidgetRenderer.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTesterBalanceTest, "HeavensDivide.Tester.Balance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTesterBalanceTest::RunTest(const FString&)
{
    auto* Settings = NewObject<UTesterBalanceSettings>(); Settings->Reset();
    auto* Original = NewObject<UTesterBalanceSettings>(); Original->CopyFrom(*GetDefault<UTesterBalanceSettings>());
    auto* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    auto* GI = NewObject<UGameInstance>(GEngine); World->SetGameInstance(GI);
    auto* Enemy = World->SpawnActor<AEnemyBase>();
    const float Health = Enemy->GetHealthComponent()->GetMaxHealth();
    auto* SpeedProperty = FindFProperty<FFloatProperty>(AEnemyBase::StaticClass(), TEXT("MoveSpeed"));
    const float Speed = SpeedProperty->GetPropertyValue_InContainer(Enemy);
    const FString Key = Enemy->GetClass()->GetPathName();
    Settings->Enemies.FindOrAdd(Key).Health = 2;
    Settings->Enemies.FindOrAdd(Key).Speed = .5f;
    Settings->ApplyEnemy(Enemy);
    TestEqual(TEXT("Disabled overrides preserve enemy health"), Enemy->GetHealthComponent()->GetMaxHealth(), Health);
    Settings->bEnabled = true; Settings->ApplyEnemy(Enemy);
    TestEqual(TEXT("Health override applies"), Enemy->GetHealthComponent()->GetMaxHealth(), Health * 2);
    TestEqual(TEXT("Speed override applies"), SpeedProperty->GetPropertyValue_InContainer(Enemy), Speed * .5f);
    auto* Spawner = World->SpawnActor<AEnemySpawner>();
    FEnemyPressurePhase Phase; Phase.GlobalMaxAlive = 20; Phase.bEventsEnabled = true;
    FEnemyPopulationPhaseEntry Population; Population.DesiredPopulation = 10; Population.MaxPopulation = 15;
    Phase.EnemyPopulationEntries.Add(Population); Spawner->PressurePhases.Add(Phase);
    FEnemySpawnEntry Definition; Definition.HealthScalingPerMinute = .1f; Spawner->EnemySpawnEntries.Add(Definition);
    Settings->MaxAlive = 80; Settings->Population = 2; Settings->SpawnInterval = .5f;
    Settings->HealthGrowth = 0; Settings->bDisableEvents = true;
    Settings->ApplySpawner(Spawner);
    TestEqual(TEXT("Absolute cap changed"), Spawner->AbsoluteHardAliveCap, 80);
    TestEqual(TEXT("Phase cap changed"), Spawner->PressurePhases[0].GlobalMaxAlive, 80);
    TestEqual(TEXT("Population target scales"), Spawner->PressurePhases[0].EnemyPopulationEntries[0].DesiredPopulation, 20);
    TestEqual(TEXT("Per-type cap scales"), Spawner->PressurePhases[0].EnemyPopulationEntries[0].MaxPopulation, 30);
    TestEqual(TEXT("Refill interval scales"), Spawner->PressurePhases[0].NormalSpawnInterval, Phase.NormalSpawnInterval * .5f);
    TestFalse(TEXT("Events disabled"), Spawner->PressurePhases[0].bEventsEnabled);
    TestEqual(TEXT("Health growth disabled"), Spawner->EnemySpawnEntries[0].HealthScalingPerMinute, 0.f);
    // Save/reload an isolated file, never overwrite a tester's real configuration.
    const FString File = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation/TesterBalanceRoundtrip.ini"));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true);
    Settings->SaveConfig(CPF_Config, *File);
    auto* Loaded = NewObject<UTesterBalanceSettings>(); Loaded->Reset(); Loaded->LoadConfig(nullptr, *File);
    TestEqual(TEXT("Configuration cap persists"), Loaded->MaxAlive, 80);
    TestEqual(TEXT("Per-enemy configuration persists"), Loaded->Enemies.FindRef(Key).Health, 2.f);
    GetMutableDefault<UTesterBalanceSettings>()->CopyFrom(*Original);
    Settings->MaxAlive = 100000; Settings->Enemies.FindOrAdd(Key).Speed = -20; Settings->Sanitize();
    TestEqual(TEXT("Population bound enforced"), Settings->MaxAlive, 500);
    TestEqual(TEXT("Speed bound enforced"), Settings->Enemies.FindRef(Key).Speed, .1f);
    Settings->Reset(); TestFalse(TEXT("Reset disables overrides"), Settings->bEnabled);
    TestTrue(TEXT("Reset clears enemy overrides"), Settings->Enemies.IsEmpty());
    for (const auto& Row : UTesterBalanceSettings::Roster())
        TestNotNull(*FString::Printf(TEXT("Roster class exists: %s"), *Row.Key), LoadClass<AEnemyBase>(nullptr, *Row.Value));
    auto* MenuClass = LoadClass<UMainMenuWidget>(nullptr, TEXT("/Game/HeavensDivide/Blueprints/UI/MainMenu/WBP_MainMenu.WBP_MainMenu_C"));
    auto* PC = World->SpawnActor<APlayerController>();
    auto* Player = NewObject<ULocalPlayer>(GEngine); Player->SetControllerId(0); PC->SetPlayer(Player);
    auto* Menu = CreateWidget<UMainMenuWidget>(PC, MenuClass);
    // Background video is irrelevant to this page; avoid a dependency on external movie files.
    FindFProperty<FObjectProperty>(UMainMenuWidget::StaticClass(), TEXT("BackgroundMediaPlayer"))->SetObjectPropertyValue_InContainer(Menu, nullptr);
    auto Slate = Menu->TakeWidget();
    auto* Button = Cast<UButton>(Menu->WidgetTree->FindWidget(TEXT("TesterBalanceButton")));
    if (TestNotNull(TEXT("Main menu exposes tester button"), Button)) Button->OnClicked.Broadcast();
    if (FParse::Param(FCommandLine::Get(), TEXT("TesterBalanceScreenshot")))
    {
        FWidgetRenderer Renderer(true, false);
#if WITH_EDITOR
        FAssetCompilingManager::Get().FinishAllCompilation();
#endif
        auto* Warmup = Renderer.DrawWidget(Slate, FVector2D(1920, 1080));
        TArray<FColor> WarmupPixels; Warmup->GameThread_GetRenderTargetResource()->ReadPixels(WarmupPixels);
        auto* Target = Renderer.DrawWidget(Slate, FVector2D(1920, 1080));
        TArray<FColor> Pixels; Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
        TArray64<uint8> PNG; FImageUtils::PNGCompressImageArray(1920,1080,Pixels,PNG);
        FFileHelper::SaveArrayToFile(PNG, *(FPaths::ProjectSavedDir() / TEXT("TesterBalance.png")));
    }
    World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
    return true;
}
#endif
