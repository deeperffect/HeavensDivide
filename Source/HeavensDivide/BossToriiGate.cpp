#include "BossToriiGate.h"

#include "Components/StaticMeshComponent.h"
#include "EnemySpawner.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "RunTravelSubsystem.h"
#include "SurvivorPlayerController.h"
#include "MinimapMarkerComponent.h"
#include "ObjectiveInteractionComponent.h"
#include "TimerManager.h"

ABossToriiGate::ABossToriiGate()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	MinimapMarker = CreateDefaultSubobject<UMinimapMarkerComponent>(TEXT("MinimapMarker"));
	MinimapMarker->MarkerType = EMinimapMarkerType::BossGate;
	MinimapMarker->MarkerState = EMinimapMarkerState::Locked;
	MinimapMarker->DisplayName = FText::FromString(TEXT("Boss Torii Gate"));
	GateVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GateVisual"));
	GateVisual->SetupAttachment(SceneRoot);
	GateVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ObjectiveInteraction = CreateDefaultSubobject<UObjectiveInteractionComponent>(TEXT("ObjectiveInteraction"));
	ObjectiveInteraction->SetupAttachment(SceneRoot);
	ObjectiveInteraction->ConfigureDefaults(FText::FromString(TEXT("Enter Boss Arena")),300.0f,1.0f,240.0f);
}

void ABossToriiGate::BeginPlay()
{
	Super::BeginPlay();
	FindRequiredReferences();
	PollUnlockState();
	if(ObjectiveInteraction)ObjectiveInteraction->SetUnavailablePresentation(GateState==EBossToriiGateState::Locked,FText::FromString(TEXT("SEALED")));
	if (GateState == EBossToriiGateState::Locked) GetWorldTimerManager().SetTimer(UnlockPollTimer, this, &ABossToriiGate::PollUnlockState, 0.25f, true);
}

void ABossToriiGate::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(UnlockPollTimer);
	Super::EndPlay(EndPlayReason);
}

bool ABossToriiGate::CanInteract_Implementation(APawn* InteractingPawn) const
{
	return GateState == EBossToriiGateState::Unlocked && InteractingPawn
		&& (!PlayerController || !PlayerController->IsAnyObjectiveActive())
		&& ObjectiveInteraction && ObjectiveInteraction->IsInRange(InteractingPawn);
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
	MinimapMarker->SetMarkerVisible(false);
	if(ObjectiveInteraction)ObjectiveInteraction->HidePrompt();
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
	if(ObjectiveInteraction)ObjectiveInteraction->SetUnavailablePresentation(false);
	MinimapMarker->SetMarkerState(EMinimapMarkerState::Available);
	GetWorldTimerManager().ClearTimer(UnlockPollTimer);
	OnBossGateUnlocked.Broadcast();
	if(ObjectiveInteraction)ObjectiveInteraction->RefreshPrompt();
}
