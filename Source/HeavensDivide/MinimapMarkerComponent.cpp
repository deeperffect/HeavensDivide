#include "MinimapMarkerComponent.h"
#include "Components/SceneComponent.h"
#include "MinimapRegistrySubsystem.h"
#include "ObjectiveGuidanceSubsystem.h"

UMinimapMarkerComponent::UMinimapMarkerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMinimapMarkerComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UWorld* World = GetWorld()) World->GetSubsystem<UMinimapRegistrySubsystem>()->RegisterMarker(this);
    if (MarkerType==EMinimapMarkerType::Objective && !DisplayName.IsEmpty())
        if (auto* Guidance=GetWorld()->GetSubsystem<UObjectiveGuidanceSubsystem>()) Guidance->Announce(GetOwner());
}

void UMinimapMarkerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld()) World->GetSubsystem<UMinimapRegistrySubsystem>()->UnregisterMarker(this);
	Super::EndPlay(EndPlayReason);
}

void UMinimapMarkerComponent::SetMarkerState(EMinimapMarkerState NewState)
{
	if (MarkerState == NewState) return;
    const bool bGateUnlocked=MarkerType==EMinimapMarkerType::BossGate && MarkerState==EMinimapMarkerState::Locked && NewState==EMinimapMarkerState::Available;
	MarkerState = NewState;
	NotifyChanged();
    if(bGateUnlocked) if(UWorld* World=GetWorld())
        if(auto* Guidance=World->GetSubsystem<UObjectiveGuidanceSubsystem>()) Guidance->Announce(GetOwner());
}

void UMinimapMarkerComponent::SetMarkerVisible(bool bNewVisible)
{
	if (bMarkerVisible == bNewVisible) return;
	bMarkerVisible = bNewVisible;
	NotifyChanged();
}

FVector UMinimapMarkerComponent::GetMarkerWorldLocation() const
{
	if (LocationAnchor) return LocationAnchor->GetComponentLocation();
	return GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
}

void UMinimapMarkerComponent::NotifyChanged()
{
	if (UWorld* World = GetWorld()) World->GetSubsystem<UMinimapRegistrySubsystem>()->NotifyMarkerChanged(this);
}
