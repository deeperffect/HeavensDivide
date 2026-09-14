// Copyright Epic Games, Inc. All Rights Reserved.

#include "NinjaCharacter.h"

#include "AutoAttackComponent.h"
#include "NinjaBuildComponent.h"

ANinjaCharacter::ANinjaCharacter()
{
	AutoAttackComponent = CreateDefaultSubobject<UAutoAttackComponent>(TEXT("AutoAttackComponent"));
	NinjaBuildComponent = CreateDefaultSubobject<UNinjaBuildComponent>(TEXT("NinjaBuildComponent"));
}
