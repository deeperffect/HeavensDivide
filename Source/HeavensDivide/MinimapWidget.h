#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "MinimapWidget.generated.h"

class ASurvivorPlayerController;

UCLASS(Blueprintable)
class HEAVENSDIVIDE_API UMinimapWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void InitializeMinimap(ASurvivorPlayerController* InController);
protected:
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Minimap") FVector2D MapSize = FVector2D(250.0f, 250.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Minimap") FSlateBrush MinimapBackground;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Minimap") FLinearColor BackgroundColor = FLinearColor(0.015f, 0.02f, 0.03f, 0.82f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Minimap") float ContentPadding = 8.0f;
private:
	TWeakObjectPtr<ASurvivorPlayerController> Controller;
};
