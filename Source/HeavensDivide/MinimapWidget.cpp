#include "MinimapWidget.h"
#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "MinimapBounds.h"
#include "MinimapMarkerComponent.h"
#include "MinimapRegistrySubsystem.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "SurvivorPlayerController.h"

void UMinimapWidget::InitializeMinimap(ASurvivorPlayerController* InController)
{
	Controller = InController;
}

int32 UMinimapWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Draw, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const
{
	Layer = Super::NativePaint(Args, Geo, Cull, Draw, Layer, Style, bEnabled);
	const FVector2D Size = Geo.GetLocalSize();
	const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
	FSlateDrawElement::MakeBox(Draw, ++Layer, Geo.ToPaintGeometry(), MinimapBackground.GetResourceObject() ? &MinimapBackground : White, ESlateDrawEffect::None, MinimapBackground.GetResourceObject() ? FLinearColor::White : BackgroundColor);
	const FVector2D InnerSize(FMath::Max(1.0f, Size.X - 2.0f * ContentPadding), FMath::Max(1.0f, Size.Y - 2.0f * ContentPadding));

	const UWorld* World = GetWorld();
	const UMinimapRegistrySubsystem* Registry = World ? World->GetSubsystem<UMinimapRegistrySubsystem>() : nullptr;
	const AMinimapBounds* Bounds = Registry ? Registry->GetBounds() : nullptr;
	if (!Bounds) return Layer;

	for (const TWeakObjectPtr<UMinimapMarkerComponent>& Entry : Registry->GetMarkers())
	{
		const UMinimapMarkerComponent* Marker = Entry.Get();
		if (!Marker || !Marker->IsMarkerVisible()) continue;
		bool bClamped = false;
		const FVector2D Normalized = Bounds->WorldToNormalized(Marker->GetMarkerWorldLocation(), bClamped);
		const FVector2D MarkerSize = Marker->MarkerSize;
		const FVector2D Pos = FVector2D(ContentPadding) + Normalized * InnerSize - MarkerSize * 0.5f;
		FLinearColor Color = Marker->GetMarkerType() == EMinimapMarkerType::BossGate ? FLinearColor(0.75f, 0.2f, 0.85f, 1.0f) : FLinearColor(1.0f, 0.7f, 0.12f, 1.0f);
		if (Marker->GetMarkerState() == EMinimapMarkerState::Locked) Color = FLinearColor(0.35f, 0.35f, 0.42f, 1.0f);
		else if (Marker->GetMarkerState() == EMinimapMarkerState::Active) Color = FLinearColor(1.0f, 0.25f, 0.12f, 1.0f);
		if (bClamped) Color.A = 0.65f;
		FSlateDrawElement::MakeBox(Draw, ++Layer, Geo.ToPaintGeometry(MarkerSize, FSlateLayoutTransform(Pos)), Marker->Icon.GetResourceObject() ? &Marker->Icon : White, ESlateDrawEffect::None, Color);
	}

	const ASurvivorPlayerController* PC = Controller.Get();
	const ACharacterBase* Player = PC && PC->GetCharacterManager() ? PC->GetCharacterManager()->GetActiveCharacter() : nullptr;
	if (Player)
	{
		bool bClamped = false;
		const FVector2D Center = FVector2D(ContentPadding) + Bounds->WorldToNormalized(Player->GetActorLocation(), bClamped) * InnerSize;
		const float Angle = FMath::DegreesToRadians(Bounds->WorldYawToMapYaw(Player->GetVisualForwardVector().Rotation().Yaw));
		const FVector2D Forward(FMath::Cos(Angle), -FMath::Sin(Angle));
		const FVector2D Right(-Forward.Y, Forward.X);
		TArray<FVector2D> Points{Center + Forward * 10.0f, Center - Forward * 7.0f + Right * 6.0f, Center - Forward * 7.0f - Right * 6.0f, Center + Forward * 10.0f};
		FSlateDrawElement::MakeLines(Draw, ++Layer, Geo.ToPaintGeometry(), Points, ESlateDrawEffect::None, bClamped ? FLinearColor(0.35f,0.8f,1.0f,0.7f) : FLinearColor(0.2f,0.85f,1.0f,1.0f), true, 3.0f);
	}
	return Layer;
}
