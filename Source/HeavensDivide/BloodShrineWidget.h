// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BloodShrineWidget.generated.h"

class STextBlock;
class SBox;

UCLASS()
class HEAVENSDIVIDE_API UBloodShrineWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void ShowInteractionPrompt();
	void ShowInteractionPrompt(const FText& ShrineName);
	void ConfigureForWorldSpace();
	void ShowChallenge(int32 CurrentBlood, int32 RequiredBlood, float TimeRemaining);
	void ShowResult(bool bSuccess);
	void HideStatus();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void SetLines(const FText& Header, const FText& Progress, const FText& Time);

	TSharedPtr<STextBlock> HeaderText;
	TSharedPtr<STextBlock> ProgressText;
	TSharedPtr<STextBlock> TimeText;
	TSharedPtr<STextBlock> KeyText;
	TSharedPtr<SBox> RootBox;
};
