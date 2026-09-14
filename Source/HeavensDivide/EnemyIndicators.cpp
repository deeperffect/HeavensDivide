#include "EnemyBase.h"

#include "Components/WidgetComponent.h"
#include "EnemyHealthBarWidget.h"
#include "EnemyMarkIndicatorWidget.h"
#include "EnemyStatusEffectComponent.h"
#include "EnemyStatusIndicatorWidget.h"
#include "HealthComponent.h"

void AEnemyBase::InitializeStatusIndicators()
{
	if (BleedStatusWidgetComponent)
	{
		BleedStatusWidgetComponent->SetDrawSize(StatusIndicatorDrawSize);
	}
	if (PoisonStatusWidgetComponent)
	{
		PoisonStatusWidgetComponent->SetDrawSize(StatusIndicatorDrawSize);
	}
	UpdateStatusIndicatorLayout();
}

void AEnemyBase::HandleStatusStacksChanged(EEnemyStatusEffect Status, int32 StackCount)
{
	UWidgetComponent* Component = Status == EEnemyStatusEffect::Bleed ? BleedStatusWidgetComponent.Get() : PoisonStatusWidgetComponent.Get();
	if (!Component) return;
	const bool bActive = StackCount > 0;
	Component->SetHiddenInGame(!bActive);
	Component->SetVisibility(bActive);
	if (bActive)
	{
		if (UEnemyStatusIndicatorWidget* Widget = Cast<UEnemyStatusIndicatorWidget>(Component->GetUserWidgetObject()))
		{
			Widget->SetStatusPresentation(Status, StackCount, bShowStatusStackCountAtOne, StatusStackFontSize,
				Status == EEnemyStatusEffect::Bleed ? BleedStatusIcon.Get() : PoisonStatusIcon.Get());
		}
	}
	UpdateStatusIndicatorLayout();
}

void AEnemyBase::UpdateStatusIndicatorLayout()
{
	if (!HealthBarWidgetComponent) return;

	// Share the bar's projected world anchor. Canvas pivots provide fixed screen-space
	// offsets, so camera rotation/perspective cannot change icon spacing or order.
	const FVector Anchor = HealthBarWidgetComponent->GetComponentLocation();
	const FVector2D BarSize = HealthBarWidgetComponent->GetDrawSize();
	const FVector2D BarPivot = HealthBarWidgetComponent->GetPivot();
	const float Width = FMath::Max(1.0, StatusIndicatorDrawSize.X);
	const float Height = FMath::Max(1.0, StatusIndicatorDrawSize.Y);
	const float HalfSpacing = FMath::Max(0.0f, StatusIndicatorSpacing) * 0.5f;
	const float BarCenterX = (0.5f - BarPivot.X) * BarSize.X;
	const float PivotY = 1.0f + (BarPivot.Y * BarSize.Y + FMath::Max(0.0f, StatusIndicatorHealthBarGap)) / Height;
	if (BleedStatusWidgetComponent)
	{
		BleedStatusWidgetComponent->SetWorldLocation(Anchor);
		BleedStatusWidgetComponent->SetDrawAtDesiredSize(false);
		BleedStatusWidgetComponent->SetDrawSize(FVector2D(Width, Height));
		BleedStatusWidgetComponent->SetPivot(FVector2D(1.0f + (HalfSpacing - BarCenterX) / Width, PivotY));
	}
	if (PoisonStatusWidgetComponent)
	{
		PoisonStatusWidgetComponent->SetWorldLocation(Anchor);
		PoisonStatusWidgetComponent->SetDrawAtDesiredSize(false);
		PoisonStatusWidgetComponent->SetDrawSize(FVector2D(Width, Height));
		PoisonStatusWidgetComponent->SetPivot(FVector2D(-(HalfSpacing + BarCenterX) / Width, PivotY));
	}
}

void AEnemyBase::InitializeHealthBar()
{
	if (!ShouldUseWorldHealthBar())
	{
		HideHealthBar();
		return;
	}
	if (!HealthBarWidgetComponent)
	{
		return;
	}

	HealthBarWidgetComponent->SetRelativeLocation(FVector(0.0f, 0.0f, HealthBarHeightOffset));
	HealthBarWidgetComponent->SetDrawSize(HealthBarDrawSize);
	UpdateHealthBarVisibility(HealthComponent ? HealthComponent->GetHealthPercent() : 0.0f);

	if (HealthBarWidgetClass)
	{
		HealthBarWidgetComponent->SetWidgetClass(HealthBarWidgetClass);
	}

	HealthBarWidgetComponent->InitWidget();

	UEnemyHealthBarWidget* HealthBarWidget = Cast<UEnemyHealthBarWidget>(HealthBarWidgetComponent->GetUserWidgetObject());
	if (!HealthBarWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("EnemyBase %s has no EnemyHealthBarWidget assigned."), *GetNameSafe(this));
		UpdateHealthBarVisibility(0.0f);
		return;
	}

	HealthBarWidget->InitializeFromHealthComponent(HealthComponent);
	UpdateHealthBarVisibility(HealthComponent ? HealthComponent->GetHealthPercent() : 0.0f);
}

void AEnemyBase::UpdateHealthBarVisibility(float HealthPercent)
{
	if (!ShouldUseWorldHealthBar())
	{
		HideHealthBar();
		return;
	}
	if (!HealthBarWidgetComponent)
	{
		return;
	}

	const bool bShouldShowHealthBar = !bIsDead
		&& HealthPercent > KINDA_SMALL_NUMBER
		&& !FMath::IsNearlyEqual(HealthPercent, 1.0f, KINDA_SMALL_NUMBER);
	HealthBarWidgetComponent->SetHiddenInGame(!bShouldShowHealthBar);
	HealthBarWidgetComponent->SetVisibility(bShouldShowHealthBar, true);
	HealthBarWidgetComponent->SetComponentTickEnabled(bShouldShowHealthBar);

	if (bShouldShowHealthBar)
	{
		HealthBarWidgetComponent->Activate(true);
	}
	else
	{
		HealthBarWidgetComponent->Deactivate();
	}
}

void AEnemyBase::HideHealthBar()
{
	if (!HealthBarWidgetComponent)
	{
		return;
	}

	if (UUserWidget* HealthBarWidget = HealthBarWidgetComponent->GetUserWidgetObject())
	{
		HealthBarWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	HealthBarWidgetComponent->SetHiddenInGame(true);
	HealthBarWidgetComponent->SetVisibility(false, true);
	HealthBarWidgetComponent->SetComponentTickEnabled(false);
	HealthBarWidgetComponent->Deactivate();
	HealthBarWidgetComponent->SetWidget(nullptr);
}

void AEnemyBase::InitializeMarkIndicator()
{
	if (!MarkIndicatorWidgetComponent)
	{
		return;
	}

	MarkIndicatorWidgetComponent->SetRelativeLocation(MarkIndicatorRelativeLocation);
	MarkIndicatorWidgetComponent->SetDrawSize(MarkIndicatorDrawSize);
	MarkIndicatorWidgetComponent->SetRelativeScale3D(FVector(MarkIndicatorScale));

	if (MarkIndicatorWidgetClass)
	{
		MarkIndicatorWidgetComponent->SetWidgetClass(MarkIndicatorWidgetClass);
	}

	MarkIndicatorWidgetComponent->InitWidget();
	UpdateMarkIndicatorVisibility();
}

void AEnemyBase::UpdateMarkIndicatorVisibility()
{
	if (!MarkIndicatorWidgetComponent)
	{
		return;
	}

	const bool bShouldShowMarkIndicator = bIsMarked && !bIsDead;
	MarkIndicatorWidgetComponent->SetHiddenInGame(!bShouldShowMarkIndicator);
	MarkIndicatorWidgetComponent->SetVisibility(bShouldShowMarkIndicator, true);
	MarkIndicatorWidgetComponent->SetComponentTickEnabled(false);

	if (UUserWidget* MarkWidget = MarkIndicatorWidgetComponent->GetUserWidgetObject())
	{
		MarkWidget->SetVisibility(bShouldShowMarkIndicator ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (bShouldShowMarkIndicator)
	{
		MarkIndicatorWidgetComponent->Activate(true);
	}
	else
	{
		MarkIndicatorWidgetComponent->Deactivate();
	}
}

void AEnemyBase::HideMarkIndicator()
{
	if (!MarkIndicatorWidgetComponent)
	{
		return;
	}

	if (UUserWidget* MarkWidget = MarkIndicatorWidgetComponent->GetUserWidgetObject())
	{
		MarkWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	MarkIndicatorWidgetComponent->SetHiddenInGame(true);
	MarkIndicatorWidgetComponent->SetVisibility(false, true);
	MarkIndicatorWidgetComponent->SetComponentTickEnabled(false);
	MarkIndicatorWidgetComponent->Deactivate();
}

