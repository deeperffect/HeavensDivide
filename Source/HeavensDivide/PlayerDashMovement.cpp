#include "PlayerDashMovement.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

bool PlayerDashMovement::Move(ACharacter* Character, const FVector& Delta)
{
    auto* Movement = Character ? Character->GetCharacterMovement() : nullptr;
    if (!Movement || !Movement->UpdatedComponent) return true;
    FHitResult Hit;
    Movement->SafeMoveUpdatedComponent(Delta, Character->GetActorQuat(), true, Hit);
    if (!Hit.bBlockingHit) return false;
    // Only grounded dashes may step: no climbing walls or gaining a step in air.
    if (!Movement->IsMovingOnGround()) return true;
    Movement->FindFloor(Character->GetActorLocation(), Movement->CurrentFloor, false);
    if (!Movement->CurrentFloor.IsWalkableFloor()) return true;
    FVector Remaining = Delta * (1.f - Hit.Time);
    if (Movement->IsWalkable(Hit))
    {
        // Follow the walkable plane while preserving authored horizontal range.
        Remaining.Z = -FVector::DotProduct(Remaining, Hit.ImpactNormal) / Hit.ImpactNormal.Z;
        Movement->SafeMoveUpdatedComponent(Remaining, Character->GetActorQuat(), true, Hit);
        if (!Hit.bBlockingHit) { Movement->bForceNextFloorCheck = true; return false; }
        Remaining *= 1.f - Hit.Time;
    }
    // Native StepUp sweeps up/forward/down, checks walkability, ceiling and
    // MaxStepHeight, and rolls back failed attempts instead of teleporting.
    const FVector BeforeStep = Character->GetActorLocation();
    const bool bStepped = Movement->StepUp(FVector::DownVector, Remaining, Hit);
    Movement->bForceNextFloorCheck = true;
    // StepUp can succeed after sliding in place against a wall. Require the
    // remaining forward travel too, so a solid wall still ends the dash.
    const double Progress = FVector::DotProduct(Character->GetActorLocation()-BeforeStep, Remaining.GetSafeNormal2D());
    return !bStepped || Progress < Remaining.Size2D()-1.0;
}
