#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MinimapRegistrySubsystem.generated.h"

class AMinimapBounds;
class UMinimapMarkerComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FMinimapMarkerRegistryEvent, UMinimapMarkerComponent*);

UCLASS()
class HEAVENSDIVIDE_API UMinimapRegistrySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	void RegisterMarker(UMinimapMarkerComponent* Marker);
	void UnregisterMarker(UMinimapMarkerComponent* Marker);
	void NotifyMarkerChanged(UMinimapMarkerComponent* Marker);
	const TArray<TWeakObjectPtr<UMinimapMarkerComponent>>& GetMarkers() const { return Markers; }
	void SetBounds(AMinimapBounds* InBounds);
	AMinimapBounds* GetBounds() const { return Bounds.Get(); }

	FMinimapMarkerRegistryEvent OnMarkerRegistered;
	FMinimapMarkerRegistryEvent OnMarkerUnregistered;
	FMinimapMarkerRegistryEvent OnMarkerStateChanged;
private:
	TArray<TWeakObjectPtr<UMinimapMarkerComponent>> Markers;
	TWeakObjectPtr<AMinimapBounds> Bounds;
};

