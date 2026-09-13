#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "HeavensDivideGameUserSettings.h"
#include "SurvivorPlayerController.h"
#include "InputMappingContext.h"
#include "MainMenuWidget.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKeybindSettingsTest,"HeavensDivide.Settings.Keybinds",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FKeybindSettingsTest::RunTest(const FString&)
{
 // Redirect persistence checks away from the player's actual settings.
 const FString TestIni=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Automation/KeybindSettings.ini"));
 TGuardValue<FString> UserIni(GGameUserSettingsIni,TestIni);
 TGuardValue<FString> EditorIni(GEditorSettingsIni,TestIni);
 auto* Settings=NewObject<UHeavensDivideGameUserSettings>();Settings->ResetKeyBindings();
 TestEqual(TEXT("Default swap is right-click"),Settings->GetKeyBinding(TEXT("Swap")),EKeys::RightMouseButton);
 TestTrue(TEXT("Keyboard remap accepted"),Settings->SetKeyBinding(TEXT("Swap"),EKeys::Q));
 TestEqual(TEXT("Override active"),Settings->GetKeyBinding(TEXT("Swap")),EKeys::Q);
 TestTrue(TEXT("Mouse remap accepted"),Settings->SetKeyBinding(TEXT("Interact"),EKeys::MiddleMouseButton));
 TestTrue(TEXT("Conflicting key accepted by exchange"),Settings->SetKeyBinding(TEXT("Swap"),EKeys::W));
 TestEqual(TEXT("Conflicting movement gets previous swap key"),Settings->GetKeyBinding(TEXT("MoveForward")),EKeys::Q);
 TestFalse(TEXT("Escape stays available for cancellation"),Settings->SetKeyBinding(TEXT("Dash"),EKeys::Escape));
 TestFalse(TEXT("Gamepad bindings are separate"),Settings->SetKeyBinding(TEXT("Swap"),EKeys::Gamepad_FaceButton_Top));
 auto* Reloaded=NewObject<UHeavensDivideGameUserSettings>();Reloaded->LoadConfig(nullptr,*TestIni);
 TestEqual(TEXT("Saved binding reloads"),Reloaded->GetKeyBinding(TEXT("Swap")),EKeys::W);
 TestEqual(TEXT("Conflict exchange reloads"),Reloaded->GetKeyBinding(TEXT("MoveForward")),EKeys::Q);
 UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);World->InitializeActorsForPlay(FURL());
 const auto Class=LoadClass<ASurvivorPlayerController>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController.BP_SurvivorPlayerController_C"));
 auto* PC=World->SpawnActor<ASurvivorPlayerController>(Class);
 const auto Original=PC->DefaultMappingContext->GetMappings();
 PC->BuildRuntimeKeyMappings(Settings);
 TestTrue(TEXT("Runtime mappings use a private copy"),PC->RuntimeMappingContext!=PC->DefaultMappingContext);
 auto HasMapping=[&](const UInputAction* Action,FKey Key){return PC->RuntimeMappingContext->GetMappings().ContainsByPredicate([=](const FEnhancedActionKeyMapping& M){return M.Action==Action&&M.Key==Key;});};
 TestTrue(TEXT("Runtime swap uses chosen key"),HasMapping(PC->SwapAction,EKeys::W));
 TestFalse(TEXT("Old mouse swap removed from runtime"),HasMapping(PC->SwapAction,EKeys::RightMouseButton));
 TestTrue(TEXT("Runtime movement uses exchanged key"),HasMapping(PC->MoveAction,EKeys::Q));
 TestTrue(TEXT("Runtime interact uses mouse binding"),HasMapping(PC->InteractAction,EKeys::MiddleMouseButton));
 TestFalse(TEXT("Old interact E binding is removed"),HasMapping(PC->InteractAction,EKeys::E));
 TestTrue(TEXT("Controller swap preserved"),HasMapping(PC->SwapAction,EKeys::Gamepad_FaceButton_Top));
 for(const auto& M:Original)if(M.Action==PC->MoveAction&&M.Key==EKeys::W)
 {
  const auto* Remapped=PC->RuntimeMappingContext->GetMappings().FindByPredicate([&](const auto& R){return R.Action==PC->MoveAction&&R.Key==EKeys::Q;});
  TestTrue(TEXT("Movement modifiers preserved"),Remapped&&Remapped->Modifiers.Num()==M.Modifiers.Num());
 }
 TestEqual(TEXT("Source asset mapping count unchanged"),PC->DefaultMappingContext->GetMappings().Num(),Original.Num());
 TestTrue(TEXT("Source asset persists right-click default"),Original.ContainsByPredicate([&](const auto& M){return M.Action==PC->SwapAction&&M.Key==EKeys::RightMouseButton;}));
 Settings->ResetKeyBindings();PC->BuildRuntimeKeyMappings(Settings);
 TestTrue(TEXT("Reset restores live default swap"),HasMapping(PC->SwapAction,EKeys::RightMouseButton));
 TestTrue(TEXT("Reset restores movement"),HasMapping(PC->MoveAction,EKeys::W));
 auto* GI=NewObject<UGameInstance>(GEngine);World->SetGameInstance(GI);
 auto* LocalPlayer=NewObject<ULocalPlayer>(GEngine);LocalPlayer->SetControllerId(0);PC->SetPlayer(LocalPlayer);
 const auto MenuClass=LoadClass<UMainMenuWidget>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/UI/MainMenu/WBP_MainMenu.WBP_MainMenu_C"));
 auto* Menu=CreateWidget<UMainMenuWidget>(PC,MenuClass);
 if(TestNotNull(TEXT("Saved menu builds"),Menu))
 {
  Menu->TakeWidget();Menu->ShowSettingsPanel();Menu->HandleKeybindSettings();
  TestEqual(TEXT("Keybind settings subpage opens"),Menu->SettingsSwitcher->GetActiveWidgetIndex(),1);
  TestEqual(TEXT("All keyboard/mouse actions have selectors"),Menu->KeybindSelectors.Num(),7);
  TestTrue(TEXT("Shared page background is configured"),Menu->GetPageBackgroundTexture()!=nullptr);
  Menu->HandleBack();TestTrue(TEXT("Back returns to settings"),Menu->bSettingsPopupOpen&&Menu->SettingsSwitcher->GetActiveWidgetIndex()==0);
 }
 World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return true;
}
#endif
