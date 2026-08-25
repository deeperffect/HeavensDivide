// Copyright Epic Games, Inc. All Rights Reserved.

#include "TwinSoulTrial.h"

#include "BloodShrineWidget.h"
#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "EnemyBase.h"
#include "EnemySpawner.h"
#include "EngineUtils.h"
#include "HealthComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "SurvivorPlayerController.h"
#include "UObject/ConstructorHelpers.h"

ATwinSoulTrial::ATwinSoulTrial()
{
	PrimaryActorTick.bCanEverTick = true;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	PortalMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PortalMesh"));
	PortalMesh->SetupAttachment(SceneRoot);
	PortalMesh->SetRelativeScale3D(FVector(1.5f, 1.5f, 3.0f));
	if (CubeMesh.Succeeded()) PortalMesh->SetStaticMesh(CubeMesh.Object);

	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(SceneRoot);
	InteractionSphere->InitSphereRadius(300.0f);
	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	InteractionPromptComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractionPromptComponent"));
	InteractionPromptComponent->SetupAttachment(SceneRoot);
	InteractionPromptComponent->SetWidgetSpace(EWidgetSpace::World);
	InteractionPromptComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	InteractionPromptComponent->SetVisibility(false);

	TrialFloor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TrialFloor"));
	TrialFloor->SetupAttachment(SceneRoot);
	TrialFloor->SetRelativeLocation(TrialArenaOffset);
	TrialFloor->SetRelativeScale3D(FVector(30.0f, 30.0f, 0.5f));
	if (CubeMesh.Succeeded()) TrialFloor->SetStaticMesh(CubeMesh.Object);

	const FVector WallLocations[] = {
		TrialArenaOffset + FVector(0, 3000, 300), TrialArenaOffset + FVector(0, -3000, 300),
		TrialArenaOffset + FVector(3000, 0, 300), TrialArenaOffset + FVector(-3000, 0, 300)};
	const FVector WallScales[] = {FVector(30, 0.5f, 3), FVector(30, 0.5f, 3), FVector(0.5f, 30, 3), FVector(0.5f, 30, 3)};
	for (int32 Index = 0; Index < 4; ++Index)
	{
		UStaticMeshComponent* Wall = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("TrialWall%d"), Index));
		Wall->SetupAttachment(SceneRoot);
		Wall->SetRelativeLocation(WallLocations[Index]);
		Wall->SetRelativeScale3D(WallScales[Index]);
		if (CubeMesh.Succeeded()) Wall->SetStaticMesh(CubeMesh.Object);
		TrialWalls.Add(Wall);
	}
}

void ATwinSoulTrial::BeginPlay()
{
	Super::BeginPlay();
	FindReferences();
	CreateInteractionPrompt();
}

void ATwinSoulTrial::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (TrialState == ETwinSoulTrialState::Inactive)
	{
		UpdateInactivePrompt();
	}
	FaceInteractionPromptToCamera();
}

void ATwinSoulTrial::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CleanupTargetBindings();
	if (PlayerController)
	{
		PlayerController->EndObjective(this);
	}
	if (PlayerController)
	{
		PlayerController->OnTwinSoulRewardCompleted.RemoveDynamic(this, &ATwinSoulTrial::HandleRewardCompleted);
		if (UHealthComponent* Health = PlayerController->GetPlayerHealthComponent())
		{
			Health->OnDeath.RemoveDynamic(this, &ATwinSoulTrial::HandlePlayerDeath);
		}
	}
	Super::EndPlay(EndPlayReason);
}

bool ATwinSoulTrial::CanInteract_Implementation(APawn* InteractingPawn) const
{
	return TrialState == ETwinSoulTrialState::Inactive && InteractingPawn
		&& (!PlayerController || !PlayerController->IsAnyObjectiveActive())
		&& FVector::DistSquared2D(InteractingPawn->GetActorLocation(), GetActorLocation()) <= FMath::Square(300.0f);
}

void ATwinSoulTrial::Interact_Implementation(APawn* InteractingPawn)
{
	EnterTrial(InteractingPawn);
}

bool ATwinSoulTrial::EnterTrial(APawn* InteractingPawn)
{
	if (!CanInteract_Implementation(InteractingPawn)) return false;
	FindReferences();
	if (!MainSpawner || !PlayerController || PlayerController->IsPlayerDead()) return false;
	UCharacterManagerComponent* Manager = PlayerController->GetCharacterManager();
	ACharacterBase* ActiveCharacter = Manager ? Manager->GetActiveCharacter() : nullptr;
	if (!ActiveCharacter) return false;
	if (!PlayerController->TryBeginObjective(this)) return false;

	ReturnTransform = ActiveCharacter->GetActorTransform();
	ReturnTransform.AddToTranslation(ReturnOffset);
	TrialState = ETwinSoulTrialState::TrialActive;
	bCrimsonDead = false;
	bVioletDead = false;
	MainSpawner->SetTrialSuspended(true);
	FVector TrialLocation=GetActorLocation()+TrialArenaOffset+TrialPlayerOffset;
	if(TrialFloor)
	{
		const float FloorTop=TrialFloor->Bounds.Origin.Z+TrialFloor->Bounds.BoxExtent.Z;
		const UCapsuleComponent* Capsule=ActiveCharacter->GetCapsuleComponent();
		TrialLocation.Z=FloorTop+(Capsule?Capsule->GetScaledCapsuleHalfHeight():0.0f)+2.0f;
	}
	ActiveCharacter->SetActorLocation(TrialLocation, false, nullptr, ETeleportType::TeleportPhysics);
	if (!SpawnTargets())
	{
		TrialState = ETwinSoulTrialState::Inactive;
		ActiveCharacter->SetActorTransform(ReturnTransform, false, nullptr, ETeleportType::TeleportPhysics);
		MainSpawner->SetTrialSuspended(false);
		PlayerController->EndObjective(this);
		return false;
	}

	if (UHealthComponent* Health = PlayerController->GetPlayerHealthComponent()) Health->OnDeath.AddUniqueDynamic(this, &ATwinSoulTrial::HandlePlayerDeath);
	PlayerController->OnTwinSoulRewardCompleted.AddUniqueDynamic(this, &ATwinSoulTrial::HandleRewardCompleted);
	if (InteractionPromptComponent) InteractionPromptComponent->SetVisibility(false);
	OnTrialEntered.Broadcast();
	return true;
}

bool ATwinSoulTrial::SpawnTargets()
{
	if (!CrimsonEnemyClass || !VioletEnemyClass || !GetWorld()) return false;
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	CrimsonTarget = GetWorld()->SpawnActor<AEnemyBase>(CrimsonEnemyClass, GetActorLocation() + TrialArenaOffset + CrimsonSpawnOffset, FRotator::ZeroRotator, Params);
	VioletTarget = GetWorld()->SpawnActor<AEnemyBase>(VioletEnemyClass, GetActorLocation() + TrialArenaOffset + VioletSpawnOffset, FRotator::ZeroRotator, Params);
	if (!CrimsonTarget || !VioletTarget) return false;

	CrimsonTarget->ConfigureObjectiveEnemy(CrimsonMaxHealth, EPlayerAttackSource::Samurai, CrimsonOverlayMaterial, FLinearColor(0.6f, 0.01f, 0.015f));
	VioletTarget->ConfigureObjectiveEnemy(VioletMaxHealth, EPlayerAttackSource::Ninja, VioletOverlayMaterial, FLinearColor(0.28f, 0.01f, 0.55f));
	CrimsonTarget->OnEnemyDied.AddUniqueDynamic(this, &ATwinSoulTrial::HandleCrimsonDied);
	VioletTarget->OnEnemyDied.AddUniqueDynamic(this, &ATwinSoulTrial::HandleVioletDied);
	OnCrimsonSpawned.Broadcast(CrimsonTarget);
	OnVioletSpawned.Broadcast(VioletTarget);
	return true;
}

void ATwinSoulTrial::HandleCrimsonDied(AEnemyBase* Enemy)
{
	if (TrialState != ETwinSoulTrialState::TrialActive || bCrimsonDead) return;
	bCrimsonDead = true;
	OnCrimsonDefeated.Broadcast();
	if (bVioletDead) CompleteTrial();
}

void ATwinSoulTrial::HandleVioletDied(AEnemyBase* Enemy)
{
	if (TrialState != ETwinSoulTrialState::TrialActive || bVioletDead) return;
	bVioletDead = true;
	OnVioletDefeated.Broadcast();
	if (bCrimsonDead) CompleteTrial();
}

void ATwinSoulTrial::CompleteTrial()
{
	if (TrialState != ETwinSoulTrialState::TrialActive || !PlayerController || PlayerController->IsPlayerDead()) return;
	TrialState = ETwinSoulTrialState::AwaitingReward;
	OnTrialCompleted.Broadcast();
	PlayerController->RequestTwinSoulCompletionRewards(RewardChoiceCount, RewardChoiceCount);
}

void ATwinSoulTrial::HandleRewardCompleted()
{
	if (TrialState == ETwinSoulTrialState::AwaitingReward) ReturnPlayerToArena();
}

void ATwinSoulTrial::ReturnPlayerToArena()
{
	UCharacterManagerComponent* Manager = PlayerController ? PlayerController->GetCharacterManager() : nullptr;
	if (ACharacterBase* ActiveCharacter = Manager ? Manager->GetActiveCharacter() : nullptr)
	{
		ActiveCharacter->SetActorTransform(ReturnTransform, false, nullptr, ETeleportType::TeleportPhysics);
	}
	if (MainSpawner) MainSpawner->SetTrialSuspended(false);
	TrialState = ETwinSoulTrialState::Completed;
	if (PlayerController) PlayerController->EndObjective(this);
	OnPlayerReturned.Broadcast();
}

void ATwinSoulTrial::HandlePlayerDeath()
{
	if (TrialState == ETwinSoulTrialState::TrialActive || TrialState == ETwinSoulTrialState::AwaitingReward)
	{
		TrialState = ETwinSoulTrialState::Failed;
		CleanupTargetBindings();
		if (MainSpawner) MainSpawner->SetTrialSuspended(false);
		if (PlayerController) PlayerController->EndObjective(this);
	}
}

void ATwinSoulTrial::FindReferences()
{
	PlayerController = Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	if (!MainSpawner && GetWorld())
	{
		for (TActorIterator<AEnemySpawner> It(GetWorld()); It; ++It) { MainSpawner = *It; break; }
	}
}

void ATwinSoulTrial::CreateInteractionPrompt()
{
	if (!InteractionPromptComponent) return;
	InteractionPromptComponent->SetRelativeLocation(FVector(0.0f, 0.0f, PromptVerticalOffset));
	InteractionPromptComponent->SetDrawSize(FVector2D(FMath::Max(1.0f, PromptDrawSize.X), FMath::Max(1.0f, PromptDrawSize.Y)));
	InteractionPromptComponent->SetRelativeScale3D(FVector(FMath::Max(0.01f, PromptWorldScale)));
	InteractionPromptComponent->SetWidgetClass(UBloodShrineWidget::StaticClass());
	InteractionPromptComponent->InitWidget();
	if (UBloodShrineWidget* Prompt = Cast<UBloodShrineWidget>(InteractionPromptComponent->GetUserWidgetObject()))
	{
		Prompt->ConfigureForWorldSpace();
		Prompt->ShowInteractionPrompt(FText::FromString(TEXT("TWIN SOUL TRIAL")));
	}
	InteractionPromptComponent->SetVisibility(false);
}

void ATwinSoulTrial::UpdateInactivePrompt()
{
	if (!InteractionPromptComponent || !PlayerController)
	{
		return;
	}

	APawn* Pawn = PlayerController->GetPawn();
	InteractionPromptComponent->SetVisibility(CanInteract_Implementation(Pawn));
}

void ATwinSoulTrial::FaceInteractionPromptToCamera()
{
	if (!InteractionPromptComponent || !InteractionPromptComponent->IsVisible() || !PlayerController) return;
	if (APlayerCameraManager* Camera = PlayerController->PlayerCameraManager)
	{
		InteractionPromptComponent->SetWorldRotation((Camera->GetCameraLocation() - InteractionPromptComponent->GetComponentLocation()).Rotation());
	}
}

void ATwinSoulTrial::CleanupTargetBindings()
{
	if (CrimsonTarget) CrimsonTarget->OnEnemyDied.RemoveDynamic(this, &ATwinSoulTrial::HandleCrimsonDied);
	if (VioletTarget) VioletTarget->OnEnemyDied.RemoveDynamic(this, &ATwinSoulTrial::HandleVioletDied);
}
