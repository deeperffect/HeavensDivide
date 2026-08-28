#include "ObjectiveInteractionComponent.h"

#include "BloodShrineWidget.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Camera/PlayerCameraManager.h"
#include "Interactable.h"
#include "Kismet/GameplayStatics.h"
#include "SurvivorPlayerController.h"

UObjectiveInteractionComponent::UObjectiveInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetWidgetSpace(EWidgetSpace::Screen);
	SetDrawAtDesiredSize(false);
	SetPivot(FVector2D(0.5f, 1.0f));
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetVisibility(false);
	SetHiddenInGame(true);
}

void UObjectiveInteractionComponent::BeginPlay()
{
	Super::BeginPlay();
	PlayerController = Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	InitializePrompt();
	StartSpawnEmergence();
	RefreshPrompt();
}

void UObjectiveInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!PlayerController) PlayerController = Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	UpdateSpawnEmergence(DeltaTime);
	ApplyPresentationSettings();
	UpdateViewportPosition();
	RefreshPrompt();
}

bool UObjectiveInteractionComponent::IsInRange(const APawn* Pawn) const
{
	if (bSpawnEmergenceActive) return false;
	const AActor* Owner = GetOwner();
	const FVector Origin = PresentationAnchor ? PresentationAnchor->GetComponentLocation() : (Owner ? Owner->GetActorLocation() : GetComponentLocation());
	return Pawn && FVector::DistSquared2D(Pawn->GetActorLocation(), Origin) <= FMath::Square(FMath::Max(1.0f, InteractionRange));
}

void UObjectiveInteractionComponent::StartSpawnEmergence()
{
	if (!bEnableSpawnEmergence || !PresentationVisual || SpawnRiseDistance <= 0.0f || SpawnRiseDuration <= 0.0f) return;
	SpawnVisualFinalRelativeLocation = PresentationVisual->GetRelativeLocation();
	SpawnVisualFinalRelativeRotation = PresentationVisual->GetRelativeRotation();
	SpawnEmergenceElapsed = 0.0f;
	bSpawnEmergenceActive = true;
	if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(PresentationVisual))
	{
		SpawnVisualOriginalCollision = Primitive->GetCollisionEnabled();
		Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	const FVector FinalWorldLocation = PresentationVisual->GetComponentLocation();
	PresentationVisual->SetWorldLocation(FinalWorldLocation - FVector::UpVector * SpawnRiseDistance);
	PresentationVisual->SetRelativeRotation(SpawnVisualFinalRelativeRotation);
	SetPromptVisible(false);
	if (SpawnSound) UGameplayStatics::PlaySoundAtLocation(this, SpawnSound, GetOwner() ? GetOwner()->GetActorLocation() : FinalWorldLocation);
}

void UObjectiveInteractionComponent::UpdateSpawnEmergence(float DeltaTime)
{
	if (!bSpawnEmergenceActive || !PresentationVisual) return;
	SpawnEmergenceElapsed += FMath::Max(0.0f, DeltaTime);
	const float Alpha = FMath::Clamp(SpawnEmergenceElapsed / FMath::Max(0.01f, SpawnRiseDuration), 0.0f, 1.0f);
	const float EasedAlpha = FMath::SmoothStep(0.0f, 1.0f, Alpha);
	const USceneComponent* Parent = PresentationVisual->GetAttachParent();
	const FVector FinalWorldLocation = Parent
		? Parent->GetComponentTransform().TransformPosition(SpawnVisualFinalRelativeLocation)
		: SpawnVisualFinalRelativeLocation;
	PresentationVisual->SetWorldLocation(FinalWorldLocation - FVector::UpVector * SpawnRiseDistance * (1.0f - EasedAlpha));
	const float Wobble = FMath::Sin(Alpha * UE_TWO_PI * 2.5f) * SpawnWobbleDegrees * (1.0f - EasedAlpha);
	PresentationVisual->SetRelativeRotation(SpawnVisualFinalRelativeRotation + FRotator(0.0f, 0.0f, Wobble));
	if (Alpha >= 1.0f) FinishSpawnEmergence();
}

void UObjectiveInteractionComponent::FinishSpawnEmergence()
{
	if (!PresentationVisual) { bSpawnEmergenceActive = false; return; }
	PresentationVisual->SetRelativeLocation(SpawnVisualFinalRelativeLocation);
	PresentationVisual->SetRelativeRotation(SpawnVisualFinalRelativeRotation);
	if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(PresentationVisual)) Primitive->SetCollisionEnabled(SpawnVisualOriginalCollision);
	bSpawnEmergenceActive = false;
	RefreshPrompt();
}

void UObjectiveInteractionComponent::RefreshPrompt()
{
	AActor* Owner = GetOwner();
	APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	const bool bInRange = IsInRange(Pawn);
	const bool bAvailable = bInRange && Owner && Owner->GetClass()->ImplementsInterface(UInteractable::StaticClass())
		&& IInteractable::Execute_CanInteract(Owner, Pawn);
	const bool bShouldBeVisible = bAvailable || (bInRange && bShowUnavailable);
	SetPromptVisible(bShouldBeVisible);
	if (!bShouldBeVisible || !ViewportPrompt) return;
	if (!bHasDisplayedContent || bDisplayedAvailable != bAvailable)
	{
		if (bAvailable) ViewportPrompt->ShowInteractionPrompt(Title);
		else ViewportPrompt->ShowUnavailablePrompt(Title, UnavailableStatus);
		bDisplayedAvailable = bAvailable;
		bHasDisplayedContent = true;
	}
}

void UObjectiveInteractionComponent::HidePrompt()
{
	SetPromptVisible(false);
}

void UObjectiveInteractionComponent::ConfigureDefaults(const FText& InTitle, float InRange, float InWorldScale, float InVerticalOffset)
{
	Title=InTitle; InteractionRange=InRange; WorldScale=InWorldScale; VerticalOffset=InVerticalOffset;
	bHasDisplayedContent=false;
}

void UObjectiveInteractionComponent::SetUnavailablePresentation(bool bShow, const FText& Status)
{
	bShowUnavailable=bShow; UnavailableStatus=Status; bHasDisplayedContent=false; RefreshPrompt();
}

void UObjectiveInteractionComponent::InitializePrompt()
{
	// This component owns interaction state and the world anchor, but no longer asks
	// UWidgetComponent's screen layer to position the prompt. The viewport widget is
	// projected explicitly, avoiding stale screen-layer geometry after hide/show.
	SetWidgetClass(nullptr);
	SetHiddenInGame(true);
	SetVisibility(false);
	if (PlayerController)
	{
		ViewportPrompt = CreateWidget<UBloodShrineWidget>(PlayerController, UBloodShrineWidget::StaticClass());
		if (ViewportPrompt)
		{
			ViewportPrompt->ConfigureForWorldSpace();
			ViewportPrompt->AddToViewport(60);
			ViewportPrompt->SetAlignmentInViewport(FVector2D(0.5f, 1.0f));
			ViewportPrompt->SetDesiredSizeInViewport(BaseDrawSize);
			ViewportPrompt->SetRenderTransformPivot(FVector2D(0.5f, 1.0f));
			ViewportPrompt->ShowInteractionPrompt(Title);
			ViewportPrompt->ForceLayoutPrepass();
			bDisplayedAvailable = true;
			bHasDisplayedContent = true;
			ViewportPrompt->SetVisibility(ESlateVisibility::Hidden);
		}
	}
	ApplyPresentationSettings();
	UpdateViewportPosition();
}

void UObjectiveInteractionComponent::ApplyPresentationSettings()
{
	const AActor* Owner=GetOwner();
	const FVector Origin=PresentationAnchor?PresentationAnchor->GetComponentLocation():(Owner?Owner->GetActorLocation():GetComponentLocation());
	const FVector DesiredWorldLocation=Origin+FVector::UpVector*VerticalOffset;
	if(const USceneComponent* Parent=GetAttachParent())SetRelativeLocation(Parent->GetComponentTransform().InverseTransformPosition(DesiredWorldLocation));
	else SetWorldLocation(DesiredWorldLocation);
	SetRelativeRotation(FRotator::ZeroRotator);
	SetRelativeScale3D(FVector::OneVector);
	if(ViewportPrompt)ViewportPrompt->SetRenderScale(FVector2D(FMath::Max(0.01f,WorldScale)));
}

void UObjectiveInteractionComponent::UpdateViewportPosition()
{
	if (!ViewportPrompt || !PlayerController) return;
	const FVector PromptWorld = GetComponentLocation();
	const APlayerCameraManager* Camera = PlayerController->PlayerCameraManager;
	if (!Camera || FVector::DotProduct(PromptWorld-Camera->GetCameraLocation(), Camera->GetActorForwardVector()) <= 0.0f)
	{
		ViewportPrompt->SetVisibility(ESlateVisibility::Hidden);
		return;
	}
	FVector2D ScreenPosition;
	// This returns DPI-aware Slate/widget coordinates, matching the coordinate space
	// expected below when bRemoveDPIScale is false.
	if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PlayerController, PromptWorld, ScreenPosition, true))
	{
		ViewportPrompt->SetVisibility(ESlateVisibility::Hidden);
		return;
	}
	ViewportPrompt->SetPositionInViewport(ScreenPosition, false);
	if (bPromptVisible) ViewportPrompt->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UObjectiveInteractionComponent::SetPromptVisible(bool bShouldShow)
{
	bPromptVisible = bShouldShow;
	if (!ViewportPrompt) return;
	if (!bShouldShow) ViewportPrompt->SetVisibility(ESlateVisibility::Hidden);
	else UpdateViewportPosition();
}
