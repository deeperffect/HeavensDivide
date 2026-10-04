#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "MenuFeedbackWidget.h"
#include "MetaSkillTreeWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/WidgetSwitcher.h"
#include "Components/InputKeySelector.h"
#include "Widgets/Input/SButton.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMenuFeedbackTest, "HeavensDivide.UI.MenuFeedback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMenuFeedbackTest::RunTest(const FString&)
{
    auto* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->SetGameInstance(NewObject<UGameInstance>(GEngine));
    World->SetGameMode(FURL()); World->InitializeActorsForPlay(FURL());
    auto* PC = World->SpawnActor<APlayerController>();
    auto* Player = NewObject<ULocalPlayer>(GEngine); Player->SetControllerId(0); PC->SetPlayer(Player);
    PC->PlayerState = World->SpawnActor<APlayerState>();
    for (const TCHAR* Path : {TEXT("/Game/HeavensDivide/Blueprints/UI/MainMenu/WBP_MainMenu.WBP_MainMenu_C"),
        TEXT("/Game/HeavensDivide/Blueprints/UI/UpgradeUI/WBP_LevelUp.WBP_LevelUp_C"),
        TEXT("/Game/HeavensDivide/Blueprints/UI/WBP_GameOver.WBP_GameOver_C")})
    {
        UClass* Class = LoadClass<UMenuFeedbackWidget>(nullptr, Path);
        if (TestNotNull(TEXT("Saved menu inherits shared feedback"), Class))
            TestTrue(TEXT("Saved menu allows feedback ticking"), Class->GetDefaultObject<UMenuFeedbackWidget>()->GetDesiredTickFrequency() == EWidgetTickFrequency::Auto);
    }
    auto* Menu = CreateWidget<UMenuFeedbackWidget>(PC);
    auto* Switcher = Menu->WidgetTree->ConstructWidget<UWidgetSwitcher>();
    Menu->WidgetTree->RootWidget = Switcher;
    auto* Panel = Menu->WidgetTree->ConstructWidget<UVerticalBox>(); Switcher->AddChild(Panel);
    Switcher->AddChild(Menu->WidgetTree->ConstructWidget<UVerticalBox>());
    auto AddButton = [&]()
    {
        auto* Button = Menu->WidgetTree->ConstructWidget<UButton>();
        auto* Text = Menu->WidgetTree->ConstructWidget<UTextBlock>(); Text->SetText(FText::FromString(TEXT("TEST BUTTON")));
        Button->SetContent(Text); Panel->AddChild(Button); return Button;
    };
    auto* Button = AddButton();
    auto Root = Menu->TakeWidget(); Root->SlatePrepass(1.f);
    Menu->DiscoverButtons();
    TestEqual(TEXT("UMG button discovered through Slate tree"), Menu->Buttons.Num(), 1);
    TestNotNull(TEXT("Hover MetaSound loads"), Menu->LoadedHoverSound.Get());
    TestNotNull(TEXT("Press MetaSound loads"), Menu->LoadedPressSound.Get());
    const FVector2D DesiredSize = Button->GetDesiredSize();
    auto SlateButton = StaticCastSharedRef<SButton>(Button->TakeWidget());
    const FPointerEvent PointerEvent;
    SlateButton->OnMouseEnter(FGeometry(), PointerEvent); Menu->UpdateFeedback(.3f);
    if (Menu->Buttons.Num()) TestTrue(TEXT("Actual mouse hover enlarges the button"), Menu->Buttons[0].Scale > 1.049f);
    SlateButton->OnMouseLeave(PointerEvent);
    const FKeyEvent Accept(EKeys::Enter, FModifierKeysState(), 0, false, 0, 0);
    SlateButton->OnKeyDown(FGeometry(), Accept);
    if (Menu->Buttons.Num()) TestTrue(TEXT("Slate press delegate gives an immediate pop"), FMath::IsNearlyEqual(Menu->Buttons[0].Scale, 1.08f));
    SlateButton->OnKeyUp(FGeometry(), Accept);
    Menu->UpdateFeedback(.2f);
    Menu->SetMenuFocusedButton(Button); Menu->UpdateFeedback(.3f);
    if (Menu->Buttons.Num())
    {
        TestTrue(TEXT("Controller selection smoothly enlarges"), Menu->Buttons[0].Scale > 1.049f);
        Root->SlatePrepass(1.f);
        TestTrue(TEXT("Feedback preserves layout size"), Button->GetDesiredSize().Equals(DesiredSize));
        // This isolated world has no game-instance player list; pause through its controller.
        PC->SetPause(true);
        TestTrue(TEXT("Test world is actually paused"), UGameplayStatics::IsGamePaused(World));
        Menu->PressMenuFocusedButton();
        TestTrue(TEXT("Press pops immediately while paused"), FMath::IsNearlyEqual(Menu->Buttons[0].Scale, 1.08f));
        Menu->UpdateFeedback(.2f); Menu->UpdateFeedback(.3f);
        TestTrue(TEXT("Press settles back to hover size"), FMath::IsNearlyEqual(Menu->Buttons[0].Scale, 1.05f, .001f));
        Button->SetIsEnabled(false); Menu->UpdateFeedback(.01f);
        TestEqual(TEXT("Disabled button resets immediately"), Menu->Buttons[0].Scale, 1.f);
        Button->SetIsEnabled(true); Menu->UpdateFeedback(.3f);
        Switcher->SetActiveWidgetIndex(1); Menu->DiscoverButtons(); Menu->UpdateFeedback(.01f);
        TestEqual(TEXT("Inactive switcher page does not retain focus scale"), Menu->Buttons[0].Scale, 1.f);
        Switcher->SetActiveWidgetIndex(0);
    }
    auto* Dynamic = AddButton(); Dynamic->TakeWidget(); Menu->DiscoverButtons(); Menu->DiscoverButtons();
    TestEqual(TEXT("New buttons discovered without duplicate registration"), Menu->Buttons.Num(), 2);
    TestTrue(TEXT("Dynamic button has immediate press feedback"), Dynamic->OnPressed.IsBound());
    auto* KeySelector = Menu->WidgetTree->ConstructWidget<UInputKeySelector>();
    Panel->AddChild(KeySelector); KeySelector->TakeWidget(); Menu->DiscoverButtons();
    TestEqual(TEXT("Keybinding selector's internal button is covered"), Menu->Buttons.Num(), 3);
    auto* Tree = CreateWidget<UMetaSkillTreeWidget>(PC);
    auto TreeRoot = Tree->TakeWidget();
    UMenuFeedbackWidget* TreeFeedback = Tree;
    TreeFeedback->DiscoverButtons();
    TestEqual(TEXT("Skill tree's four native Slate action buttons are covered"), TreeFeedback->Buttons.Num(), 4);
    TreeFeedback->NativeDestruct();
    Menu->NativeDestruct();
    TestFalse(TEXT("Teardown removes feedback callbacks"), Dynamic->OnPressed.IsBound());
    TestEqual(TEXT("Teardown clears state"), Menu->Buttons.Num(), 0);
    World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
    return true;
}
#endif
