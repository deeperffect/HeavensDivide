// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EnemyStatusTypes.h"
#include "EnemyStatusIndicatorWidget.generated.h"

class UBorder;
class UTextBlock;

/** Compact status icon with an overlaid authoritative stack count. */
UCLASS()
class HEAVENSDIVIDE_API UEnemyStatusIndicatorWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	void SetStatusPresentation(EEnemyStatusEffect Status, int32 StackCount, bool bShowCountAtOne, int32 FontSize);

private:
	UPROPERTY(Transient)
	TObjectPtr<UBorder> IconBackground;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusGlyph;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StackCountText;
};
