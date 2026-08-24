// Copyright Epic Games, Inc. All Rights Reserved.

#include "TwinSoulTrial.h"

#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "EnemyBase.h"
#include "EnemySpawner.h"
#include "EngineUtils.h"
#include "HealthComponent.h"
#include "Kismet/GameplayStatics.h"
#include "SurvivorPlayerController.h"
#include "UObject/ConstructorHelpers.h"

ATwinSoulTrial::ATwinSoulTrial()
{
	PrimaryActorTick.bCanEverTick = false;
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

	InteractionPrompt = CreateDefaultSubobject<UTextRenderComponent>(TEXT("InteractionPrompt"));
	InteractionPrompt->SetupAttachment(SceneRoot);
	InteractionPrompt->SetRelativeLocation(FVector(0.0f, 0.0f, 260.0f));
	InteractionPrompt->SetText(FText::FromString(TEXT("Enter Twin Soul Trial")));
	InteractionPrompt->SetHorizontalAlignment(EHTA_Center);
	InteractionPrompt->SetWorldSize(42.0f);

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
}

void ATwinSoulTrial::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CleanupTargetBindings();
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

	ReturnTransform = ActiveCharacter->GetActorTransform();
	ReturnTransform.AddToTranslation(ReturnOffset);
	TrialState = ETwinSoulTrialState::TrialActive;
	bCrimsonDead = false;
	bVioletDead = false;
	MainSpawner->SetTrialSuspended(true);
	ActiveCharacter->SetActorLocation(GetActorLocation() + TrialArenaOffset + TrialPlayerOffset, false, nullptr, ETeleportType::TeleportPhysics);
	if (!SpawnTargets())
	{
		TrialState = ETwinSoulTrialState::Inactive;
		ActiveCharacter->SetActorTransform(ReturnTransform, false, nullptr, ETeleportType::TeleportPhysics);
		MainSpawner->SetTrialSuspended(false);
		return false;
	}

	if (UHealthComponent* Health = PlayerController->GetPlayerHealthComponent()) Health->OnDeath.AddUniqueDynamic(this, &ATwinSoulTrial::HandlePlayerDeath);
	PlayerController->OnTwinSoulRewardCompleted.AddUniqueDynamic(this, &ATwinSoulTrial::HandleRewardCompleted);
	InteractionPrompt->SetVisibility(false);
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
	PlayerController->RequestTwinSoulSynergyReward(RewardChoiceCount);
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
	OnPlayerReturned.Broadcast();
}

void ATwinSoulTrial::HandlePlayerDeath()
{
	if (TrialState == ETwinSoulTrialState::TrialActive || TrialState == ETwinSoulTrialState::AwaitingReward)
	{
		TrialState = ETwinSoulTrialState::Failed;
		CleanupTargetBindings();
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

void ATwinSoulTrial::CleanupTargetBindings()
{
	if (CrimsonTarget) CrimsonTarget->OnEnemyDied.RemoveDynamic(this, &ATwinSoulTrial::HandleCrimsonDied);
	if (VioletTarget) VioletTarget->OnEnemyDied.RemoveDynamic(this, &ATwinSoulTrial::HandleVioletDied);
}
