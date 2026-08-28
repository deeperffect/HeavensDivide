// Copyright Epic Games, Inc. All Rights Reserved.

#include "TwinSoulTrial.h"

#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnemyBase.h"
#include "EnemySpawner.h"
#include "EngineUtils.h"
#include "HealthComponent.h"
#include "Kismet/GameplayStatics.h"
#include "SurvivorPlayerController.h"
#include "MinimapMarkerComponent.h"
#include "ObjectiveInteractionComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "TrialArenaAnchor.h"
#include "TrialArenaSubsystem.h"

ATwinSoulTrial::ATwinSoulTrial()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	MinimapMarker = CreateDefaultSubobject<UMinimapMarkerComponent>(TEXT("MinimapMarker"));
	MinimapMarker->DisplayName = FText::FromString(TEXT("Twin Soul Trial"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	PortalMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PortalMesh"));
	PortalMesh->SetupAttachment(SceneRoot);
	PortalMesh->SetRelativeScale3D(FVector(1.5f, 1.5f, 3.0f));
	if (CubeMesh.Succeeded()) PortalMesh->SetStaticMesh(CubeMesh.Object);

	ObjectiveInteraction = CreateDefaultSubobject<UObjectiveInteractionComponent>(TEXT("ObjectiveInteraction"));
	ObjectiveInteraction->SetupAttachment(SceneRoot);
	ObjectiveInteraction->ConfigureDefaults(FText::FromString(TEXT("Twin Soul Trial")),300.0f,1.0f,240.0f);
	ObjectiveInteraction->SetPresentationVisual(PortalMesh);

	TrialFloor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TrialFloor"));
	TrialFloor->SetupAttachment(SceneRoot);
	TrialFloor->SetRelativeLocation(TrialArenaOffset);
	TrialFloor->SetRelativeScale3D(FVector(30.0f, 30.0f, 0.5f));
	TrialFloor->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	TrialFloor->SetCollisionResponseToAllChannels(ECR_Block);
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
		Wall->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Wall->SetCollisionResponseToAllChannels(ECR_Block);
	}
}

void ATwinSoulTrial::BeginPlay()
{
	Super::BeginPlay();
	FindReferences();
	InitializeTrialArena();
}

void ATwinSoulTrial::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CleanupTargetBindings();
	if(IsValid(CrimsonTarget))CrimsonTarget->Destroy();
	if(IsValid(VioletTarget))VioletTarget->Destroy();
	if(GetWorld())GetWorld()->GetSubsystem<UTrialArenaSubsystem>()->ReleaseAnchor(this);
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
		&& ObjectiveInteraction && ObjectiveInteraction->IsInRange(InteractingPawn);
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
	FVector TrialLocation;
	if(!IsTrialArenaValid(ActiveCharacter,TrialLocation)){UE_LOG(LogTemp,Error,TEXT("[TrialRuntime] ERROR Trial arena/start invalid; refusing teleport Trial=%s Anchor=%s Floor=%s"),*GetName(),*GetNameSafe(ArenaAnchor),*GetNameSafe(TrialFloor));return false;}
	if (!PlayerController->TryBeginObjective(this)) return false;

	ReturnTransform = ActiveCharacter->GetActorTransform();
	ReturnTransform.AddToTranslation(ReturnOffset);
	TrialState = ETwinSoulTrialState::TrialActive;
	MinimapMarker->SetMarkerState(EMinimapMarkerState::Active);
	bCrimsonDead = false;
	bVioletDead = false;
	MainSpawner->SetTrialSuspended(true);
	UE_LOG(LogTemp,Log,TEXT("[TrialRuntime] Enter Trial=%s Return=%s Arrival=%s FloorTop=%.1f"),*GetName(),*ReturnTransform.GetLocation().ToCompactString(),*TrialLocation.ToCompactString(),TrialFloor->Bounds.Origin.Z+TrialFloor->Bounds.BoxExtent.Z);
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
	if (ObjectiveInteraction) ObjectiveInteraction->HidePrompt();
	OnTrialEntered.Broadcast();
	return true;
}

bool ATwinSoulTrial::SpawnTargets()
{
	if (!CrimsonEnemyClass || !VioletEnemyClass || !GetWorld()) return false;
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	const FTransform ArenaTransform(TrialFloor ? TrialFloor->GetComponentQuat() : FQuat::Identity, ArenaOrigin);
	CrimsonTarget = GetWorld()->SpawnActor<AEnemyBase>(CrimsonEnemyClass, ArenaTransform.TransformPositionNoScale(CrimsonSpawnOffset), ArenaTransform.Rotator(), Params);
	VioletTarget = GetWorld()->SpawnActor<AEnemyBase>(VioletEnemyClass, ArenaTransform.TransformPositionNoScale(VioletSpawnOffset), ArenaTransform.Rotator(), Params);
	if (!CrimsonTarget || !VioletTarget) return false;

	CrimsonTarget->ConfigureObjectiveEnemy(CrimsonMaxHealth, EPlayerAttackSource::Samurai, CrimsonOverlayMaterial, FLinearColor(0.6f, 0.01f, 0.015f));
	VioletTarget->ConfigureObjectiveEnemy(VioletMaxHealth, EPlayerAttackSource::Ninja, VioletOverlayMaterial, FLinearColor(0.28f, 0.01f, 0.55f));
	CrimsonTarget->OnEnemyDied.AddUniqueDynamic(this, &ATwinSoulTrial::HandleCrimsonDied);
	VioletTarget->OnEnemyDied.AddUniqueDynamic(this, &ATwinSoulTrial::HandleVioletDied);
	OnCrimsonSpawned.Broadcast(CrimsonTarget);
	OnVioletSpawned.Broadcast(VioletTarget);
	return true;
}

bool ATwinSoulTrial::InitializeTrialArena()
{
	ArenaAnchor=GetWorld()->GetSubsystem<UTrialArenaSubsystem>()->ReserveAnchor(this);
	const FVector LegacyOrigin=TrialFloor?TrialFloor->GetComponentLocation():GetActorTransform().TransformPosition(TrialArenaOffset);
	ArenaOrigin=ArenaAnchor?ArenaAnchor->GetActorLocation():LegacyOrigin;
	if(ArenaAnchor){const FVector Delta=ArenaOrigin-LegacyOrigin;if(TrialFloor)TrialFloor->AddWorldOffset(Delta);for(UStaticMeshComponent* Wall:TrialWalls)if(Wall)Wall->AddWorldOffset(Delta);}
	bArenaReady=TrialFloor&&TrialFloor->IsRegistered()&&TrialFloor->GetCollisionEnabled()!=ECollisionEnabled::NoCollision;
	UE_LOG(LogTemp,Log,TEXT("[TrialRuntime] Setup Trial=%s Entrance=%s Anchor=%s ArenaOrigin=%s Floor=%s Ready=%s"),*GetName(),*GetActorLocation().ToCompactString(),*GetNameSafe(ArenaAnchor),*ArenaOrigin.ToCompactString(),*GetNameSafe(TrialFloor),bArenaReady?TEXT("true"):TEXT("false"));
	return bArenaReady;
}

bool ATwinSoulTrial::IsTrialArenaValid(const ACharacterBase* Character,FVector& OutArrivalLocation) const
{
	const FTransform ArenaTransform(TrialFloor?TrialFloor->GetComponentQuat():FQuat::Identity,ArenaOrigin);
	OutArrivalLocation=ArenaTransform.TransformPositionNoScale(TrialPlayerOffset);
	if(!bArenaReady||!TrialFloor||!TrialFloor->IsRegistered()||TrialFloor->GetCollisionEnabled()==ECollisionEnabled::NoCollision)return false;
	const FBox FloorBox=TrialFloor->Bounds.GetBox();
	const UCapsuleComponent* Capsule=Character?Character->GetCapsuleComponent():nullptr;
	OutArrivalLocation.Z=FloorBox.Max.Z+(Capsule?Capsule->GetScaledCapsuleHalfHeight():0.0f)+2.0f;
	return FloorBox.IsInsideXY(FVector(OutArrivalLocation.X,OutArrivalLocation.Y,FloorBox.GetCenter().Z));
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
	MinimapMarker->SetMarkerState(EMinimapMarkerState::Completed);
	if (PlayerController) PlayerController->EndObjective(this);
	OnPlayerReturned.Broadcast();
}

void ATwinSoulTrial::HandlePlayerDeath()
{
	if (TrialState == ETwinSoulTrialState::TrialActive || TrialState == ETwinSoulTrialState::AwaitingReward)
	{
		TrialState = ETwinSoulTrialState::Failed;
		MinimapMarker->SetMarkerState(EMinimapMarkerState::Failed);
		CleanupTargetBindings();
		if(IsValid(CrimsonTarget))CrimsonTarget->Destroy();
		if(IsValid(VioletTarget))VioletTarget->Destroy();
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

void ATwinSoulTrial::CleanupTargetBindings()
{
	if (CrimsonTarget) CrimsonTarget->OnEnemyDied.RemoveDynamic(this, &ATwinSoulTrial::HandleCrimsonDied);
	if (VioletTarget) VioletTarget->OnEnemyDied.RemoveDynamic(this, &ATwinSoulTrial::HandleVioletDied);
}
