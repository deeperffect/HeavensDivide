#pragma once
#include "CoreMinimal.h"
#include "MenuPromptWidget.h"
#include "PauseMenuWidget.generated.h"

UCLASS()
class HEAVENSDIVIDE_API UPauseMenuWidget : public UMenuPromptWidget
{
    GENERATED_BODY()
    friend class FPauseMenuTest;
public:
    UPauseMenuWidget(const FObjectInitializer& ObjectInitializer);
    void FocusResume();
    void CloseSettings();
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeDestruct() override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
private:
    UFUNCTION() void Resume();
    UFUNCTION() void Settings();
    UFUNCTION() void MainMenu();
    UPROPERTY() TSubclassOf<class UMainMenuWidget> SettingsClass;
    UPROPERTY(Transient) TObjectPtr<class UMainMenuWidget> SettingsWidget;
    UPROPERTY(Transient) TObjectPtr<UButton> ResumeButton;
};
