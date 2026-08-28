#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "ObjectiveInteractionComponent.generated.h"

class APawn;
class ASurvivorPlayerController;
class UBloodShrineWidget;
class USceneComponent;
class USoundBase;
class UPrimitiveComponent;

UCLASS(ClassGroup=(Objective), meta=(BlueprintSpawnableComponent))
class HEAVENSDIVIDE_API UObjectiveInteractionComponent : public UWidgetComponent
{
	GENERATED_BODY()
public:
	UObjectiveInteractionComponent();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintPure, Category="Objective Interaction") bool IsInRange(const APawn* Pawn) const;
	UFUNCTION(BlueprintPure, Category="Objective Interaction") bool IsPromptVisible() const { return bPromptVisible; }
	UFUNCTION(BlueprintCallable, Category="Objective Interaction") void RefreshPrompt();
	UFUNCTION(BlueprintCallable, Category="Objective Interaction") void HidePrompt();
	UFUNCTION(BlueprintCallable, Category="Objective Interaction") void ApplyPresentationSettings();
	void ConfigureDefaults(const FText& InTitle, float InRange=300.0f, float InWorldScale=1.0f, float InVerticalOffset=150.0f);
	void SetPresentationVisual(USceneComponent* InVisual) { PresentationVisual = InVisual; }
	void SetUnavailablePresentation(bool bShow, const FText& Status=FText::GetEmpty());

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objective Interaction") FText Title = FText::FromString(TEXT("Objective"));
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objective Interaction", meta=(ClampMin="1.0")) float InteractionRange = 300.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objective Interaction", meta=(ClampMin="0.01")) float WorldScale = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objective Interaction") float VerticalOffset = 150.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objective Interaction") TObjectPtr<USceneComponent> PresentationAnchor;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objective Interaction|Spawn Emergence") bool bEnableSpawnEmergence = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objective Interaction|Spawn Emergence", meta=(ClampMin="0.0", Units="cm")) float SpawnRiseDistance = 300.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objective Interaction|Spawn Emergence", meta=(ClampMin="0.01", Units="s")) float SpawnRiseDuration = 0.85f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objective Interaction|Spawn Emergence", meta=(ClampMin="0.0", Units="deg")) float SpawnWobbleDegrees = 6.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objective Interaction|Spawn Emergence") TObjectPtr<USoundBase> SpawnSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objective Interaction|Spawn Emergence") TObjectPtr<USceneComponent> PresentationVisual;

private:
	void InitializePrompt();
	void StartSpawnEmergence();
	void UpdateSpawnEmergence(float DeltaTime);
	void FinishSpawnEmergence();
	void UpdateViewportPosition();
	void SetPromptVisible(bool bShouldShow);
	UPROPERTY() TObjectPtr<ASurvivorPlayerController> PlayerController;
	UPROPERTY() TObjectPtr<UBloodShrineWidget> ViewportPrompt;
	bool bShowUnavailable = false;
	bool bPromptVisible = false;
	bool bDisplayedAvailable = false;
	bool bHasDisplayedContent = false;
	bool bSpawnEmergenceActive = false;
	float SpawnEmergenceElapsed = 0.0f;
	FVector SpawnVisualFinalRelativeLocation = FVector::ZeroVector;
	FRotator SpawnVisualFinalRelativeRotation = FRotator::ZeroRotator;
	TEnumAsByte<ECollisionEnabled::Type> SpawnVisualOriginalCollision = ECollisionEnabled::NoCollision;
	FText UnavailableStatus;
	FVector2D BaseDrawSize = FVector2D(360.0f, 160.0f);
};
