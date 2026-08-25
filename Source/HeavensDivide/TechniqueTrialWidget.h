#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TechniqueTrialWidget.generated.h"

class STextBlock;

UCLASS()
class HEAVENSDIVIDE_API UTechniqueTrialWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void ShowMemoryStatus(int32 RoundIndex, int32 RoundCount, const FText& PhaseText, int32 StepIndex, int32 StepCount);
	void ShowComplete();
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
private:
	TSharedPtr<STextBlock> Header;
	TSharedPtr<STextBlock> Stance;
	TSharedPtr<STextBlock> Instruction;
	TSharedPtr<STextBlock> Time;
};
