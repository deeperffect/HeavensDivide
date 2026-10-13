#include "DisplaySettingsView.h"
#include "BloodshiftMenuStyle.h"
#include "MenuDropdownStyle.h"
#include "HeavensDivideGameUserSettings.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/Application/SlateApplication.h"

namespace
{
    const FButtonStyle& DisplayAction() { static const auto Style=BloodshiftMenu::ActionStyle();return Style; }
    const FSlateBrush* DisplayRule() { static const auto Brush=BloodshiftMenu::Solid(BloodshiftMenu::Rule);return &Brush; }
    FText ResolutionText(FIntPoint R) { return FText::FromString(FString::Printf(TEXT("%d x %d"),R.X,R.Y)); }
    FText ModeText(EWindowMode::Type Mode)
    { return FText::FromString(Mode==EWindowMode::Fullscreen?TEXT("Fullscreen"):Mode==EWindowMode::WindowedFullscreen?TEXT("Borderless"):TEXT("Windowed")); }
    TSharedRef<STextBlock> Label(const FText& Text,int32 Size=21)
    { return SNew(STextBlock).Text(Text).Font(BloodshiftMenu::BodyFont(Size)).ColorAndOpacity(BloodshiftMenu::Paper); }
}

FDisplaySettingsPreview::FDisplaySettingsPreview(UGameUserSettings* InSettings,TFunction<void()> InApply):Settings(InSettings),Apply(MoveTemp(InApply)) {}
FDisplaySettingsPreview::~FDisplaySettingsPreview() { Cancel(); }
bool FDisplaySettingsPreview::Begin(FIntPoint Resolution,EWindowMode::Type Mode,double Now)
{
    if (!Settings.IsValid() || bPending || Resolution.X<=0 || Resolution.Y<=0 || !FMath::IsFinite(Now)
        || (Mode!=EWindowMode::Windowed && Mode!=EWindowMode::WindowedFullscreen && Mode!=EWindowMode::Fullscreen)) return false;
    OriginalResolution=Settings->GetScreenResolution();OriginalMode=Settings->GetFullscreenMode();
    if (OriginalResolution==Resolution && OriginalMode==Mode) return false;
    Deadline=Now+15; bPending=true;
    Settings->SetScreenResolution(Resolution);Settings->SetFullscreenMode(Mode);Apply();return true;
}
void FDisplaySettingsPreview::Keep()
{
    if (!bPending) return;
    bPending=false;
    if(Settings.IsValid()) { Settings->ConfirmVideoMode();Settings->SaveSettings(); }
}
void FDisplaySettingsPreview::Cancel()
{
    if (!bPending) return;
    bPending=false;
    if(Settings.IsValid())
    { Settings->SetScreenResolution(OriginalResolution);Settings->SetFullscreenMode(OriginalMode);Apply();Settings->ConfirmVideoMode();Settings->SaveSettings(); }
}
void FDisplaySettingsPreview::Update(double Now) { if(bPending && Now>=Deadline)Cancel(); }
int32 FDisplaySettingsPreview::SecondsLeft(double Now) const { return bPending?FMath::Max(0,FMath::CeilToInt(float(Deadline-Now))):0; }

void SDisplaySettingsView::Construct(const FArguments& Args)
{
    World=Args._World;PageActive=Args._PageActive;
    Settings=UHeavensDivideGameUserSettings::GetHeavensDivideGameUserSettings();
    Preview=MakeUnique<FDisplaySettingsPreview>(Settings.Get(),[Weak=Settings] { if(Weak.IsValid())Weak->ApplyResolutionSettings(false); });
    Modes={MakeShared<EWindowMode::Type>(EWindowMode::Windowed),MakeShared<EWindowMode::Type>(EWindowMode::WindowedFullscreen),MakeShared<EWindowMode::Type>(EWindowMode::Fullscreen)};
    auto Rows=SNew(SVerticalBox);
    Rows->AddSlot().AutoHeight()[Label(FText::FromString(TEXT("DISPLAY")),32)];
    Rows->AddSlot().AutoHeight().Padding(0,16,0,30)[SNew(SBox).HeightOverride(1)[SNew(SBorder).BorderImage(DisplayRule())]];
    auto Controls=SNew(SVerticalBox).IsEnabled_Lambda([this]{return !Preview->IsPending();});
    Controls->AddSlot().AutoHeight().Padding(0,8,0,22)[SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Label(FText::FromString(TEXT("Window mode")))]
        +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(340)[SAssignNew(ModePicker,SComboBox<TSharedPtr<EWindowMode::Type>>)
            .OptionsSource(&Modes).ComboBoxStyle(&MenuDropdown::BoxStyle()).HasDownArrow(false).ContentPadding(FMargin(16,8))
            .OnGenerateWidget_Lambda([](TSharedPtr<EWindowMode::Type> M){return Label(ModeText(*M),19);})
            .OnSelectionChanged_Lambda([this](TSharedPtr<EWindowMode::Type> M,ESelectInfo::Type){if(M && !bRefreshing){SelectedMode=*M;RebuildResolutions();}})
            [SNew(SHorizontalBox)
             +SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Text_Lambda([this]{return ModeText(SelectedMode);}).Font(BloodshiftMenu::BodyFont(19))]
             +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(16,0,0,0)[SNew(SMenuDropdownArrow)]]]]];
    Controls->AddSlot().AutoHeight().Padding(0,8,0,30)[SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Label(FText::FromString(TEXT("Resolution")))]
        +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(340)[SAssignNew(ResolutionPicker,SComboBox<TSharedPtr<FIntPoint>>)
            .OptionsSource(&Resolutions).ComboBoxStyle(&MenuDropdown::BoxStyle()).HasDownArrow(false).ContentPadding(FMargin(16,8))
            .IsEnabled_Lambda([this]{return SelectedMode!=EWindowMode::WindowedFullscreen;})
            .OnGenerateWidget_Lambda([](TSharedPtr<FIntPoint> R){return Label(ResolutionText(*R),19);})
            .OnSelectionChanged_Lambda([this](TSharedPtr<FIntPoint> R,ESelectInfo::Type){if(R && !bRefreshing)SelectedResolution=*R;})
            [SNew(SHorizontalBox)
             +SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Text_Lambda([this]{return SelectedMode==EWindowMode::WindowedFullscreen?FText::FromString(TEXT("Desktop")):ResolutionText(SelectedResolution);}).Font(BloodshiftMenu::BodyFont(19))]
             +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(16,0,0,0)[SNew(SMenuDropdownArrow)]]]]];
    Controls->AddSlot().AutoHeight()[SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth()[SNew(SButton).ButtonStyle(&DisplayAction())
        .IsEnabled_Lambda([this]{return CanApply() && Settings.IsValid() && (Settings->GetScreenResolution()!=SelectedResolution || Settings->GetFullscreenMode()!=SelectedMode);})
        .ToolTipText_Lambda([this]{return CanApply()?FText::GetEmpty():FText::FromString(TEXT("Change display settings in Standalone or a packaged game."));})
        .OnClicked_Lambda([this]{ApplySelected();return FReply::Handled();})[Label(FText::FromString(TEXT("APPLY")),18)]]];
    Rows->AddSlot().AutoHeight()[Controls];
    auto Confirm=SNew(SVerticalBox).Visibility_Lambda([this]{return Preview->IsPending()?EVisibility::Visible:EVisibility::Collapsed;});
    Confirm->AddSlot().AutoHeight().Padding(0,24,0,16)[SNew(STextBlock).Font(BloodshiftMenu::BodyFont(21)).ColorAndOpacity(BloodshiftMenu::Paper)
        .Text_Lambda([this]{return FText::FromString(FString::Printf(TEXT("Keep these settings? %ds"),Preview->SecondsLeft(FPlatformTime::Seconds())));})];
    Confirm->AddSlot().AutoHeight()[SNew(SHorizontalBox)
        +SHorizontalBox::Slot().AutoWidth().Padding(0,0,12,0)[SAssignNew(KeepButton,SButton).ButtonStyle(&DisplayAction()).OnClicked_Lambda([this]{Preview->Update(FPlatformTime::Seconds());Preview->Keep();Refresh();FocusInitial();return FReply::Handled();})[Label(FText::FromString(TEXT("KEEP")),18)]]
        +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).ButtonStyle(&DisplayAction()).OnClicked_Lambda([this]{Cancel();return FReply::Handled();})[Label(FText::FromString(TEXT("REVERT")),18)]]];
    Rows->AddSlot().AutoHeight()[Confirm];
    ChildSlot[Rows];Refresh();
}
SDisplaySettingsView::~SDisplaySettingsView() { if(PreviewTicker.IsValid())FTSTicker::GetCoreTicker().RemoveTicker(PreviewTicker); }
void SDisplaySettingsView::FocusInitial() { if(ModePicker && FSlateApplication::IsInitialized())FSlateApplication::Get().SetKeyboardFocus(ModePicker,EFocusCause::SetDirectly); }
bool SDisplaySettingsView::CloseOpenSelector()
{
    if(ResolutionPicker && ResolutionPicker->IsOpen()){ResolutionPicker->SetIsOpen(false);return true;}
    if(ModePicker && ModePicker->IsOpen()){ModePicker->SetIsOpen(false);return true;}
    return false;
}
bool SDisplaySettingsView::CanApply() const
{ return World.IsValid() && World->WorldType==EWorldType::Game && GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport; }
void SDisplaySettingsView::Refresh()
{
    if(!Settings.IsValid())return;
    TGuardValue<bool> Guard(bRefreshing,true);
    SelectedResolution=Settings->GetScreenResolution();SelectedMode=Settings->GetFullscreenMode();
    for(auto M:Modes)if(*M==SelectedMode)ModePicker->SetSelectedItem(M);
    RebuildResolutions();
}
void SDisplaySettingsView::RebuildResolutions()
{
    TGuardValue<bool> Guard(bRefreshing,true);
    const FIntPoint Desktop=Settings.IsValid()?Settings->GetDesktopResolution():FIntPoint(1920,1080);
    TArray<FIntPoint> Choices;
    if(SelectedMode==EWindowMode::WindowedFullscreen) { SelectedResolution=Desktop;Choices.Add(Desktop); }
    else
    {
        if(SelectedMode==EWindowMode::Fullscreen)UKismetSystemLibrary::GetSupportedFullscreenResolutions(Choices);
        else UKismetSystemLibrary::GetConvenientWindowedResolutions(Choices);
        if(Choices.IsEmpty() || SelectedMode==EWindowMode::Windowed)
            for(FIntPoint R:{FIntPoint(1280,720),FIntPoint(1366,768),FIntPoint(1600,900),FIntPoint(1920,1080),FIntPoint(2560,1440),FIntPoint(3840,2160)})
                if(R.X<=Desktop.X && R.Y<=Desktop.Y)Choices.AddUnique(R);
        Choices.AddUnique(Desktop);
        if(SelectedResolution.X>0 && SelectedResolution.Y>0)Choices.AddUnique(SelectedResolution);
        Choices.RemoveAll([](FIntPoint R){return R.X<640 || R.Y<360;});
        Choices.Sort([](FIntPoint A,FIntPoint B){return A.X==B.X?A.Y<B.Y:A.X<B.X;});
        if(SelectedResolution.X<=0 || SelectedResolution.Y<=0)SelectedResolution=Desktop;
    }
    Resolutions.Reset();TSharedPtr<FIntPoint> Selection;
    for(FIntPoint R:Choices){auto Item=MakeShared<FIntPoint>(R);Resolutions.Add(Item);if(R==SelectedResolution)Selection=Item;}
    ResolutionPicker->RefreshOptions();ResolutionPicker->SetSelectedItem(Selection);
}
void SDisplaySettingsView::ApplySelected()
{
    if(PreviewTicker.IsValid()){FTSTicker::GetCoreTicker().RemoveTicker(PreviewTicker);PreviewTicker.Reset();}
    if(!CanApply() || !Settings.IsValid())return;
    // A resized window or Alt+Enter may differ from the last saved preference.
    // Capture the working viewport before applying the player's selected values.
    Settings->ResetToCurrentSettings();
    if(!Preview->Begin(SelectedResolution,SelectedMode,FPlatformTime::Seconds()))return;
    if(KeepButton && FSlateApplication::IsInitialized())FSlateApplication::Get().SetKeyboardFocus(KeepButton,EFocusCause::SetDirectly);
    PreviewTicker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateSP(this,&SDisplaySettingsView::UpdatePreview),.1f);
}

#if WITH_DEV_AUTOMATION_TESTS
#include "DisplaySettingsTests.inl"
#endif
bool SDisplaySettingsView::UpdatePreview(float Delta)
{
    if(!PageActive.Get(true))Preview->Cancel();
    Preview->Update(FPlatformTime::Seconds());
    if(Preview->IsPending())return true;
    PreviewTicker.Reset();Refresh();return false;
}
void SDisplaySettingsView::Cancel()
{
    if(PreviewTicker.IsValid()){FTSTicker::GetCoreTicker().RemoveTicker(PreviewTicker);PreviewTicker.Reset();}
    if(Preview)Preview->Cancel();
    if(ResolutionPicker)Refresh();
}
