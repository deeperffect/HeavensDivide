#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Containers/Ticker.h"
#include "GameFramework/GameUserSettings.h"
#include "Widgets/Input/SComboBox.h"
class UWorld;

/** Video-only preview. Keep saves new choices; cancellation restores the working mode. */
class FDisplaySettingsPreview
{
public:
    FDisplaySettingsPreview(UGameUserSettings* InSettings,TFunction<void()> InApply);
    ~FDisplaySettingsPreview();
    bool Begin(FIntPoint Resolution,EWindowMode::Type Mode,double Now);
    void Keep();
    void Cancel();
    void Update(double Now);
    bool IsPending() const { return bPending; }
    int32 SecondsLeft(double Now) const;
private:
    TWeakObjectPtr<UGameUserSettings> Settings;
    TFunction<void()> Apply;
    FIntPoint OriginalResolution=FIntPoint::ZeroValue;
    EWindowMode::Type OriginalMode=EWindowMode::Windowed;
    double Deadline=0;
    bool bPending=false;
};

class SDisplaySettingsView : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SDisplaySettingsView) {}
        SLATE_ARGUMENT(UWorld*, World)
        SLATE_ATTRIBUTE(bool, PageActive)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    virtual ~SDisplaySettingsView();
    void Refresh();
    void Cancel();
    void FocusInitial();
    bool CloseOpenSelector();
private:
    void RebuildResolutions();
    void ApplySelected();
    bool CanApply() const;
    bool UpdatePreview(float Delta);
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<UGameUserSettings> Settings;
    TAttribute<bool> PageActive;
    TUniquePtr<FDisplaySettingsPreview> Preview;
    TArray<TSharedPtr<FIntPoint>> Resolutions;
    TArray<TSharedPtr<EWindowMode::Type>> Modes;
    TSharedPtr<SComboBox<TSharedPtr<FIntPoint>>> ResolutionPicker;
    TSharedPtr<SComboBox<TSharedPtr<EWindowMode::Type>>> ModePicker;
    TSharedPtr<class SButton> KeepButton;
    FIntPoint SelectedResolution=FIntPoint::ZeroValue;
    EWindowMode::Type SelectedMode=EWindowMode::Windowed;
    FTSTicker::FDelegateHandle PreviewTicker;
    bool bRefreshing=false;
};
