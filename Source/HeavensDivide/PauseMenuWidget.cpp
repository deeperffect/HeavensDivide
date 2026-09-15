#include "PauseMenuWidget.h"
#include "MainMenuWidget.h"
#include "SurvivorPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UObject/ConstructorHelpers.h"

UPauseMenuWidget::UPauseMenuWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    static ConstructorHelpers::FClassFinder<UMainMenuWidget> Menu(TEXT("/Game/HeavensDivide/Blueprints/UI/MainMenu/WBP_MainMenu"));
    SettingsClass = Menu.Succeeded() ? Menu.Class.Get() : UMainMenuWidget::StaticClass();
}

void UPauseMenuWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(true);
    auto* Stack = BuildPromptPanel(TEXT("PauseTitle"), FText::FromString(TEXT("PAUSED")), FVector2D(900, 600));
    if (!Stack) return;
    auto AddAction = [&](const TCHAR* Label, const TCHAR* Name)
    {
        auto* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
        PromptActions->AddChildToVerticalBox(Row)->SetHorizontalAlignment(HAlign_Center);
        return AddPromptButton(Row, FText::FromString(Label), Name);
    };
    ResumeButton = AddAction(TEXT("RESUME"), TEXT("ResumeButton"));
    ResumeButton->OnClicked.AddDynamic(this, &UPauseMenuWidget::Resume);
    AddAction(TEXT("SETTINGS"), TEXT("SettingsButton"))->OnClicked.AddDynamic(this, &UPauseMenuWidget::Settings);
    AddAction(TEXT("RETURN TO MAIN MENU"), TEXT("MainMenuButton"))->OnClicked.AddDynamic(this, &UPauseMenuWidget::MainMenu);
}
void UPauseMenuWidget::FocusResume() { if (ResumeButton) ResumeButton->SetUserFocus(GetOwningPlayer()); }
void UPauseMenuWidget::Resume() { if (auto* PC = Cast<ASurvivorPlayerController>(GetOwningPlayer())) PC->ResumePausedRun(); }
void UPauseMenuWidget::MainMenu() { if (auto* PC = Cast<ASurvivorPlayerController>(GetOwningPlayer())) PC->ReturnToMenuFromPause(); }
void UPauseMenuWidget::Settings()
{
    if (SettingsWidget) return;
    SettingsWidget = CreateWidget<UMainMenuWidget>(GetOwningPlayer(), SettingsClass);
    if (!SettingsWidget) return;
    SettingsWidget->OpenInRunSettings(FSimpleDelegate::CreateUObject(this, &UPauseMenuWidget::CloseSettings));
    SetVisibility(ESlateVisibility::Hidden);
    SettingsWidget->AddToViewport(301);
    SettingsWidget->ShowSettingsPanel();
}
void UPauseMenuWidget::CloseSettings()
{
    if (SettingsWidget) { SettingsWidget->RemoveFromParent(); SettingsWidget = nullptr; }
    SetVisibility(ESlateVisibility::Visible);
    FocusResume();
}
void UPauseMenuWidget::NativeDestruct()
{
    if (SettingsWidget) { SettingsWidget->RemoveFromParent(); SettingsWidget = nullptr; }
    Super::NativeDestruct();
}
FReply UPauseMenuWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (Event.GetKey() == EKeys::Escape || Event.GetKey() == EKeys::Gamepad_FaceButton_Right)
    {
        if (!Event.IsRepeat()) Resume();
        return FReply::Handled();
    }
    return Super::NativeOnPreviewKeyDown(Geometry, Event);
}
