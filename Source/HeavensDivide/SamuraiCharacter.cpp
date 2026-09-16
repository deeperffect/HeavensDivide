// Copyright Epic Games, Inc. All Rights Reserved.

#include "SamuraiCharacter.h"

#include "AutoAttackComponent.h"

ASamuraiCharacter::ASamuraiCharacter()
{
    ComboAbility.DisplayName = FText::FromString(TEXT("Tornado"));
    ComboAbility.InitialRadius = 360.f;
    ComboAbility.FinalRadius = 1000.f;
	AutoAttackComponent = CreateDefaultSubobject<UAutoAttackComponent>(TEXT("AutoAttackComponent"));
}
