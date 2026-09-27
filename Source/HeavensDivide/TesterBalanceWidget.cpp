#include "TesterBalanceWidget.h"
#include "TesterBalanceSettings.h"
#include "MainMenuWidget.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"

void UTesterBalanceWidget::Open(UMainMenuWidget* Menu)
{
    OwnerMenu = Menu;
    Font = Menu->GetMenuButtonFont(); Font.Size = 16;
    Draft = NewObject<UTesterBalanceSettings>(this);
    Draft->CopyFrom(*GetDefault<UTesterBalanceSettings>());
    Status = FText::GetEmpty();
    if (Host) Host->SetContent(BuildPage());
}
TSharedRef<SWidget> UTesterBalanceWidget::RebuildWidget()
{
    SAssignNew(Host, SBox);
    if (Draft) Host->SetContent(BuildPage());
    return Host.ToSharedRef();
}
void UTesterBalanceWidget::ReleaseSlateResources(bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    Host.Reset();
}
FReply UTesterBalanceWidget::Close(bool bSave)
{
    if (bSave)
    {
        auto* Settings = GetMutableDefault<UTesterBalanceSettings>();
        Settings->CopyFrom(*Draft);
        Settings->SaveConfig();
        UE_LOG(LogTemp, Display, TEXT("[TesterBalance] Saved: %s"), *Settings->Report());
    }
    if (OwnerMenu) OwnerMenu->ShowMainPanel();
    return FReply::Handled();
}
TSharedRef<SWidget> UTesterBalanceWidget::BuildPage()
{
    const FLinearColor Ivory(.85f, .81f, .71f);
    auto Label = [&](const FString& Text) -> TSharedRef<SWidget> {
        return SNew(STextBlock).Text(FText::FromString(Text)).Font(Font).ColorAndOpacity(Ivory).AutoWrapText(true);
    };
    auto Number = [](TFunction<float()> Read, TFunction<void(float)> Write, float Min, float Max) -> TSharedRef<SWidget> {
        return SNew(SBox).WidthOverride(130)[SNew(SSpinBox<float>).MinValue(Min).MaxValue(Max)
            .MinSliderValue(Min).MaxSliderValue(Max).Delta(.1f).MinFractionalDigits(1).MaxFractionalDigits(2)
            .Value_Lambda([Read] { return Read(); }).OnValueChanged_Lambda([Write](float Value) { Write(Value); })];
    };
    auto Rows = SNew(SVerticalBox);
    Rows->AddSlot().AutoHeight().Padding(0, 8)[Label(TEXT("TESTER BALANCE"))];
    Rows->AddSlot().AutoHeight().Padding(0, 8)[Label(TEXT("Applies to NEW RUNS. 1x = normal balance.\nSave to keep changes. Back discards unsaved changes."))];
    Rows->AddSlot().AutoHeight().Padding(0, 10)[SNew(SCheckBox)
        .IsChecked_Lambda([this] { return Draft->bEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
        .OnCheckStateChanged_Lambda([this](ECheckBoxState Value) { Draft->bEnabled = Value == ECheckBoxState::Checked; })
        [Label(TEXT("Enable tester overrides"))]];
    auto Row = [&](const FString& Name, TSharedRef<SWidget> Control) {
        Rows->AddSlot().AutoHeight().Padding(0, 5)[SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Label(Name)]
            + SHorizontalBox::Slot().AutoWidth()[Control]];
    };
    Row(TEXT("Maximum alive (0 = phase defaults; cap, not spawn target)"), SNew(SBox).WidthOverride(130)
        [SNew(SSpinBox<int32>).MinValue(0).MaxValue(500).Delta(1)
         .Value_Lambda([this] { return Draft->MaxAlive; }).OnValueChanged_Lambda([this](int32 Value) { Draft->MaxAlive = Value; })]);
    Row(TEXT("Population density multiplier"), Number([this]{return Draft->Population;}, [this](float V){Draft->Population=V;}, .1f, 5));
    Row(TEXT("Spawn interval multiplier (lower = faster refills)"), Number([this]{return Draft->SpawnInterval;}, [this](float V){Draft->SpawnInterval=V;}, .1f, 5));
    Row(TEXT("Health growth per minute multiplier (0 = no growth)"), Number([this]{return Draft->HealthGrowth;}, [this](float V){Draft->HealthGrowth=V;}, 0, 5));
    Rows->AddSlot().AutoHeight().Padding(0, 10)[SNew(SCheckBox)
        .IsChecked_Lambda([this] { return Draft->bDisableEvents ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
        .OnCheckStateChanged_Lambda([this](ECheckBoxState V) { Draft->bDisableEvents = V == ECheckBoxState::Checked; })
        [Label(TEXT("Disable pressure events (scheduled ambushes)"))]];
    Rows->AddSlot().AutoHeight().Padding(0, 12)[SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1)[Label(TEXT("MONSTER"))]
        + SHorizontalBox::Slot().AutoWidth().Padding(12,0)[SNew(SBox).WidthOverride(130)[Label(TEXT("HEALTH x"))]]
        + SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(130)[Label(TEXT("SPEED x"))]]];
    for (const auto& Entry : UTesterBalanceSettings::Roster())
    {
        const FString Key = Entry.Value;
        Rows->AddSlot().AutoHeight().Padding(0, 4)[SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Label(Entry.Key)]
            + SHorizontalBox::Slot().AutoWidth().Padding(12, 0)[Number([this,Key]{return Draft->Enemies.FindRef(Key).Health;}, [this,Key](float V){Draft->Enemies.FindOrAdd(Key).Health=V;}, .1f, 10)]
            + SHorizontalBox::Slot().AutoWidth()[Number([this,Key]{return Draft->Enemies.FindRef(Key).Speed;}, [this,Key](float V){Draft->Enemies.FindOrAdd(Key).Speed=V;}, .1f, 3)]];
    }
    Rows->AddSlot().AutoHeight().Padding(0, 12)[Label(TEXT("Speed changes normal movement; scripted dashes and attack timings keep their authored values. Population controls affect the survival director, not trial/boss summons."))];
    auto Actions = SNew(SHorizontalBox);
    auto Button = [&](const FString& Text, TFunction<FReply()> Click) {
        Actions->AddSlot().FillWidth(1).Padding(5)[SNew(SButton).ContentPadding(FMargin(12, 10))
            .ButtonColorAndOpacity(FLinearColor(.12f,.10f,.06f))
            .OnClicked_Lambda([Click]{return Click();})[Label(Text)]];
    };
    Button(TEXT("SAVE & BACK"), [this]{return Close(true);});
    Button(TEXT("BACK"), [this]{return Close(false);});
    Button(TEXT("RESET"), [this]{Draft->Reset(); Status=FText::FromString(TEXT("Defaults restored. Save to keep them.")); return FReply::Handled();});
    Button(TEXT("COPY SETTINGS"), [this]{Draft->Sanitize(); FPlatformApplicationMisc::ClipboardCopy(*Draft->Report()); Status=FText::FromString(TEXT("Settings copied. Paste them with your playtest feedback.")); return FReply::Handled();});
    return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.018f,.016f,.014f,1))
        .Padding(28).HAlign(HAlign_Center).VAlign(VAlign_Center)
        [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
         [SNew(SBox).WidthOverride(1000).HeightOverride(830)
          [SNew(SVerticalBox)
           + SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox)+SScrollBox::Slot()[Rows]]
           + SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(STextBlock).Text_Lambda([this]{return Status;}).ColorAndOpacity(Ivory)]
           + SVerticalBox::Slot().AutoHeight()[Actions]]]];
}
