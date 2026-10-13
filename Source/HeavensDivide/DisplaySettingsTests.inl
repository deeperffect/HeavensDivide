#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "MainMenuWidget.h"
#include "Components/NativeWidgetHost.h"
#include "Components/WidgetSwitcher.h"
#include "Widgets/Input/SButton.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplaySettingsTest,"HeavensDivide.Settings.Display",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDisplaySettingsTest::RunTest(const FString&)
{
    const FString TestIni=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Automation/DisplaySettings.ini"));
    // UE 5.8 SaveSettings reloads the native path via ReloadWindowPositions.
    // Exercise that real path in this subprocess's cache, prevent disk writes,
    // and restore the complete cache afterwards instead of redirecting a path
    // the engine replaces internally.
    const FString NativeIni=GGameUserSettingsIni;
    FConfigFile* NativeConfig=GConfig->Find(NativeIni);
    if(!TestNotNull(TEXT("Native user settings config exists"),NativeConfig))return false;
    const FConfigFile OriginalConfig=*NativeConfig;
    NativeConfig->NoSave=true;
    ON_SCOPE_EXIT { *GConfig->Find(NativeIni)=OriginalConfig; };
    TGuardValue<FString> EditorIni(GEditorSettingsIni,TestIni);
    auto* Settings=NewObject<UHeavensDivideGameUserSettings>();
    TGuardValue<TObjectPtr<UGameUserSettings>> EngineSettings(GEngine->GameUserSettings,Settings);
    Settings->SetScreenResolution(FIntPoint(1280,720));Settings->SetFullscreenMode(EWindowMode::Windowed);
    Settings->ConfirmVideoMode();Settings->SaveSettings();
    int32 Requests=0;
    // Keep this automated check from resizing the user's real game/editor window.
    FDisplaySettingsPreview Preview(Settings,[&Requests]{++Requests;});
    TestFalse(TEXT("Invalid resolution never reaches the display backend"),Preview.Begin(FIntPoint(0,1080),EWindowMode::Fullscreen,100));
    TestFalse(TEXT("Unchanged display settings do not start a preview"),Preview.Begin(FIntPoint(1280,720),EWindowMode::Windowed,100));
    TestEqual(TEXT("No unnecessary display requests"),Requests,0);
    TestTrue(TEXT("Resolution and mode can be previewed together"),Preview.Begin(FIntPoint(1920,1080),EWindowMode::Fullscreen,100));
    TestFalse(TEXT("A pending preview cannot replace its rollback state"),Preview.Begin(FIntPoint(2560,1440),EWindowMode::Windowed,101));
    TestEqual(TEXT("Resolution applies for the preview"),Settings->GetScreenResolution(),FIntPoint(1920,1080));
    auto* Saved=NewObject<UHeavensDivideGameUserSettings>();Saved->LoadConfig(nullptr,*NativeIni);
    TestEqual(TEXT("Unconfirmed resolution is not persisted"),Saved->GetScreenResolution(),FIntPoint(1280,720));
    Preview.Update(114.99);
    TestTrue(TEXT("Full fifteen real-time seconds are available"),Preview.IsPending());
    Preview.Update(115.01);
    TestFalse(TEXT("Unconfirmed change expires"),Preview.IsPending());
    TestEqual(TEXT("Timeout restores original resolution"),Settings->GetScreenResolution(),FIntPoint(1280,720));
    TestEqual(TEXT("Timeout restores original window mode"),Settings->GetFullscreenMode(),EWindowMode::Windowed);
    TestEqual(TEXT("Apply and automatic rollback both request the backend"),Requests,2);
    Preview.Begin(FIntPoint(1600,900),EWindowMode::Windowed,200);Preview.Cancel();
    TestEqual(TEXT("Manual Revert restores the previous resolution"),Settings->GetScreenResolution(),FIntPoint(1280,720));
    Preview.Begin(FIntPoint(1600,900),EWindowMode::Fullscreen,300);Preview.Keep();
    int32 StoredWidth=0;
    TestTrue(TEXT("Keep serializes resolution to the native user config"),GConfig->GetInt(TEXT("/Script/HeavensDivide.HeavensDivideGameUserSettings"),TEXT("ResolutionSizeX"),StoredWidth,NativeIni));
    TestEqual(TEXT("Serialized resolution matches the kept choice"),StoredWidth,1600);
    Saved=NewObject<UHeavensDivideGameUserSettings>();
    Saved->LoadConfig(nullptr,*NativeIni);
    TestEqual(TEXT("Kept resolution survives reload"),Saved->GetScreenResolution(),FIntPoint(1600,900));
    TestEqual(TEXT("Kept window mode survives reload"),Saved->GetFullscreenMode(),EWindowMode::Fullscreen);
    TestEqual(TEXT("Kept resolution is user-confirmed"),Saved->GetLastConfirmedScreenResolution(),FIntPoint(1600,900));
    Preview.Update(1000);
    TestEqual(TEXT("Kept change cannot later time out"),Settings->GetScreenResolution(),FIntPoint(1600,900));
    { FDisplaySettingsPreview Removed(Settings,[&Requests]{++Requests;});Removed.Begin(FIntPoint(1600,900),EWindowMode::Windowed,400); }
    TestEqual(TEXT("Removing a preview restores the kept display mode"),Settings->GetFullscreenMode(),EWindowMode::Fullscreen);

    auto* World=UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->SetGameInstance(NewObject<UGameInstance>(GEngine));World->InitializeActorsForPlay(FURL());
    auto* PC=World->SpawnActor<APlayerController>();PC->SetPlayer(NewObject<ULocalPlayer>(GEngine));
    auto* Class=LoadClass<UMainMenuWidget>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/UI/MainMenu/WBP_MainMenu.WBP_MainMenu_C"));
    auto* Menu=CreateWidget<UMainMenuWidget>(PC,Class);
    FindFProperty<FObjectProperty>(UMainMenuWidget::StaticClass(),TEXT("BackgroundMediaPlayer"))->SetObjectPropertyValue_InContainer(Menu,nullptr);
    const auto MenuSlate=Menu->TakeWidget();Menu->ShowSettingsPanel();
    auto* Host=Cast<UNativeWidgetHost>(Menu->GetWidgetFromName(TEXT("DisplaySettingsHost")));
    TestNotNull(TEXT("Saved menu hosts the display page"),Host);
    auto* Button=Cast<UNativeWidgetHost>(Menu->GetWidgetFromName(TEXT("DisplaySettingsButton")));
    if(TestNotNull(TEXT("Display action appears in Settings"),Button) && TestTrue(TEXT("Display action retains its Slate content"),Button->GetContent().IsValid()))
    {
        auto Action=StaticCastSharedRef<SButton>(Button->GetContent().ToSharedRef());
        Action->OnKeyDown(FGeometry(),FKeyEvent(EKeys::SpaceBar,FModifierKeysState(),0,false,0,0));
        Action->OnKeyUp(FGeometry(),FKeyEvent(EKeys::SpaceBar,FModifierKeysState(),0,false,0,0));
        auto* Switcher=Cast<UWidgetSwitcher>(Menu->GetWidgetFromName(TEXT("SettingsSubpages")));
        TestEqual(TEXT("Display action opens its subpage"),Switcher->GetActiveWidgetIndex(),2);
        Menu->ShowSettingsPanel();TestEqual(TEXT("Returning restores General Settings"),Switcher->GetActiveWidgetIndex(),0);
    }
    World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return !HasAnyErrors();
}

#if WITH_EDITOR
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "TextureResource.h"
#include "ImageUtils.h"
#include "RenderingThread.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDisplaySettingsVisualTest,"HeavensDivide.UI.DisplaySettingsVisual",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FDisplaySettingsVisualTest::RunTest(const FString&)
{
    FWidgetRenderer Renderer(false);
    auto View=SNew(SDisplaySettingsView).PageActive(true);
    auto Panel=SNew(SBorder).Padding(48).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(BloodshiftMenu::Ink)[View];
    const FString Dir=FPaths::ProjectSavedDir()/TEXT("DisplaySettings/Previews");IFileManager::Get().MakeDirectory(*Dir,true);
    for(FIntPoint Size:{FIntPoint(1280,720),FIntPoint(1920,1080)})
    {
        auto* Target=Renderer.DrawWidget(Panel,FVector2D(Size));
        for(int32 Pass=0;Pass<3;++Pass){++GFrameCounter;Renderer.DrawWidget(Target,Panel,FVector2D(Size),.016f);FlushRenderingCommands();}
        TArray<FColor> Pixels;
        if(!TestTrue(TEXT("Display page renders"),Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels)))continue;
        for(FColor& P:Pixels)P.A=255;
        TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,PNG);
        TestTrue(TEXT("Resolution settings preview saved"),FFileHelper::SaveArrayToFile(PNG,*(Dir/FString::Printf(TEXT("Display_%dx%d.png"),Size.X,Size.Y))));
    }
    return !HasAnyErrors();
}
#endif
