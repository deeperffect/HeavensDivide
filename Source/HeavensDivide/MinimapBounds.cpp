#include "MinimapBounds.h"
#include "Components/BoxComponent.h"
#include "MinimapRegistrySubsystem.h"

AMinimapBounds::AMinimapBounds()
{
	PrimaryActorTick.bCanEverTick = false;
	PlayableArea = CreateDefaultSubobject<UBoxComponent>(TEXT("PlayableArea"));
	SetRootComponent(PlayableArea);
	PlayableArea->SetBoxExtent(FVector(5000.0f, 5000.0f, 500.0f));
	PlayableArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlayableArea->ShapeColor = FColor(35, 180, 255);
}

void AMinimapBounds::BeginPlay()
{
	Super::BeginPlay();
	GetWorld()->GetSubsystem<UMinimapRegistrySubsystem>()->SetBounds(this);
}

void AMinimapBounds::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		UMinimapRegistrySubsystem* Registry = World->GetSubsystem<UMinimapRegistrySubsystem>();
		if (Registry->GetBounds() == this) Registry->SetBounds(nullptr);
	}
	Super::EndPlay(EndPlayReason);
}

FVector2D AMinimapBounds::WorldToNormalized(const FVector& WorldLocation, bool& bWasClamped) const
{
	const FVector Local = PlayableArea->GetComponentTransform().InverseTransformPosition(WorldLocation);
	const FVector Extent = PlayableArea->GetUnscaledBoxExtent();
	const FVector2D Raw(0.5f + Local.X / FMath::Max(2.0f * Extent.X, 1.0f), 0.5f - Local.Y / FMath::Max(2.0f * Extent.Y, 1.0f));
	const FVector2D Clamped(FMath::Clamp(Raw.X, 0.0f, 1.0f), FMath::Clamp(Raw.Y, 0.0f, 1.0f));
	bWasClamped = !Raw.Equals(Clamped, KINDA_SMALL_NUMBER);
	// Presentation orientation: rotate 90 degrees counter-clockwise, then mirror vertically.
	return FVector2D(Clamped.Y, Clamped.X);
}

float AMinimapBounds::WorldYawToMapYaw(float WorldYaw) const
{
	const float LocalYaw = WorldYaw - PlayableArea->GetComponentRotation().Yaw;
	return FMath::UnwindDegrees(-90.0f - LocalYaw);
}
