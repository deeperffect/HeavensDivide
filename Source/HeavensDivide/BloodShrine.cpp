// Copyright Epic Games, Inc. All Rights Reserved.

#include "BloodShrine.h"

#include "BloodShrineWidget.h"
#include "Components/StaticMeshComponent.h"
#include "EnemyBase.h"
#include "EnemySpawner.h"
#include "EngineUtils.h"
#include "HealthComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "SurvivorPlayerController.h"
#include "MinimapMarkerComponent.h"
#include "ObjectiveInteractionComponent.h"
#include "TimerManager.h"

ABloodShrine::ABloodShrine()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	MinimapMarker = CreateDefaultSubobject<UMinimapMarkerComponent>(TEXT("MinimapMarker"));
	MinimapMarker->DisplayName = FText::FromString(TEXT("Blood Shrine"));

	ShrineMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShrineMesh"));
	ShrineMesh->SetupAttachment(SceneRoot);
	ShrineMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaceholderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (PlaceholderMesh.Succeeded())
	{
		ShrineMesh->SetStaticMesh(PlaceholderMesh.Object);
		ShrineMesh->SetRelativeScale3D(FVector(1.25f, 1.25f, 2.0f));
	}

	ObjectiveInteraction = CreateDefaultSubobject<UObjectiveInteractionComponent>(TEXT("ObjectiveInteraction"));
	ObjectiveInteraction->SetupAttachment(SceneRoot);
	ObjectiveInteraction->ConfigureDefaults(FText::FromString(TEXT("Blood Shrine")),300.0f,1.0f,240.0f);
}

void ABloodShrine::BeginPlay()
{
	Super::BeginPlay();
	FindRequiredReferences();
	CreateStatusWidget();
}

void ABloodShrine::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	EndChallenge();
	GetWorldTimerManager().ClearTimer(RewardTimer);

	if (PlayerController && PlayerController->GetPlayerHealthComponent())
	{
		PlayerController->GetPlayerHealthComponent()->OnDeath.RemoveDynamic(this, &ABloodShrine::HandlePlayerDeath);
	}

	if (StatusWidget)
	{
		StatusWidget->RemoveFromParent();
	}

	Super::EndPlay(EndPlayReason);
}

bool ABloodShrine::CanInteract_Implementation(APawn* InteractingPawn) const
{
	const bool bStateAllowsInteraction = ShrineState == EBloodShrineState::Inactive
		|| (bAllowReactivation && (ShrineState == EBloodShrineState::Success || ShrineState == EBloodShrineState::Failed));
	return bStateAllowsInteraction
		&& InteractingPawn
		&& (!PlayerController || !PlayerController->IsAnyObjectiveActive())
		&& ObjectiveInteraction && ObjectiveInteraction->IsInRange(InteractingPawn);
}

void ABloodShrine::Interact_Implementation(APawn* InteractingPawn)
{
	ActivateShrine(InteractingPawn);
}

bool ABloodShrine::ActivateShrine(APawn* InteractingPawn)
{
	if (!CanInteract_Implementation(InteractingPawn))
	{
		return false;
	}

	FindRequiredReferences();
	if (!EnemySpawner || !PlayerController || PlayerController->IsPlayerDead())
	{
		UE_LOG(LogTemp, Warning, TEXT("Blood Shrine activation failed: Spawner=%s Controller=%s PlayerDead=%s"),
			*GetNameSafe(EnemySpawner), *GetNameSafe(PlayerController), PlayerController && PlayerController->IsPlayerDead() ? TEXT("true") : TEXT("false"));
		return false;
	}
	if (!PlayerController->TryBeginObjective(this))
	{
		return false;
	}

	GetWorldTimerManager().ClearTimer(RewardTimer);
	bRewardRequested = false;
	CurrentBlood = 0;
	ShrineState = EBloodShrineState::Active;
	MinimapMarker->SetMarkerState(EMinimapMarkerState::Active);
	if (ObjectiveInteraction) ObjectiveInteraction->HidePrompt();
	ChallengeEndTime = GetWorld()->GetTimeSeconds() + FMath::Max(0.1f, ChallengeDuration);

	EnemySpawner->SetSpawnPressureModifier(GetPressureModifierId(), SpawnPressureMultiplier);
	FEnemySpawnModifierContext BloodboundContext;
	BloodboundContext.bMakeBloodbound = true;
	BloodboundContext.HealthMultiplier = BloodboundHealthMultiplier;
	BloodboundContext.DamageMultiplier = BloodboundDamageMultiplier;
	BloodboundContext.MovementSpeedMultiplier = BloodboundMovementSpeedMultiplier;
	BloodboundContext.bDropsXP = bBloodboundDropsXP;
	EnemySpawner->SetEnemySpawnModifierContext(GetPressureModifierId(), BloodboundContext);
	EnemySpawner->OnEnemyBecameBloodbound.AddUniqueDynamic(this, &ABloodShrine::HandleEnemyBecameBloodbound);
	EnemySpawner->ConvertRandomAliveEnemiesToBloodbound(BloodboundContext, InitialBloodboundConversionPercent);
	EnemySpawner->OnEnemyKilled.AddUniqueDynamic(this, &ABloodShrine::HandleEnemyKilled);
	if (UHealthComponent* PlayerHealth = PlayerController->GetPlayerHealthComponent())
	{
		PlayerHealth->OnDeath.AddUniqueDynamic(this, &ABloodShrine::HandlePlayerDeath);
	}

	GetWorldTimerManager().SetTimer(ChallengeUpdateTimer, this, &ABloodShrine::UpdateChallenge, 0.1f, true);
	if (StatusWidget)
	{
		StatusWidget->ShowChallenge(CurrentBlood, RequiredBlood, GetTimeRemaining());
	}

	OnShrineActivated.Broadcast();
	ReceiveShrineActivated();
	OnProgressChanged.Broadcast(CurrentBlood, RequiredBlood);
	ReceiveProgressChanged(CurrentBlood, RequiredBlood);
	return true;
}

EBloodShrineState ABloodShrine::GetShrineState() const
{
	return ShrineState;
}

int32 ABloodShrine::GetCurrentBlood() const
{
	return CurrentBlood;
}

int32 ABloodShrine::GetCurrentKills() const
{
	return CurrentBlood;
}

float ABloodShrine::GetTimeRemaining() const
{
	return ShrineState == EBloodShrineState::Active && GetWorld()
		? FMath::Max(0.0f, static_cast<float>(ChallengeEndTime - GetWorld()->GetTimeSeconds()))
		: 0.0f;
}

void ABloodShrine::HandleEnemyKilled(AEnemyBase* Enemy)
{
	if (ShrineState != EBloodShrineState::Active || !Enemy || !Enemy->IsBloodbound())
	{
		return;
	}

	CurrentBlood = FMath::Min(CurrentBlood + Enemy->GetBloodValue(), FMath::Max(1, RequiredBlood));
	OnProgressChanged.Broadcast(CurrentBlood, RequiredBlood);
	ReceiveProgressChanged(CurrentBlood, RequiredBlood);
	if (StatusWidget)
	{
		StatusWidget->ShowChallenge(CurrentBlood, RequiredBlood, GetTimeRemaining());
	}

	if (CurrentBlood >= FMath::Max(1, RequiredBlood))
	{
		SucceedChallenge();
	}
}

void ABloodShrine::HandleEnemyBecameBloodbound(AEnemyBase* Enemy)
{
	if (ShrineState == EBloodShrineState::Active && IsValid(Enemy))
	{
		OwnedBloodboundEnemies.AddUnique(Enemy);
	}
}

void ABloodShrine::HandlePlayerDeath()
{
	if (ShrineState == EBloodShrineState::Active)
	{
		FailChallenge(false);
	}
}

void ABloodShrine::UpdateChallenge()
{
	if (ShrineState != EBloodShrineState::Active)
	{
		return;
	}

	const float Remaining = GetTimeRemaining();
	if (StatusWidget)
	{
		StatusWidget->ShowChallenge(CurrentBlood, RequiredBlood, Remaining);
	}

	if (Remaining <= 0.0f)
	{
		FailChallenge(true);
	}
}

void ABloodShrine::SucceedChallenge()
{
	if (ShrineState != EBloodShrineState::Active)
	{
		return;
	}

	EndChallenge();
	ShrineState = EBloodShrineState::Success;
	MinimapMarker->SetMarkerState(bAllowReactivation ? EMinimapMarkerState::Available : EMinimapMarkerState::Completed);
	if (StatusWidget) StatusWidget->ShowResult(true);
	OnShrineSucceeded.Broadcast();
	ReceiveShrineSucceeded();

	if (ResultDisplayDuration > 0.0f)
	{
		GetWorldTimerManager().SetTimer(RewardTimer, this, &ABloodShrine::RequestReward, ResultDisplayDuration, false);
	}
	else
	{
		RequestReward();
	}
}

void ABloodShrine::FailChallenge(bool bShowFailure)
{
	if (ShrineState != EBloodShrineState::Active)
	{
		return;
	}

	EndChallenge();
	ShrineState = EBloodShrineState::Failed;
	MinimapMarker->SetMarkerState(bAllowReactivation ? EMinimapMarkerState::Available : EMinimapMarkerState::Failed);
	if (StatusWidget)
	{
		if (bShowFailure) StatusWidget->ShowResult(false);
		else StatusWidget->HideStatus();
	}
	OnShrineFailed.Broadcast();
	ReceiveShrineFailed();

	if (bShowFailure && ResultDisplayDuration > 0.0f)
	{
		GetWorldTimerManager().SetTimer(RewardTimer, StatusWidget.Get(), &UBloodShrineWidget::HideStatus, ResultDisplayDuration, false);
	}
}

void ABloodShrine::EndChallenge()
{
	GetWorldTimerManager().ClearTimer(ChallengeUpdateTimer);
	if (EnemySpawner)
	{
		EnemySpawner->OnEnemyKilled.RemoveDynamic(this, &ABloodShrine::HandleEnemyKilled);
		EnemySpawner->OnEnemyBecameBloodbound.RemoveDynamic(this, &ABloodShrine::HandleEnemyBecameBloodbound);
		EnemySpawner->RemoveSpawnPressureModifier(GetPressureModifierId());
		EnemySpawner->RemoveEnemySpawnModifierContext(GetPressureModifierId());
	}
	RevertOwnedBloodboundEnemies();
	if (PlayerController)
	{
		PlayerController->EndObjective(this);
	}
}

void ABloodShrine::RevertOwnedBloodboundEnemies()
{
	for (const TWeakObjectPtr<AEnemyBase>& EnemyPtr : OwnedBloodboundEnemies)
	{
		AEnemyBase* Enemy = EnemyPtr.Get();
		if (IsValid(Enemy) && !Enemy->IsActorBeingDestroyed() && !Enemy->IsDead())
		{
			Enemy->RemoveBloodbound();
		}
	}
	OwnedBloodboundEnemies.Reset();
}

void ABloodShrine::RequestReward()
{
	if (bRewardRequested || ShrineState != EBloodShrineState::Success)
	{
		return;
	}

	bRewardRequested = true;
	if (StatusWidget) StatusWidget->HideStatus();
	OnBloodShrineRewardRequested.Broadcast(this);
	if (PlayerController)
	{
		PlayerController->RequestBloodShrineUpgradeReward(FMath::Max(1, RewardUpgradeChoices));
	}
}

void ABloodShrine::FindRequiredReferences()
{
	PlayerController = Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	if (!EnemySpawner && GetWorld())
	{
		for (TActorIterator<AEnemySpawner> It(GetWorld()); It; ++It)
		{
			EnemySpawner = *It;
			break;
		}
	}
}

void ABloodShrine::CreateStatusWidget()
{
	if (!StatusWidget && PlayerController)
	{
		StatusWidget = CreateWidget<UBloodShrineWidget>(PlayerController, UBloodShrineWidget::StaticClass());
		if (StatusWidget)
		{
			StatusWidget->AddToViewport(50);
			StatusWidget->HideStatus();
		}
	}
}

FName ABloodShrine::GetPressureModifierId() const
{
	return FName(*FString::Printf(TEXT("BloodShrine.%s"), *GetPathName()));
}
