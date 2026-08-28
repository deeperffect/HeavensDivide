#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ObjectiveSpawnPoint.generated.h"

class UBillboardComponent;

UCLASS(Blueprintable)
class HEAVENSDIVIDE_API AObjectiveSpawnPoint : public AActor
{
	GENERATED_BODY()
public:
	AObjectiveSpawnPoint();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Objective Director") TObjectPtr<UBillboardComponent> EditorSprite;
};

