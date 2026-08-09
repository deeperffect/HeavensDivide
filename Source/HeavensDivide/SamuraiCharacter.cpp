// Copyright Epic Games, Inc. All Rights Reserved.

#include "SamuraiCharacter.h"

#include "AutoAttackComponent.h"

ASamuraiCharacter::ASamuraiCharacter()
{
	AutoAttackComponent = CreateDefaultSubobject<UAutoAttackComponent>(TEXT("AutoAttackComponent"));
}
