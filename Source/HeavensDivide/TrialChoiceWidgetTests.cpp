#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "TrialChoiceWidget.h"
#include "GameOverWidget.h"
#include "VictoryWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "MainMenuWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Layout/ArrangedChildren.h"
#include "Widgets/SWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrialChoiceLayoutTest, "HeavensDivide.UI.TrialChoiceLayout",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTrialChoiceLayoutTest::RunTest(const FString&)
{
    auto* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    World->SetGameInstance(NewObject<UGameInstance>(GEngine));
    auto* PC = World->SpawnActor<APlayerController>();
    auto* Player = NewObject<ULocalPlayer>(GEngine);
    Player->SetControllerId(0);
    PC->SetPlayer(Player);
    auto* DeathClass = LoadClass<UGameOverWidget>(nullptr, TEXT("/Game/HeavensDivide/Blueprints/UI/WBP_GameOver.WBP_GameOver_C"));
    TestNotNull(TEXT("Saved death screen class"), DeathClass);
    for (UClass* Class : {UTrialChoiceWidget::StaticClass(), DeathClass ? DeathClass : UGameOverWidget::StaticClass(), UVictoryWidget::StaticClass()})
    {
    auto* Choice = CreateWidget<UUserWidget>(PC, Class);
    if (TestNotNull(TEXT("Menu-styled prompt builds"), Choice))
    {
        Choice->TakeWidget();
        const bool bTrial = Choice->IsA<UTrialChoiceWidget>();
        const bool bDeath = Choice->IsA<UGameOverWidget>();
        for (const TCHAR* Name : {bTrial ? TEXT("SamuraiTrialButton") : bDeath ? TEXT("RestartRunButton") : TEXT("NewRunButton"), bTrial ? TEXT("NinjaTrialButton") : TEXT("MainMenuButton")})
        {
            auto* Button = Cast<UButton>(Choice->GetWidgetFromName(Name));
            if (TestNotNull(TEXT("Action button exists"), Button))
                TestTrue(TEXT("Action retains its click handler"), Button->OnClicked.IsBound());
        }
        if (auto* Death = Cast<UGameOverWidget>(Choice))
        {
            Death->InitializeGameOver(nullptr, 125.f);
            auto* Time = Cast<UTextBlock>(Death->GetWidgetFromName(TEXT("FinalRunTimeText")));
            if (TestNotNull(TEXT("Death screen keeps run time"), Time))
                TestEqual(TEXT("Final run time updates"), Time->GetText().ToString(), FString(TEXT("02:05")));
        }
        auto* Background = Cast<UImage>(Choice->GetWidgetFromName(TEXT("MenuPromptBackground")));
        auto* MenuClass = LoadClass<UMainMenuWidget>(nullptr, TEXT("/Game/HeavensDivide/Blueprints/UI/MainMenu/WBP_MainMenu.WBP_MainMenu_C"));
        if (TestNotNull(TEXT("Background image exists"), Background) && TestNotNull(TEXT("Authored menu exists"), MenuClass))
        {
            TestTrue(TEXT("Background uses the authored menu texture"), Background->GetBrush().GetResourceObject()
                && Background->GetBrush().GetResourceObject() == MenuClass->GetDefaultObject<UMainMenuWidget>()->GetPageBackgroundTexture());
            // Arrange the actual Slate hierarchy at two viewport sizes, rather than only checking slot settings.
            auto Root = Choice->WidgetTree->RootWidget->TakeWidget();
            Root->SlatePrepass(1.0f);
            for (FVector2D Viewport : {FVector2D(1920, 1080), FVector2D(640, 360)})
            {
                bool bFound = false;
                TFunction<void(TSharedRef<SWidget>, const FGeometry&)> Inspect;
                Inspect = [&](TSharedRef<SWidget> Widget, const FGeometry& Geometry)
                {
                    if (Widget == Background->GetCachedWidget())
                    {
                        bFound = true;
                        const FVector2D Size = Geometry.GetLocalSize();
                        const FVector2D Center = Geometry.LocalToAbsolute(Size * 0.5f);
                        TestTrue(TEXT("Panel art has visible area"), Size.X > 100 && Size.Y > 100);
                        TestTrue(TEXT("Panel art is centered in the viewport"), Center.Equals(Viewport * 0.5f, 1.0f));
                        const FVector2D End = Geometry.LocalToAbsolute(Size);
                        TestTrue(TEXT("Panel fits inside viewport"), End.X <= Viewport.X && End.Y <= Viewport.Y);
                    }
                    FArrangedChildren Children(EVisibility::Visible);
                    Widget->ArrangeChildren(Geometry, Children);
                    for (int32 Index = 0; Index < Children.Num(); ++Index)
                        Inspect(Children[Index].Widget, Children[Index].Geometry);
                };
                Inspect(Root, FGeometry::MakeRoot(Viewport, FSlateLayoutTransform()));
                TestTrue(TEXT("Panel art participates in visible layout"), bFound);
            }
        }
    }
    }
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}
#endif
