// Copyright Epic Games, Inc. All Rights Reserved.

#include "SamuraiCharacter.h"

#include "AutoAttackComponent.h"
#include "TimerManager.h"

ASamuraiCharacter::ASamuraiCharacter()
{
    ComboAbility.DisplayName = FText::FromString(TEXT("Tornado"));
    ComboAbility.InitialRadius = 360.f;
    ComboAbility.FinalRadius = 1000.f;
	AutoAttackComponent = CreateDefaultSubobject<UAutoAttackComponent>(TEXT("AutoAttackComponent"));
}

void ASamuraiCharacter::ApplySharedMoveSpeedMultiplier(float Multiplier)
{
	SharedMoveSpeedMultiplier = Multiplier;
	Super::ApplySharedMoveSpeedMultiplier(Multiplier * (1.f + BloodRushBonus));
}

void ASamuraiCharacter::ApplyBloodRush(float Bonus, float Duration)
{
	if (GetCharacterMode() != ECharacterMode::Active || !GetWorld() || Duration <= 0.f) return;
	BloodRushBonus = FMath::Max(0.f, Bonus);
	ApplySharedMoveSpeedMultiplier(SharedMoveSpeedMultiplier);
	// One timer and one bonus: another qualifying kill refreshes rather than stacks.
	GetWorldTimerManager().SetTimer(BloodRushTimer, this, &ASamuraiCharacter::ClearBloodRush, Duration, false);
}

void ASamuraiCharacter::ClearBloodRush()
{
	if (GetWorld()) GetWorldTimerManager().ClearTimer(BloodRushTimer);
	BloodRushBonus = 0.f;
	ApplySharedMoveSpeedMultiplier(SharedMoveSpeedMultiplier);
}

void ASamuraiCharacter::SetCharacterMode(ECharacterMode NewMode)
{
	if (NewMode != ECharacterMode::Active) ClearBloodRush();
	Super::SetCharacterMode(NewMode);
}
