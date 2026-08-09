// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CharacterBase.h"
#include "SamuraiCharacter.generated.h"

class UAutoAttackComponent;

UCLASS()
class HEAVENSDIVIDE_API ASamuraiCharacter : public ACharacterBase
{
	GENERATED_BODY()

public:
	ASamuraiCharacter();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UAutoAttackComponent> AutoAttackComponent;
};
