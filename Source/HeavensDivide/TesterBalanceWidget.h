#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TesterBalanceWidget.generated.h"
class UTesterBalanceSettings;
class SBox;
class UMainMenuWidget;
UCLASS()
class HEAVENSDIVIDE_API UTesterBalanceWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void Open(UMainMenuWidget* Menu);
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
private:
    TSharedRef<SWidget> BuildPage();
    FReply Close(bool bSave);
    UPROPERTY(Transient) TObjectPtr<UTesterBalanceSettings> Draft;
    UPROPERTY(Transient) TObjectPtr<UMainMenuWidget> OwnerMenu;
    TSharedPtr<SBox> Host;
    FSlateFontInfo Font;
    FText Status;
};
