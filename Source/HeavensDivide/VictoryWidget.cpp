#include "VictoryWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet/GameplayStatics.h"
#include "RunTravelSubsystem.h"

void UVictoryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	BuildVictoryScreen();
}

void UVictoryWidget::BuildVictoryScreen()
{
	if (!WidgetTree || WidgetTree->RootWidget) return;

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("VictoryRoot"));
	WidgetTree->RootWidget = Root;
	UBorder* ScreenFade = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("VictoryScreenFade"));
	ScreenFade->SetBrushColor(FLinearColor(0.005f, 0.008f, 0.014f, 0.86f));
	Root->AddChildToOverlay(ScreenFade);

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("VictoryPanel"));
	Panel->SetBrushColor(FLinearColor(0.025f, 0.045f, 0.035f, 0.98f));
	Panel->SetPadding(FMargin(64.0f, 42.0f));
	UOverlaySlot* PanelSlot = Root->AddChildToOverlay(Panel);
	PanelSlot->SetHorizontalAlignment(HAlign_Center);
	PanelSlot->SetVerticalAlignment(VAlign_Center);

	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VictoryStack"));
	Panel->SetContent(Stack);
	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("VictoryTitle"));
	Title->SetText(FText::FromString(TEXT("VICTORY")));
	Title->SetJustification(ETextJustify::Center);
	Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.90f, 0.78f, 0.30f)));
	FSlateFontInfo TitleFont = Title->GetFont();
	TitleFont.Size = 48;
	Title->SetFont(TitleFont);
	Stack->AddChildToVerticalBox(Title)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 30.0f));

	NewRunButton = AddActionButton(Stack, FText::FromString(TEXT("NEW RUN")), TEXT("NewRunButton"));
	UButton* MainMenuButton = AddActionButton(Stack, FText::FromString(TEXT("BACK TO MENU")), TEXT("MainMenuButton"));
	NewRunButton->OnClicked.AddDynamic(this, &UVictoryWidget::HandleNewRun);
	MainMenuButton->OnClicked.AddDynamic(this, &UVictoryWidget::HandleMainMenu);
}

UButton* UVictoryWidget::AddActionButton(UVerticalBox* Parent, const FText& Label, FName WidgetName)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), WidgetName);
	Button->SetBackgroundColor(FLinearColor(0.13f, 0.10f, 0.15f, 1.0f));
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetText(Label);
	Text->SetJustification(ETextJustify::Center);
	Text->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	FSlateFontInfo Font = Text->GetFont();
	Font.Size = 22;
	Text->SetFont(Font);
	Button->AddChild(Text);
	Parent->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.0f, 5.0f));
	return Button;
}

void UVictoryWidget::FocusInitialButton()
{
	if (NewRunButton) NewRunButton->SetKeyboardFocus();
}

void UVictoryWidget::PrepareForTravel()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (URunTravelSubsystem* TravelState = GameInstance->GetSubsystem<URunTravelSubsystem>()) TravelState->ClearSnapshot();
	}
	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::SetGamePaused(World, false);
		UGameplayStatics::SetGlobalTimeDilation(World, 1.0f);
	}
}

void UVictoryWidget::HandleNewRun()
{
	if (bTravelRequested) return;
	bTravelRequested = true;
	PrepareForTravel();
	UGameplayStatics::OpenLevel(this, TEXT("Lvl_B1_Lvl1"));
}

void UVictoryWidget::HandleMainMenu()
{
	if (bTravelRequested) return;
	bTravelRequested = true;
	PrepareForTravel();
	UGameplayStatics::OpenLevel(this, TEXT("Lvl_MainMenu"));
}
