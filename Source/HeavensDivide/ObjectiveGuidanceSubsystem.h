#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ObjectiveGuidanceSubsystem.generated.h"

class UObjectiveGuidanceWidget;

/** Run-local spawn notifications and player-relative guidance. Never owns a pause. */
UCLASS()
class HEAVENSDIVIDE_API UObjectiveGuidanceSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    void Announce(AActor* Objective);
    virtual void Tick(float DeltaTime) override;
    virtual void Deinitialize() override;
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UObjectiveGuidanceSubsystem,STATGROUP_Tickables); }
    virtual bool DoesSupportWorldType(EWorldType::Type Type) const override { return Type==EWorldType::Game || Type==EWorldType::PIE; }
    int32 GetTrackedCount() const { return Objectives.Num(); }
    static bool FindGuidanceTarget(AActor* Objective,const FVector& Player,FVector& Target);
private:
    void ClearPresentation();
    TArray<TWeakObjectPtr<AActor>> Objectives, PendingPopups;
    TWeakObjectPtr<AActor> PopupOwner;
    float PopupAge=3.f;
    UPROPERTY(Transient) TObjectPtr<UObjectiveGuidanceWidget> Widget;
};
