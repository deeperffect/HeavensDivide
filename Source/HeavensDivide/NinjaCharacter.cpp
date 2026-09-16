// Copyright Epic Games, Inc. All Rights Reserved.

#include "NinjaCharacter.h"

#include "AutoAttackComponent.h"
#include "NinjaBuildComponent.h"

ANinjaCharacter::ANinjaCharacter()
{
    ComboAbility.DisplayName = FText::FromString(TEXT("Thousand Cuts"));
    ComboAbility.DamagePerPulse = 6.f;
    ComboAbility.InitialRadius = 650.f;
    ComboAbility.FinalRadius = 650.f;
    ComboAbility.EffectDuration = 1.4f;
    ComboAbility.PulseInterval = .08f;
    ComboAbility.bFaceEnemyPack = true;
    ComboAbility.FallbackColor = FLinearColor(.35f, .75f, 1.f);
	AutoAttackComponent = CreateDefaultSubobject<UAutoAttackComponent>(TEXT("AutoAttackComponent"));
	NinjaBuildComponent = CreateDefaultSubobject<UNinjaBuildComponent>(TEXT("NinjaBuildComponent"));
}
