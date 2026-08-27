#include "MinimapMarkerComponent.h"
#include "MinimapRegistrySubsystem.h"

UMinimapMarkerComponent::UMinimapMarkerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMinimapMarkerComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UWorld* World = GetWorld()) World->GetSubsystem<UMinimapRegistrySubsystem>()->RegisterMarker(this);
}

void UMinimapMarkerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld()) World->GetSubsystem<UMinimapRegistrySubsystem>()->UnregisterMarker(this);
	Super::EndPlay(EndPlayReason);
}

void UMinimapMarkerComponent::SetMarkerState(EMinimapMarkerState NewState)
{
	if (MarkerState == NewState) return;
	MarkerState = NewState;
	NotifyChanged();
}

void UMinimapMarkerComponent::SetMarkerVisible(bool bNewVisible)
{
	if (bMarkerVisible == bNewVisible) return;
	bMarkerVisible = bNewVisible;
	NotifyChanged();
}

FVector UMinimapMarkerComponent::GetMarkerWorldLocation() const
{
	return GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
}

void UMinimapMarkerComponent::NotifyChanged()
{
	if (UWorld* World = GetWorld()) World->GetSubsystem<UMinimapRegistrySubsystem>()->NotifyMarkerChanged(this);
}

