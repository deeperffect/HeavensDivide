#include "MinimapRegistrySubsystem.h"
#include "MinimapBounds.h"
#include "MinimapMarkerComponent.h"

void UMinimapRegistrySubsystem::RegisterMarker(UMinimapMarkerComponent* Marker)
{
	if (!IsValid(Marker) || Markers.Contains(Marker)) return;
	Markers.RemoveAll([](const TWeakObjectPtr<UMinimapMarkerComponent>& Entry){ return !Entry.IsValid(); });
	Markers.Add(Marker);
	UE_LOG(LogTemp, Log, TEXT("[Minimap] Registered %s (%s)"), *GetNameSafe(Marker->GetOwner()), *UEnum::GetValueAsString(Marker->GetMarkerType()));
	OnMarkerRegistered.Broadcast(Marker);
}

void UMinimapRegistrySubsystem::UnregisterMarker(UMinimapMarkerComponent* Marker)
{
	if (!Marker) return;
	const int32 Removed = Markers.RemoveAll([Marker](const TWeakObjectPtr<UMinimapMarkerComponent>& Entry){ return !Entry.IsValid() || Entry.Get() == Marker; });
	if (Removed > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[Minimap] Unregistered %s"), *GetNameSafe(Marker->GetOwner()));
		OnMarkerUnregistered.Broadcast(Marker);
	}
}

void UMinimapRegistrySubsystem::NotifyMarkerChanged(UMinimapMarkerComponent* Marker)
{
	if (IsValid(Marker) && Markers.Contains(Marker)) OnMarkerStateChanged.Broadcast(Marker);
}

void UMinimapRegistrySubsystem::SetBounds(AMinimapBounds* InBounds)
{
	if (InBounds && Bounds.IsValid() && Bounds.Get() != InBounds)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Minimap] Multiple MinimapBounds actors found; using %s."), *GetNameSafe(InBounds));
	}
	Bounds = InBounds;
}

