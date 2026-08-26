#include "BossToriiGate.h"

#include "BloodShrineWidget.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "EnemySpawner.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "RunTravelSubsystem.h"
#include "SurvivorPlayerController.h"
#include "TimerManager.h"

ABossToriiGate::ABossToriiGate()
{
	PrimaryActorTick.bCanEverTick = true;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	GateVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GateVisual"));
	GateVisual->SetupAttachment(SceneRoot);
	GateVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(SceneRoot);
	InteractionSphere->InitSphereRadius(InteractionRadius);
	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	// Preserve the native subobject identity already serialized by BP_BossToriiGate.
	InteractionPromptComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractionPrompt"));
	InteractionPromptComponent->SetupAttachment(SceneRoot);
	InteractionPromptComponent->SetWidgetSpace(EWidgetSpace::World);
	InteractionPromptComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	InteractionPromptComponent->SetVisibility(false);
}

void ABossToriiGate::BeginPlay()
{
	Super::BeginPlay();
	InteractionSphere->SetSphereRadius(FMath::Max(1.0f, InteractionRadius));
	FindRequiredReferences();
	CreateInteractionPrompt();
	PollUnlockState();
	if (GateState == EBossToriiGateState::Locked) GetWorldTimerManager().SetTimer(UnlockPollTimer, this, &ABossToriiGate::PollUnlockState, 0.25f, true);
}

void ABossToriiGate::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(UnlockPollTimer);
	Super::EndPlay(EndPlayReason);
}

void ABossToriiGate::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GateState != EBossToriiGateState::TravelCommitted) UpdateInactivePrompt();
	FaceInteractionPromptToCamera();
}

bool ABossToriiGate::CanInteract_Implementation(APawn* InteractingPawn) const
{
	return GateState == EBossToriiGateState::Unlocked && InteractingPawn
		&& (!PlayerController || !PlayerController->IsAnyObjectiveActive())
		&& FVector::DistSquared2D(InteractingPawn->GetActorLocation(), GetActorLocation()) <= FMath::Square(FMath::Max(1.0f, InteractionRadius));
}

void ABossToriiGate::Interact_Implementation(APawn* InteractingPawn)
{
	if (!CanInteract_Implementation(InteractingPawn)) return;
	FindRequiredReferences();
	if (!EnemySpawner || !PlayerController || PlayerController->IsPlayerDead()) return;
	URunTravelSubsystem* TravelState = PlayerController->GetGameInstance() ? PlayerController->GetGameInstance()->GetSubsystem<URunTravelSubsystem>() : nullptr;
	if (!TravelState || !TravelState->CaptureFromController(PlayerController)) return;
	const FSoftObjectPath MapPath = BossArenaLevel.ToSoftObjectPath();
	if (!MapPath.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[BossGate] BossArenaLevel is not configured; travel aborted."));
		TravelState->ClearSnapshot();
		return;
	}
	GateState = EBossToriiGateState::TravelCommitted;
	InteractionPromptComponent->SetVisibility(false);
	EnemySpawner->FreezeRunTime();
	EnemySpawner->SetSpawningEnabled(false);
	OnBossGateTravelStarted.Broadcast();
	const FString PackageName = MapPath.GetLongPackageName();
	UE_LOG(LogTemp, Log, TEXT("[BossGate] Travel committed to %s"), *PackageName);
	UGameplayStatics::OpenLevel(this, FName(*PackageName));
}

void ABossToriiGate::FindRequiredReferences()
{
	PlayerController = Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	if (!EnemySpawner && GetWorld()) for (TActorIterator<AEnemySpawner> It(GetWorld()); It; ++It) { EnemySpawner = *It; break; }
}

void ABossToriiGate::CreateInteractionPrompt()
{
	if (!InteractionPromptComponent) return;
	InteractionPromptComponent->SetRelativeLocation(FVector(0.0f, 0.0f, PromptVerticalOffset));
	InteractionPromptComponent->SetDrawSize(FVector2D(FMath::Max(1.0f, PromptDrawSize.X), FMath::Max(1.0f, PromptDrawSize.Y)));
	InteractionPromptComponent->SetRelativeScale3D(FVector(FMath::Max(0.01f, PromptWorldScale)));
	InteractionPromptComponent->SetTranslucentSortPriority(PromptTranslucencySortPriority);
	InteractionPromptComponent->SetWidgetClass(UBloodShrineWidget::StaticClass());
	InteractionPromptComponent->InitWidget();
	if (UBloodShrineWidget* Prompt = Cast<UBloodShrineWidget>(InteractionPromptComponent->GetUserWidgetObject()))
	{
		Prompt->ConfigureForWorldSpace();
		Prompt->ShowUnavailablePrompt(FText::FromString(TEXT("TORII GATE")), FText::FromString(TEXT("SEALED")));
	}
	InteractionPromptComponent->SetVisibility(false);
}

void ABossToriiGate::UpdateInactivePrompt()
{
	if (!InteractionPromptComponent || !PlayerController) return;
	APawn* Pawn = PlayerController->GetPawn();
	const bool bShowPrompt = ShouldShowPrompt(Pawn);
	InteractionPromptComponent->SetVisibility(bShowPrompt);
	if (!bShowPrompt) return;
	if (UBloodShrineWidget* Prompt = Cast<UBloodShrineWidget>(InteractionPromptComponent->GetUserWidgetObject()))
	{
		if (GateState == EBossToriiGateState::Unlocked) Prompt->ShowInteractionPrompt(FText::FromString(TEXT("ENTER BOSS ARENA")));
		else Prompt->ShowUnavailablePrompt(FText::FromString(TEXT("TORII GATE")), FText::FromString(TEXT("SEALED")));
	}
}

void ABossToriiGate::FaceInteractionPromptToCamera()
{
	if (!InteractionPromptComponent || !InteractionPromptComponent->IsVisible() || !PlayerController) return;
	if (APlayerCameraManager* Camera = PlayerController->PlayerCameraManager)
	{
		InteractionPromptComponent->SetWorldRotation((Camera->GetCameraLocation() - InteractionPromptComponent->GetComponentLocation()).Rotation());
	}
}

bool ABossToriiGate::ShouldShowPrompt(APawn* InteractingPawn) const
{
	return GateState != EBossToriiGateState::TravelCommitted && InteractingPawn
		&& FVector::DistSquared2D(InteractingPawn->GetActorLocation(), GetActorLocation()) <= FMath::Square(FMath::Max(1.0f, InteractionRadius));
}

void ABossToriiGate::PollUnlockState()
{
	if (GateState != EBossToriiGateState::Locked) return;
	if (!EnemySpawner) FindRequiredReferences();
	if (EnemySpawner && EnemySpawner->GetRunTimeSeconds() >= UnlockRunTimeSeconds) UnlockGate();
}

void ABossToriiGate::UnlockGate()
{
	if (GateState != EBossToriiGateState::Locked) return;
	GateState = EBossToriiGateState::Unlocked;
	GetWorldTimerManager().ClearTimer(UnlockPollTimer);
	OnBossGateUnlocked.Broadcast();
	UpdateInactivePrompt();
}
