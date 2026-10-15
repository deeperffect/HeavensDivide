#pragma once
#include "CoreMinimal.h"
class ACharacter;
namespace PlayerDashMovement
{
    /** Swept dash movement. Returns true when solid geometry stops the dash. */
    bool Move(ACharacter* Character, const FVector& Delta);
}
