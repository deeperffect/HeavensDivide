#include "NinjaTechniqueTrial.h"

#include "AutoAttackComponent.h"
#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnemySpawner.h"
#include "EngineUtils.h"
#include "HealthComponent.h"
#include "NinjaFloorTrap.h"
#include "NinjaCharacter.h"
#include "NinjaSweepingTrap.h"
#include "NinjaTimedGate.h"
#include "NinjaTrialGoal.h"
#include "NinjaTrialTrapBase.h"
#include "ObjectiveInteractionComponent.h"
#include "SamuraiCharacter.h"
#include "SurvivorPlayerController.h"
#include "UObject/ConstructorHelpers.h"

ANinjaTechniqueTrial::ANinjaTechniqueTrial()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	NinjaStatue = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NinjaStatue"));
	NinjaStatue->SetupAttachment(SceneRoot);
	NinjaStatue->SetRelativeScale3D(FVector(1.5, 1.5, 3.0));
	if (CubeMesh.Succeeded()) NinjaStatue->SetStaticMesh(CubeMesh.Object);

	ObjectiveInteraction = CreateDefaultSubobject<UObjectiveInteractionComponent>(TEXT("ObjectiveInteraction"));
	ObjectiveInteraction->SetupAttachment(SceneRoot);
	ObjectiveInteraction->ConfigureDefaults(FText::FromString(TEXT("Ninja Trial")), 300.0f, 1.0f, 240.0f);
	ObjectiveInteraction->SetPresentationVisual(NinjaStatue);

	ArenaRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ArenaRoot"));
	ArenaRoot->SetupAttachment(SceneRoot);

	TrialFloor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TrialFloor"));
	TrialFloor->SetupAttachment(ArenaRoot);
	TrialFloor->SetRelativeScale3D(FVector(15.0, 8.0, 0.5));
	TrialFloor->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	TrialFloor->SetCollisionResponseToAllChannels(ECR_Block);
	if (CubeMesh.Succeeded()) TrialFloor->SetStaticMesh(CubeMesh.Object);

	TrialPlayerStart = CreateDefaultSubobject<USceneComponent>(TEXT("TrialPlayerStart"));
	TrialPlayerStart->SetupAttachment(ArenaRoot);
	TrialPlayerStart->SetRelativeLocation(FVector(-1200.0, 0.0, 100.0));

	FloorTrap01 = CreateDefaultSubobject<UChildActorComponent>(TEXT("FloorTrap01"));
	FloorTrap01->SetupAttachment(ArenaRoot);
	FloorTrap01->SetChildActorClass(ANinjaFloorTrap::StaticClass());

	SweepingTrap01 = CreateDefaultSubobject<UChildActorComponent>(TEXT("SweepingTrap01"));
	SweepingTrap01->SetupAttachment(ArenaRoot);
	SweepingTrap01->SetRelativeLocation(FVector(0.0, 0.0, 100.0));
	SweepingTrap01->SetChildActorClass(ANinjaSweepingTrap::StaticClass());

	TimedGate01 = CreateDefaultSubobject<UChildActorComponent>(TEXT("TimedGate01"));
	TimedGate01->SetupAttachment(ArenaRoot);
	TimedGate01->SetRelativeLocation(FVector(800.0, 0.0, 100.0));
	TimedGate01->SetChildActorClass(ANinjaTimedGate::StaticClass());

	Goal = CreateDefaultSubobject<UChildActorComponent>(TEXT("Goal"));
	Goal->SetupAttachment(ArenaRoot);
	Goal->SetRelativeLocation(FVector(1300.0, 0.0, 100.0));
	Goal->SetChildActorClass(ANinjaTrialGoal::StaticClass());
}

void ANinjaTechniqueTrial::BeginPlay()
{
	Super::BeginPlay();
	if (ObjectiveInteraction)
	{
		// The entrance visual is Blueprint-positioned independently of the actor origin.
		// Keep both the range check and prompt projection anchored to the visible statue.
		ObjectiveInteraction->PresentationAnchor = NinjaStatue;
		ObjectiveInteraction->SetPresentationVisual(NinjaStatue);
		ObjectiveInteraction->RefreshPrompt();
	}
	FindRuntimeReferences();
}

void ANinjaTechniqueTrial::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RestoreGameplayState(TrialState != ENinjaTrialState::Failed);
	Super::EndPlay(EndPlayReason);
}

bool ANinjaTechniqueTrial::CanInteract_Implementation(APawn* InteractingPawn) const
{
	return TrialState == ENinjaTrialState::Inactive
		&& IsValid(InteractingPawn)
		&& IsValid(ObjectiveInteraction)
		&& ObjectiveInteraction->IsInRange(InteractingPawn)
		&& (!PlayerController || !PlayerController->IsAnyObjectiveActive());
}

void ANinjaTechniqueTrial::Interact_Implementation(APawn* InteractingPawn)
{
	EnterTrial(InteractingPawn);
}

bool ANinjaTechniqueTrial::EnterTrial(APawn* InteractingPawn)
{
	if (!CanInteract_Implementation(InteractingPawn)) return false;
	ReturnTransform = InteractingPawn->GetActorTransform();
	bHasReturnTransform = true;

	FindRuntimeReferences();
	if (!PlayerController || !MainSpawner || PlayerController->IsPlayerDead() || !TrialPlayerStart)
	{
		UE_LOG(LogTemp, Error, TEXT("[NinjaTrialEntry] Missing entry dependency Trial=%s Controller=%s Spawner=%s PlayerStart=%s"),
			*GetName(), *GetNameSafe(PlayerController), *GetNameSafe(MainSpawner), *GetNameSafe(TrialPlayerStart));
		return false;
	}

	if (!PlayerController->TryBeginObjective(this)) return false;

	if (!PlayerController->ForceNinjaActive())
	{
		PlayerController->EndObjective(this);
		return false;
	}

	UCharacterManagerComponent* CharacterManager = PlayerController->GetCharacterManager();
	ACharacterBase* ActiveNinja = CharacterManager ? CharacterManager->GetActiveCharacter() : nullptr;
	if (!ActiveNinja)
	{
		PlayerController->EndObjective(this);
		return false;
	}

	UAutoAttackComponent* SamuraiAutoAttack = CharacterManager->GetSamurai()
		? CharacterManager->GetSamurai()->FindComponentByClass<UAutoAttackComponent>() : nullptr;
	UAutoAttackComponent* NinjaAutoAttack = CharacterManager->GetNinja()
		? CharacterManager->GetNinja()->FindComponentByClass<UAutoAttackComponent>() : nullptr;
	bSamuraiAutoAttackWasEnabled = SamuraiAutoAttack && SamuraiAutoAttack->IsAutoAttackEnabled();
	bNinjaAutoAttackWasEnabled = NinjaAutoAttack && NinjaAutoAttack->IsAutoAttackEnabled();
	if (SamuraiAutoAttack) SamuraiAutoAttack->SetAutoAttackEnabled(false);
	if (NinjaAutoAttack) NinjaAutoAttack->SetAutoAttackEnabled(false);

	PlayerController->SetSwapLocked(true);
	MainSpawner->SetTrialSuspended(true);

	const FTransform PlayerStartTransform = TrialPlayerStart->GetComponentTransform();
	const FVector TeleportDestination = PlayerStartTransform.GetLocation();
	UE_LOG(LogTemp, Log, TEXT("[NinjaTrialEntry] TrialPlayerStart=%s TeleportDestination=%s"),
		*TrialPlayerStart->GetComponentLocation().ToCompactString(), *TeleportDestination.ToCompactString());

	ActiveNinja->SetActorTransform(PlayerStartTransform, false, nullptr, ETeleportType::TeleportPhysics);
	TrialState = ENinjaTrialState::Running;
	ObjectiveInteraction->HidePrompt();
	DiscoverAndInitializeTraps();
	ActivateRegisteredTraps();
	if (UHealthComponent* Health = PlayerController->GetPlayerHealthComponent())
	{
		Health->OnDeath.AddUniqueDynamic(this, &ANinjaTechniqueTrial::HandlePlayerDeath);
	}
	PlayerController->OnNinjaTrialRewardCompleted.AddUniqueDynamic(this, &ANinjaTechniqueTrial::HandleRewardCompleted);

	UE_LOG(LogTemp, Log, TEXT("[NinjaTrialEntry] PlayerAfterTeleport=%s"),
		*ActiveNinja->GetActorLocation().ToCompactString());
	return true;
}

void ANinjaTechniqueTrial::DiscoverAndInitializeTraps()
{
	RegisteredTraps.Reset();
	RegisteredGoal = nullptr;
	if (!ArenaRoot) return;

	TArray<USceneComponent*> ArenaComponents;
	ArenaRoot->GetChildrenComponents(true, ArenaComponents);
	for (USceneComponent* ArenaComponent : ArenaComponents)
	{
		UChildActorComponent* ChildActorComponent = Cast<UChildActorComponent>(ArenaComponent);
		AActor* ChildActor = ChildActorComponent ? ChildActorComponent->GetChildActor() : nullptr;
		if (ChildActorComponent)
		{
			UE_LOG(LogTemp, Log, TEXT("[NinjaTrialTraps] Found trap component: %s Child actor: %s Trap class: %s"), *ChildActorComponent->GetName(), *GetNameSafe(ChildActor), ChildActor ? *GetNameSafe(ChildActor->GetClass()) : TEXT("None"));
		}
		if (ANinjaTrialGoal* CourseGoal = Cast<ANinjaTrialGoal>(ChildActor))
		{
			CourseGoal->InitializeForTrial(this);
			RegisteredGoal = CourseGoal;
			continue;
		}

		ANinjaTrialTrapBase* Trap = Cast<ANinjaTrialTrapBase>(ChildActor);
		if (!Trap) continue;

		Trap->InitializeForTrial(this);
		RegisteredTraps.AddUnique(Trap);
	}

	UE_LOG(LogTemp, Log, TEXT("[NinjaTrialTraps] Registered=%d Goal=%s Trial=%s"),
		RegisteredTraps.Num(), *GetNameSafe(RegisteredGoal), *GetName());
}

void ANinjaTechniqueTrial::NotifyGoalReached(AActor* ReachingActor)
{
	if (TrialState != ENinjaTrialState::Running || !PlayerController) return;

	UCharacterManagerComponent* CharacterManager = PlayerController->GetCharacterManager();
	if (!CharacterManager || ReachingActor != CharacterManager->GetActiveCharacter()) return;

	TrialState = ENinjaTrialState::AwaitingReward;
	DeactivateRegisteredTraps();
	UE_LOG(LogTemp, Log, TEXT("Ninja Trial Course Complete"));
	PlayerController->RequestNinjaTrialUpgradeReward(RewardChoiceCount);
}

void ANinjaTechniqueTrial::HandleRewardCompleted()
{
	if (TrialState != ENinjaTrialState::AwaitingReward) return;
	TrialState = ENinjaTrialState::Returning;
	RestoreGameplayState(true);
	TrialState = ENinjaTrialState::Completed;
}

void ANinjaTechniqueTrial::HandlePlayerDeath()
{
	if (TrialState != ENinjaTrialState::Running && TrialState != ENinjaTrialState::AwaitingReward) return;
	TrialState = ENinjaTrialState::Failed;
	RestoreGameplayState(false);
}

void ANinjaTechniqueTrial::ActivateRegisteredTraps()
{
	for (ANinjaTrialTrapBase* Trap : RegisteredTraps)
	{
		if (IsValid(Trap))
		{
			UE_LOG(LogTemp, Log, TEXT("[NinjaTrialTraps] Activating trap %s"), *Trap->GetName());
			Trap->ActivateTrap();
		}
	}
}

bool ANinjaTechniqueTrial::IsActivePlayerCharacter(const AActor* Candidate) const
{
	const UCharacterManagerComponent* CharacterManager = PlayerController ? PlayerController->GetCharacterManager() : nullptr;
	return Candidate && CharacterManager && Candidate == CharacterManager->GetActiveCharacter();
}

bool ANinjaTechniqueTrial::ApplyTrialHazardDamage(float DamageAmount)
{
	if (!IsTrialRunning() || !PlayerController || PlayerController->IsPlayerDead() || DamageAmount <= 0.0f) return false;
	PlayerController->ApplyDamageToPlayer(DamageAmount);
	return true;
}

void ANinjaTechniqueTrial::DeactivateRegisteredTraps()
{
	for (ANinjaTrialTrapBase* Trap : RegisteredTraps)
	{
		if (IsValid(Trap)) Trap->DeactivateTrap();
	}
	RegisteredTraps.Reset();
}

void ANinjaTechniqueTrial::FindRuntimeReferences()
{
	if (!PlayerController && GetWorld())
	{
		PlayerController = Cast<ASurvivorPlayerController>(GetWorld()->GetFirstPlayerController());
	}
	if (!MainSpawner && GetWorld())
	{
		for (TActorIterator<AEnemySpawner> It(GetWorld()); It; ++It)
		{
			MainSpawner = *It;
			break;
		}
	}
}

void ANinjaTechniqueTrial::RestoreGameplayState(bool bReturnPlayer)
{
	if (TrialState == ENinjaTrialState::Inactive) return;
	DeactivateRegisteredTraps();

	if (PlayerController)
	{
		PlayerController->OnNinjaTrialRewardCompleted.RemoveDynamic(this, &ANinjaTechniqueTrial::HandleRewardCompleted);
		if (UHealthComponent* Health = PlayerController->GetPlayerHealthComponent())
		{
			Health->OnDeath.RemoveDynamic(this, &ANinjaTechniqueTrial::HandlePlayerDeath);
		}
		if (UCharacterManagerComponent* CharacterManager = PlayerController->GetCharacterManager())
		{
			if (bReturnPlayer && bHasReturnTransform)
			{
				if (ACharacterBase* ActiveCharacter = CharacterManager->GetActiveCharacter())
				{
					ActiveCharacter->SetActorTransform(ReturnTransform, false, nullptr, ETeleportType::TeleportPhysics);
					UE_LOG(LogTemp, Log, TEXT("[NinjaTrialReturn] Saved=%s Actual=%s"), *ReturnTransform.GetLocation().ToCompactString(), *ActiveCharacter->GetActorLocation().ToCompactString());
				}
			}
			if (UAutoAttackComponent* SamuraiAutoAttack = CharacterManager->GetSamurai()
				? CharacterManager->GetSamurai()->FindComponentByClass<UAutoAttackComponent>() : nullptr)
			{
				SamuraiAutoAttack->SetAutoAttackEnabled(bSamuraiAutoAttackWasEnabled && !PlayerController->IsPlayerDead());
			}
			if (UAutoAttackComponent* NinjaAutoAttack = CharacterManager->GetNinja()
				? CharacterManager->GetNinja()->FindComponentByClass<UAutoAttackComponent>() : nullptr)
			{
				NinjaAutoAttack->SetAutoAttackEnabled(bNinjaAutoAttackWasEnabled && !PlayerController->IsPlayerDead());
			}
		}
		PlayerController->SetSwapLocked(false);
		PlayerController->EndObjective(this);
	}
	if (MainSpawner) MainSpawner->SetTrialSuspended(false);
	RegisteredGoal = nullptr;
	bHasReturnTransform = false;
}
