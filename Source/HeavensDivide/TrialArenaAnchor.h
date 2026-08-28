#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrialArenaAnchor.generated.h"

class UBillboardComponent;

UCLASS(Blueprintable)
class HEAVENSDIVIDE_API ATrialArenaAnchor : public AActor
{
	GENERATED_BODY()
public:
	ATrialArenaAnchor();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Trial Arena") TObjectPtr<UBillboardComponent> EditorSprite;
};

