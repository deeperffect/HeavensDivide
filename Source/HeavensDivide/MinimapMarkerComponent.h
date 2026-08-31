#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Styling/SlateBrush.h"
#include "MinimapTypes.h"
#include "MinimapMarkerComponent.generated.h"

class USceneComponent;

UCLASS(ClassGroup=(Minimap), meta=(BlueprintSpawnableComponent), BlueprintType)
class HEAVENSDIVIDE_API UMinimapMarkerComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UMinimapMarkerComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category="Minimap") void SetMarkerState(EMinimapMarkerState NewState);
	UFUNCTION(BlueprintCallable, Category="Minimap") void SetMarkerVisible(bool bNewVisible);
	UFUNCTION(BlueprintPure, Category="Minimap") EMinimapMarkerType GetMarkerType() const { return MarkerType; }
	UFUNCTION(BlueprintPure, Category="Minimap") EMinimapMarkerState GetMarkerState() const { return MarkerState; }
	UFUNCTION(BlueprintPure, Category="Minimap") bool IsMarkerVisible() const { return bMarkerVisible && MarkerState != EMinimapMarkerState::Completed && MarkerState != EMinimapMarkerState::Failed; }
	UFUNCTION(BlueprintPure, Category="Minimap") FVector GetMarkerWorldLocation() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Minimap") EMinimapMarkerType MarkerType = EMinimapMarkerType::Objective;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Minimap") EMinimapMarkerState MarkerState = EMinimapMarkerState::Available;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Minimap") bool bMarkerVisible = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Minimap") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Minimap") int32 Priority = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Minimap") FSlateBrush Icon;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Minimap", meta=(ClampMin="4.0")) FVector2D MarkerSize = FVector2D(14.0f, 14.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Minimap", meta=(ToolTip="Optional component whose world location is used instead of the owning actor origin.")) TObjectPtr<USceneComponent> LocationAnchor;
private:
	void NotifyChanged();
};
