#include "CameraOcclusionComponent.h"
#include "PlayerCameraRig.h"
#include "CharacterBase.h"
#include "Camera/CameraComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Stats/Stats.h"

UCameraOcclusionComponent::UCameraOcclusionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UCameraOcclusionComponent::TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* Tick)
{
    QUICK_SCOPE_CYCLE_COUNTER(STAT_CameraOcclusionFade);
    Super::TickComponent(Delta, TickType, Tick);
    const auto* Rig = Cast<APlayerCameraRig>(GetOwner());
    const auto* Target = Rig ? Rig->GetFollowTarget() : nullptr;
    const auto* Camera = Rig ? Rig->FindComponentByClass<UCameraComponent>() : nullptr;
    if (!bEnabled || !IsValid(Target) || !Camera)
    {
        RestoreAll();
        return;
    }
    Elapsed += FMath::Max(0.f, Delta);
    UntilNextCheck -= Delta;
    if (UntilNextCheck <= 0.f)
    {
        UntilNextCheck = FMath::Max(0.05f, CheckInterval);
        ++OcclusionChecks;
        Hits.Reset();
        FCollisionQueryParams Params(SCENE_QUERY_STAT(CameraOcclusion), true, Rig);
        Params.AddIgnoredActor(Target);
        // Only prepared overlap scenery participates; an enemy capsule using
        // an all-block response must not truncate the camera corridor query.
        Params.bIgnoreBlocks = true;
        // Prepared scenery overlaps this dedicated channel, so foreground
        // objects do not hide further occluders from the same query.
        GetWorld()->SweepMultiByChannel(Hits, Camera->GetComponentLocation(),
            Target->GetActorLocation() + FVector(0,0,25), FQuat::Identity,
            ECC_GameTraceChannel3, FCollisionShape::MakeSphere(ProbeRadius), Params);
        for (const FHitResult& Hit : Hits)
        {
            UPrimitiveComponent* Component = Hit.GetComponent();
            if (Component && Component->ComponentHasTag(TEXT("CameraFade")) && Component->IsVisible())
            {
                if (Fades.Num() < 32 || Fades.Contains(Component))
                    Fades.FindOrAdd(Component).LastSeen = Elapsed;
            }
        }
    }
    for (auto It = Fades.CreateIterator(); It; ++It)
    {
        UPrimitiveComponent* Component = It.Key().Get();
        if (!Component) { It.RemoveCurrent(); continue; }
        FFadeState& State = It.Value();
        const bool bOccluding = Elapsed - State.LastSeen <= FMath::Max(0.15f, CheckInterval * 2.f);
        const float Goal = bOccluding ? 1.f : 0.f;
        const float PreviousAmount = State.Amount;
        State.Amount = FMath::FInterpConstantTo(State.Amount, Goal, Delta,
            1.f / FMath::Max(0.01f, bOccluding ? FadeOutSeconds : RestoreSeconds));
        if (PreviousAmount != State.Amount)
            Component->SetCustomPrimitiveDataFloat(FadeDataIndex, State.Amount);
        if (!bOccluding && State.Amount <= 0.f) It.RemoveCurrent();
    }
    FadingObjectCount = Fades.Num();
}

void UCameraOcclusionComponent::RestoreAll()
{
    for (auto& Entry : Fades)
        if (auto* Component = Entry.Key.Get()) Component->SetCustomPrimitiveDataFloat(FadeDataIndex, 0.f);
    Fades.Reset();
    FadingObjectCount = 0;
    UntilNextCheck = 0.f;
}

void UCameraOcclusionComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    RestoreAll();
    Super::EndPlay(Reason);
}
