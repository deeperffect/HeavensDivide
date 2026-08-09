// Copyright Epic Games, Inc. All Rights Reserved.

#include "NinjaCharacter.h"

#include "AutoAttackComponent.h"

ANinjaCharacter::ANinjaCharacter()
{
	AutoAttackComponent = CreateDefaultSubobject<UAutoAttackComponent>(TEXT("AutoAttackComponent"));
}
