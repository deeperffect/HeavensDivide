#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "SurvivorPlayerController.h"
#include "PauseMenuWidget.h"
#include "MainMenuWidget.h"
#include "Components/Button.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPauseMenuTest,"HeavensDivide.UI.PauseMenu",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPauseMenuTest::RunTest(const FString&)
{
    auto* World=UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->SetGameInstance(NewObject<UGameInstance>(GEngine));
    World->SetGameMode(FURL());
    World->InitializeActorsForPlay(FURL());
    auto* PC=World->SpawnActor<ASurvivorPlayerController>();
    auto* Player=NewObject<ULocalPlayer>(GEngine);Player->SetControllerId(0);PC->SetPlayer(Player);
    PC->PlayerState=World->SpawnActor<APlayerState>();
    PC->bLevelUpTimeDilationApplied=true;
    PC->TogglePauseMenu();TestFalse(TEXT("Pause cannot cover reward choices"),PC->IsPauseMenuOpen());
    PC->bLevelUpTimeDilationApplied=false;
    World->GetWorldSettings()->SetTimeDilation(.7f);
    PC->TogglePauseMenu();
    TestTrue(TEXT("Run pauses"),UGameplayStatics::IsGamePaused(World));
    if(TestNotNull(TEXT("Pause menu created"),PC->PauseMenu.Get()))
    {
        auto* Pause=PC->PauseMenu.Get();
        for(const TCHAR* Name:{TEXT("ResumeButton"),TEXT("SettingsButton"),TEXT("MainMenuButton")})
        {
            auto* Button=Cast<UButton>(Pause->GetWidgetFromName(Name));
            if(TestNotNull(TEXT("Pause action exists"),Button))TestTrue(TEXT("Pause action is bound"),Button->OnClicked.IsBound());
        }
        Pause->Settings();
        if(TestNotNull(TEXT("Uses actual menu settings"),Pause->SettingsWidget.Get()))
        {
            auto* Settings=Pause->SettingsWidget.Get();
            TestTrue(TEXT("Settings opens in run mode"),Settings->bInRunSettings && Settings->bSettingsPopupOpen);
            Settings->HandleKeybindSettings();
            TestEqual(TEXT("Keybindings open inside pause settings"),Settings->SettingsSwitcher->GetActiveWidgetIndex(),1);
            Settings->HandleBack();
            TestEqual(TEXT("Keybindings back returns to general settings"),Settings->SettingsSwitcher->GetActiveWidgetIndex(),0);
            TestTrue(TEXT("Settings never unpauses the run"),UGameplayStatics::IsGamePaused(World));
            Settings->HandleBack();
            TestNull(TEXT("Settings back closes settings"),Pause->SettingsWidget.Get());
            TestTrue(TEXT("Back keeps pause menu open"),PC->IsPauseMenuOpen() && UGameplayStatics::IsGamePaused(World));
        }
        PC->ResumePausedRun();
        TestFalse(TEXT("Resume unpauses"),UGameplayStatics::IsGamePaused(World));
        TestFalse(TEXT("Resume removes menu"),PC->IsPauseMenuOpen());
        TestEqual(TEXT("Pause preserves existing slow motion"),World->GetWorldSettings()->TimeDilation,.7f);
        PC->TogglePauseMenu();PC->TogglePauseMenu();
        TestFalse(TEXT("Pause toggles repeatedly"),PC->IsPauseMenuOpen());
    }
    World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return true;
}
#endif
