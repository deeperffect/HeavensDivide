#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/SWidget.h"
#include "ImageUtils.h"
#include "SynergyMetaProgressionSubsystem.h"
#include "PlayerHUDWidget.h"
#include "CurrencyHUDWidget.h"
#include "SurvivorPlayerController.h"
#include "FinalBossBase.h"
#include "GameOverWidget.h"
#include "VictoryWidget.h"
#if WITH_EDITOR
#include "AssetCompilingManager.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRunCurrencyVisualTest,"HeavensDivide.UI.RunCurrencyRewards",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FRunCurrencyVisualTest::RunTest(const FString&)
{
    const FString Slot = TEXT("Automation_RunCurrencyVisual_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString PreviousCommandLine = FCommandLine::Get();
    FCommandLine::Set(*(TEXT("-MetaSaveSlot=") + Slot + TEXT(" ") + PreviousCommandLine));
    auto* GI = NewObject<UGameInstance>(GEngine);
    GI->InitializeStandalone();
    FCommandLine::Set(*PreviousCommandLine);
    auto* World = GI->GetWorld();
    World->InitializeActorsForPlay(FURL());
    auto* Meta = GI->GetSubsystem<USynergyMetaProgressionSubsystem>();
    Meta->BeginCurrencyRun();
    auto* PC = World->SpawnActor<ASurvivorPlayerController>();
    PC->SetPlayer(NewObject<ULocalPlayer>(GEngine));
    auto* HUDClass = LoadClass<UPlayerHUDWidget>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/UI/WBP_PlayerHUD.WBP_PlayerHUD_C"));
    auto* HUD = HUDClass ? CreateWidget<UPlayerHUDWidget>(PC,HUDClass) : nullptr;
    if (!TestNotNull(TEXT("Shipped HUD loads"),HUD))
    {
        GI->Shutdown(); World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
        UGameplayStatics::DeleteGameInSlot(Slot,0); return false;
    }
    HUD->InitializeFromPlayerController(PC);
    // The real viewport retains this widget. Keep the same ownership here so
    // NativeDestruct cannot remove the dynamic minimap/currency children mid-test.
    auto HUDSlate = HUD->TakeWidget();
    auto* Currency = Cast<UCurrencyHUDWidget>(HUD->WidgetTree->FindWidget(TEXT("RuntimeCurrencies")));
    if (!TestNotNull(TEXT("Currency HUD attaches to shipped layout"),Currency))
    {
        HUD->ReleaseSlateResources(true);
        GI->Shutdown(); World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
        UGameplayStatics::DeleteGameInSlot(Slot,0); return false;
    }
    TestEqual(TEXT("Run currencies stay visible during normal survival"),Currency->GetVisibility(),ESlateVisibility::HitTestInvisible);
#if WITH_EDITOR
    FAssetCompilingManager::Get().FinishAllCompilation();
#endif
    const FString Dir = FPaths::ProjectSavedDir()/TEXT("ProgressionExpansion/Previews");
    IFileManager::Get().MakeDirectory(*Dir,true);
    auto Render = [&](UUserWidget* Widget,FVector2D Size,const TCHAR* Name)
    {
        FWidgetRenderer Renderer(false);
        auto Slate = Widget->TakeWidget();
        auto* Target = Renderer.DrawWidget(Slate,Size);
        TArray<FColor> Pixels;
        if (!TestNotNull(TEXT("Reward preview render target"),Target)) return Pixels;
        for (int32 I=0;I<4;++I)
        {
            Slate->Invalidate(EInvalidateWidgetReason::Prepass);
            Widget->ForceLayoutPrepass();
            Renderer.DrawWidget(Target,Slate,Size,1.f/60.f);
        }
        FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
        if (TestTrue(TEXT("Reward preview pixels readable"),Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags)))
        {
            TArray64<uint8> PNG;
            FImageUtils::PNGCompressImageArray(Target->SizeX,Target->SizeY,Pixels,PNG);
            TestTrue(TEXT("Reward preview saved"),FFileHelper::SaveArrayToFile(PNG,*(Dir/Name)));
        }
        return Pixels;
    };
    auto TickCurrency = [&]()
    {
        Currency->TakeWidget()->Tick(FGeometry::MakeRoot(FVector2D(250,64),FSlateLayoutTransform()),FPlatformTime::Seconds(),.25f);
    };
    TickCurrency();
    const auto Before = Render(Currency,{250,64},TEXT("RunSpoilsEmpty.png"));
    Meta->AwardCurrencies(120,4);
    TestEqual(TEXT("HUD earnings include pending Ember deposits"),Meta->GetRunEmbers(),120);
    TestEqual(TEXT("HUD earnings include pending Sigil deposits"),Meta->GetRunSigils(),4);
    TickCurrency();
    const auto After = Render(Currency,{250,64},TEXT("RunSpoilsGained.png"));
    int32 Changed = 0;
    for (int32 I=0;I<FMath::Min(Before.Num(),After.Num());++I) if (Before[I] != After[I]) ++Changed;
    TestTrue(TEXT("Credited currencies repaint run totals and gain text"),Changed>100);
    Render(HUD,{1920,1080},TEXT("RunSpoilsHUD1920.png"));
    Render(HUD,{1280,720},TEXT("RunSpoilsHUD1280.png"));

    auto* GameOver = CreateWidget<UGameOverWidget>(PC);
    GameOver->InitializeGameOver(PC,395);
    auto* Victory = CreateWidget<UVictoryWidget>(PC);
    for (UUserWidget* EndScreen : {static_cast<UUserWidget*>(GameOver),static_cast<UUserWidget*>(Victory)})
    {
        auto* Text = Cast<UTextBlock>(EndScreen->WidgetTree->FindWidget(TEXT("SoulEmberReward")));
        if (TestNotNull(TEXT("Endscreen has spoils text"),Text))
        {
            const FString Value = Text->GetText().ToString();
            TestTrue(TEXT("Endscreen shows all run Embers"),Value.Contains(TEXT("+120 EMBERS")));
            TestTrue(TEXT("Endscreen shows all run Sigils"),Value.Contains(TEXT("+4 SOUL SIGILS")));
        }
    }
    Render(GameOver,{1920,1080},TEXT("RunSpoilsDefeat1920.png"));
    Render(GameOver,{1280,720},TEXT("RunSpoilsDefeat1280.png"));
    Render(Victory,{1920,1080},TEXT("RunSpoilsVictory1920.png"));
    Render(Victory,{1280,720},TEXT("RunSpoilsVictory1280.png"));
    auto* Boss = World->SpawnActor<AFinalBossBase>();
    HUD->ShowBossHealthBar(Boss);
    TestEqual(TEXT("Boss encounter hides detached minimap currency panel"),Currency->GetVisibility(),ESlateVisibility::Collapsed);
    HUD->HideBossHealthBar(Boss);
    HUD->ReleaseSlateResources(true);
    GameOver->ReleaseSlateResources(true); Victory->ReleaseSlateResources(true);
    GI->Shutdown(); World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
    UGameplayStatics::DeleteGameInSlot(Slot,0);
    return true;
}
#endif
