#include "JointComboActor.h"
#include "PlayerCameraRig.h"
#include "SwapAfterimage.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "NiagaraComponent.h"
#include "Kismet/GameplayStatics.h"

namespace
{
    float ReleaseEase(float Alpha)
    {
        Alpha=FMath::Clamp(Alpha,0.f,1.f);
        return Alpha*Alpha*(3.f-2.f*Alpha);
    }
}
float AJointComboActor::GetReleaseSpeed() const
{
    if (!Data || Age<Windup || bPerformersReleased) return 1.f;
    return FMath::Lerp(FMath::Clamp(Data->ReleaseStartSpeed,.1f,1.f),1.f,
        ReleaseEase((Age-Windup)/FMath::Min(FMath::Max(.01f,Data->ReleaseRampSeconds),ControlDuration-Windup)));
}
float AJointComboActor::GetWorldSpeed() const
{
    if (!Data || bPerformersReleased) return 1.f;
    return FMath::Lerp(FMath::Clamp(Data->CinematicWorldSpeed,.03f,1.f),1.f,
        ReleaseEase((Age-Windup)/FMath::Max(.01f,ControlDuration-Windup)));
}
void AJointComboActor::BeginSlowMotion()
{
    auto* Settings=GetWorld()->GetWorldSettings();
    SavedCinematicSpeed=Settings->CinematicTimeDilation;
    AppliedCinematicSpeed=Settings->SetCinematicTimeDilation(SavedCinematicSpeed*FMath::Clamp(Data->CinematicWorldSpeed,.03f,1.f));
    SlowCompensation=SavedCinematicSpeed/FMath::Max(.001f,AppliedCinematicSpeed);
    // Use the separate cinematic layer: pause, upgrades and death retain ownership
    // of ordinary world time. Actor compensation starts with the next computed frame.
    SlowTickHandle=FWorldDelegates::OnWorldPreActorTick.AddUObject(this,&AJointComboActor::UpdateSlowMotion);
    CompensateActor(this); CompensateActor(CameraRig.Get());
    CompensateActor(SamuraiVisual.Get()); CompensateActor(NinjaVisual.Get());
    for (const auto& Child:Children) CompensateActor(Child.Get());
}
void AJointComboActor::CompensateActor(AActor* Actor)
{
    if (!SlowTickHandle.IsValid() || !IsValid(Actor) || CompensatedActors.Contains(Actor)) return;
    CompensatedActors.Add(Actor,Actor->CustomTimeDilation);
    if (bSlowClockStarted) Actor->CustomTimeDilation*=SlowCompensation;
    TArray<AActor*> Attached;Actor->GetAttachedActors(Attached,false,true);
    for (auto* Child:Attached) CompensateActor(Child);
}
void AJointComboActor::RegisterChild(AActor* Child)
{
    if (!IsValid(Child)) return;
    Children.AddUnique(Child); CompensateActor(Child);
}
void AJointComboActor::ScheduleAttack(float Delay,TFunction<void()> Action)
{
    DelayedAttacks.Add({Age+FMath::Max(0.f,Delay),MoveTemp(Action)});
}
void AJointComboActor::RegisterEffect(UNiagaraComponent* FX)
{
    if (!SlowTickHandle.IsValid() || !IsValid(FX) || SoloEffects.Contains(FX)) return;
    const float SavedRate=FX->GetCustomTimeDilation();
    const float AppliedRate=SavedRate*(CompensatedActors.Contains(FX->GetOwner())?1.f:SlowCompensation);
    SoloEffects.Add(FX,{FX->GetForceSolo(),SavedRate,AppliedRate});
    FX->SetForceSolo(true); FX->SetCustomTimeDilation(AppliedRate);
}
void AJointComboActor::UpdateSlowMotion(UWorld* World,ELevelTick TickType,float Delta)
{
    if (World!=GetWorld()) return;
    if (!FMath::IsNearlyEqual(World->GetWorldSettings()->CinematicTimeDilation,AppliedCinematicSpeed))
    { EndSlowMotion(); return; }
    if (UGameplayStatics::IsGamePaused(this)) return;
    // World delta has ALREADY been computed with AppliedCinematicSpeed for this
    // frame. Compensate against that value, then set the next frame's world speed.
    // Every native attack, performer and effect uses the same acceleration curve.
    const float PreviousCompensation=SlowCompensation;
    SlowCompensation=SavedCinematicSpeed/FMath::Max(.001f,AppliedCinematicSpeed)*GetReleaseSpeed();
    for (auto& Pair:CompensatedActors) if (Pair.Key.IsValid())
        if (!bSlowClockStarted || FMath::IsNearlyEqual(Pair.Key->CustomTimeDilation,Pair.Value*PreviousCompensation))
            Pair.Key->CustomTimeDilation=Pair.Value*SlowCompensation;
    bSlowClockStarted=true;
    for (auto& Pair:SoloEffects) if (Pair.Key.IsValid()) {
        auto& Clock=Pair.Value;
        if (FMath::IsNearlyEqual(Pair.Key->GetCustomTimeDilation(),Clock.AppliedRate)) {
            Clock.AppliedRate=Clock.SavedRate*(CompensatedActors.Contains(Pair.Key->GetOwner())?1.f:SlowCompensation);
            Pair.Key->SetCustomTimeDilation(Clock.AppliedRate);
        }
    }
    AppliedCinematicSpeed=World->GetWorldSettings()->SetCinematicTimeDilation(SavedCinematicSpeed*GetWorldSpeed());
    // Niagara's batched simulation uses world time. Solo simulation instead uses
    // the owning attack's compensated component delta, preserving authored rates.
    for (auto& Pair:CompensatedActors) if (Pair.Key.IsValid()) {
        TInlineComponentArray<UNiagaraComponent*> EffectsOnActor(Pair.Key.Get());
        for (auto* FX:EffectsOnActor) RegisterEffect(FX);
    }
}
void AJointComboActor::EndSlowMotion()
{
    if (!SlowTickHandle.IsValid()) return;
    FWorldDelegates::OnWorldPreActorTick.Remove(SlowTickHandle); SlowTickHandle.Reset();
    if (auto* Settings=GetWorld()?GetWorld()->GetWorldSettings():nullptr)
        if (FMath::IsNearlyEqual(Settings->CinematicTimeDilation,AppliedCinematicSpeed)) Settings->SetCinematicTimeDilation(SavedCinematicSpeed);
    for (auto& Pair:CompensatedActors) if (Pair.Key.IsValid())
        if (!bSlowClockStarted || FMath::IsNearlyEqual(Pair.Key->CustomTimeDilation,Pair.Value*SlowCompensation)) Pair.Key->CustomTimeDilation=Pair.Value;
    for (auto& Pair:SoloEffects) if (Pair.Key.IsValid()) {
        if (FMath::IsNearlyEqual(Pair.Key->GetCustomTimeDilation(),Pair.Value.AppliedRate)) Pair.Key->SetCustomTimeDilation(Pair.Value.SavedRate);
        Pair.Key->SetForceSolo(Pair.Value.bSolo);
    }
    CompensatedActors.Reset(); SoloEffects.Reset(); bSlowClockStarted=false;
}
