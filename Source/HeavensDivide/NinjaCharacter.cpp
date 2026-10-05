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
    ComboAbility.bSpawnVFXEveryPulse = true;
    ComboAbility.bFaceEnemyPack = true;
    ComboAbility.FallbackColor = FLinearColor(.35f, .75f, 1.f);
	AutoAttackComponent = CreateDefaultSubobject<UAutoAttackComponent>(TEXT("AutoAttackComponent"));
	NinjaBuildComponent = CreateDefaultSubobject<UNinjaBuildComponent>(TEXT("NinjaBuildComponent"));
}

#include "TimerManager.h"
void ANinjaCharacter::ApplySharedMoveSpeedMultiplier(float Multiplier)
{
	SharedMoveSpeedMultiplier = Multiplier;
	Super::ApplySharedMoveSpeedMultiplier(Multiplier * (1.f + ViperRushBonus));
}

void ANinjaCharacter::ApplyViperRush(float Bonus, float Duration)
{
	if (GetCharacterMode() != ECharacterMode::Active || !GetWorld() || Duration <= 0.f) return;
	ViperRushBonus = FMath::Max(0.f, Bonus);
	ApplySharedMoveSpeedMultiplier(SharedMoveSpeedMultiplier);
	// One timer and one bonus: another qualifying kill refreshes rather than stacks.
	GetWorldTimerManager().SetTimer(ViperRushTimer, this, &ANinjaCharacter::ClearViperRush, Duration, false);
}

void ANinjaCharacter::ClearViperRush()
{
	if (GetWorld()) GetWorldTimerManager().ClearTimer(ViperRushTimer);
	ViperRushBonus = 0.f;
	ApplySharedMoveSpeedMultiplier(SharedMoveSpeedMultiplier);
}

void ANinjaCharacter::SetCharacterMode(ECharacterMode NewMode)
{
	if (NewMode != ECharacterMode::Active) ClearViperRush();
	Super::SetCharacterMode(NewMode);
}
