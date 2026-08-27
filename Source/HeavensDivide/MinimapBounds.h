#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MinimapBounds.generated.h"

class UBoxComponent;

UCLASS(Blueprintable)
class HEAVENSDIVIDE_API AMinimapBounds : public AActor
{
	GENERATED_BODY()
public:
	AMinimapBounds();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UFUNCTION(BlueprintPure, Category="Minimap") FVector2D WorldToNormalized(const FVector& WorldLocation, bool& bWasClamped) const;
	UFUNCTION(BlueprintPure, Category="Minimap") float WorldYawToMapYaw(float WorldYaw) const;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Minimap") TObjectPtr<UBoxComponent> PlayableArea;
};

