// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemyBase.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AnimationBudgetAllocatorParameters.h"
#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "EnemyHealthBarWidget.h"
#include "EnemyLightweightMovementComponent.h"
#include "EnemyMarkIndicatorWidget.h"
#include "ExperiencePickup.h"
#include "ExperienceComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HealthComponent.h"
#include "HAL/IConsoleManager.h"
#include "IAnimationBudgetAllocator.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "SkeletalMeshComponentBudgeted.h"
#include "SurvivorPlayerController.h"
#include "TimerManager.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "UObject/UnrealType.h"

static TAutoConsoleVariable<int32> CVarDisableEnemyCharacterMovementForProfiling(
	TEXT("hd.DisableEnemyCharacterMovementForProfiling"),
	0,
	TEXT("When set to 1, disables CharacterMovement ticking/work for EnemyBase-derived enemies for profiling."));

static TAutoConsoleVariable<int32> CVarDisableEnemyAnimationForProfiling(
	TEXT("hd.DisableEnemyAnimationForProfiling"),
	0,
	TEXT("When set to 1, disables skeletal animation ticking/evaluation for EnemyBase-derived enemies for profiling."));

static TAutoConsoleVariable<int32> CVarLogEnemyAnimationBudgetSetup(
	TEXT("hd.LogEnemyAnimationBudgetSetup"),
	0,
	TEXT("When set to 1, logs one-time Animation Budget Allocator setup details for spawned EnemyBase-derived enemies."));

static TAutoConsoleVariable<int32> CVarDebugEnemySeparation(
	TEXT("hd.DebugEnemySeparation"),
	0,
	TEXT("When set to 1, draws lightweight enemy separation debug for a small sampled subset of enemies."));

static void DumpNearestEnemy(const TArray<FString>& Args, UWorld* World)
{
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EnemyDamage] hd.DumpNearestEnemy failed: world is invalid."));
		return;
	}

	const ASurvivorPlayerController* SurvivorController = Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(World, 0));
	const UCharacterManagerComponent* CharacterManager = SurvivorController ? SurvivorController->GetCharacterManager() : nullptr;
	const ACharacterBase* ActiveCharacter = CharacterManager ? CharacterManager->GetActiveCharacter() : nullptr;
	if (!ActiveCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EnemyDamage] hd.DumpNearestEnemy failed: active player character is invalid."));
		return;
	}

	AEnemyBase* NearestEnemy = nullptr;
	float NearestDistanceSquared = TNumericLimits<float>::Max();
	for (TActorIterator<AEnemyBase> EnemyIt(World); EnemyIt; ++EnemyIt)
	{
		AEnemyBase* Enemy = *EnemyIt;
		if (!Enemy)
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared2D(Enemy->GetActorLocation(), ActiveCharacter->GetActorLocation());
		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			NearestEnemy = Enemy;
		}
	}

	if (!NearestEnemy)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EnemyDamage] hd.DumpNearestEnemy found no EnemyBase actors."));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[EnemyDamage] Nearest enemy to %s is %s at %.1f units."),
		*GetNameSafe(ActiveCharacter),
		*GetNameSafe(NearestEnemy),
		FMath::Sqrt(NearestDistanceSquared));
	NearestEnemy->LogEnemyDebugState(TEXT("hd.DumpNearestEnemy"));
}

static FAutoConsoleCommandWithWorldAndArgs GDumpNearestEnemyCommand(
	TEXT("hd.DumpNearestEnemy"),
	TEXT("Logs health, death, movement, and collision state for the nearest EnemyBase to the active player."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DumpNearestEnemy));

AEnemyBase::AEnemyBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<USkeletalMeshComponentBudgeted>(ACharacter::MeshComponentName))
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
	LightweightMovementComponent = CreateDefaultSubobject<UEnemyLightweightMovementComponent>(TEXT("LightweightMovementComponent"));

	HealthBarWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBarWidgetComponent"));
	HealthBarWidgetComponent->SetupAttachment(RootComponent);
	HealthBarWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	HealthBarWidgetComponent->SetDrawSize(HealthBarDrawSize);
	HealthBarWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HealthBarWidgetComponent->SetHiddenInGame(true);
	HealthBarWidgetComponent->SetVisibility(false);
	HealthBarWidgetComponent->SetComponentTickEnabled(false);

	MarkIndicatorWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("MarkIndicatorWidgetComponent"));
	MarkIndicatorWidgetComponent->SetupAttachment(RootComponent);
	MarkIndicatorWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	MarkIndicatorWidgetComponent->SetDrawSize(MarkIndicatorDrawSize);
	MarkIndicatorWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MarkIndicatorWidgetComponent->SetHiddenInGame(true);
	MarkIndicatorWidgetComponent->SetVisibility(false);
	MarkIndicatorWidgetComponent->SetComponentTickEnabled(false);

	ConfigureEnemyCapsuleCollisionDefaults();
	ConfigureEnemyMovementDefaults();
}

void AEnemyBase::BeginPlay()
{
	Super::BeginPlay();

	InitializeEnemyMovementMode();
	InitializeCrowdSpread();

	if (HealthComponent)
	{
		HealthComponent->OnDeath.AddDynamic(this, &AEnemyBase::HandleDeath);
		HealthComponent->OnHealthChanged.AddDynamic(this, &AEnemyBase::HandleHealthChanged);
	}

	InitializeHealthBar();
	InitializeMarkIndicator();
	InitializeTargetFromCharacterManager();
	CachePlayerExperienceComponent();
	InitializeAnimationBudgeting();
	UpdateCharacterMovementProfilingState();
	UpdateAnimationProfilingState();
	StartBehaviorUpdates();
	StartSeparationUpdates();
}

void AEnemyBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ObservedCharacterManager)
	{
		ObservedCharacterManager->OnCharacterSwapped.RemoveDynamic(this, &AEnemyBase::HandlePlayerCharacterSwapped);
	}

	if (HealthComponent)
	{
		HealthComponent->OnHealthChanged.RemoveDynamic(this, &AEnemyBase::HandleHealthChanged);
		HealthComponent->OnDeath.RemoveDynamic(this, &AEnemyBase::HandleDeath);
	}

	StopEnemyBehavior();
	StopBehaviorUpdates();
	StopSeparationUpdates();

	Super::EndPlay(EndPlayReason);
}

void AEnemyBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateCharacterMovementProfilingState();
	UpdateAnimationProfilingState();
	ApplyDesiredMovementInput(DeltaSeconds);
}

FVector AEnemyBase::GetVelocity() const
{
	if (IsUsingLightweightMovement())
	{
		return GetEnemyMovementVelocity();
	}

	return Super::GetVelocity();
}

void AEnemyBase::SetTarget(AActor* NewTarget)
{
	CurrentTarget = NewTarget;
}

AActor* AEnemyBase::GetTarget() const
{
	return CurrentTarget;
}

bool AEnemyBase::IsDead() const
{
	return bIsDead;
}

bool AEnemyBase::IsMarked() const
{
	return bIsMarked;
}

bool AEnemyBase::ApplyMark()
{
	if (bIsDead || bIsMarked)
	{
		return false;
	}

	bIsMarked = true;
	UpdateMarkIndicatorVisibility();
	OnMarked.Broadcast(this);
	return true;
}

bool AEnemyBase::ConsumeMark()
{
	if (bIsDead || !bIsMarked)
	{
		return false;
	}

	bIsMarked = false;
	UpdateMarkIndicatorVisibility();
	OnMarkConsumed.Broadcast(this);
	return true;
}

void AEnemyBase::ClearMark()
{
	if (!bIsMarked)
	{
		return;
	}

	bIsMarked = false;
	UpdateMarkIndicatorVisibility();
	OnMarkCleared.Broadcast(this);
}

UHealthComponent* AEnemyBase::GetHealthComponent() const
{
	return HealthComponent;
}

void AEnemyBase::ApplySpawnDifficultyScaling(float HealthMultiplier, float DamageMultiplier)
{
	if (HealthComponent)
	{
		const float BaseMaxHealth = HealthComponent->GetMaxHealth();
		const bool bInvalidHealthMultiplier = !FMath::IsFinite(HealthMultiplier);
		const float SafeHealthMultiplier = bInvalidHealthMultiplier ? 1.0f : FMath::Max(0.0f, HealthMultiplier);
		if (bInvalidHealthMultiplier)
		{
			UE_LOG(LogTemp, Warning, TEXT("[EnemyDamage] Invalid spawn health multiplier for %s: %.3f. Using 1.0."),
				*GetNameSafe(this),
				HealthMultiplier);
		}

		HealthComponent->SetMaxHealthPreservePercent(BaseMaxHealth * SafeHealthMultiplier);
	}
}

void AEnemyBase::LogEnemyDebugState(const TCHAR* Context) const
{
	const UHealthComponent* EnemyHealth = HealthComponent;
	const UCapsuleComponent* EnemyCapsuleComponent = GetCapsuleComponent();
	const USkeletalMeshComponent* MeshComponent = GetMesh();
	const UCharacterMovementComponent* MovementComponent = GetCharacterMovement();

	UE_LOG(LogTemp, Log, TEXT("[EnemyDamage] Enemy State Context=%s Name=%s Class=%s Location=%s Health=%.3f/%.3f Percent=%.3f HealthDead=%s EnemyDead=%s CanBeDamaged=%s ActorCollision=%s LifeSpan=%.3f Target=%s Lightweight=%s CharacterMovementTick=%s HealthBarVisible=%s"),
		Context,
		*GetNameSafe(this),
		*GetNameSafe(GetClass()),
		*GetActorLocation().ToString(),
		EnemyHealth ? EnemyHealth->GetCurrentHealth() : -1.0f,
		EnemyHealth ? EnemyHealth->GetMaxHealth() : -1.0f,
		EnemyHealth ? EnemyHealth->GetHealthPercent() : -1.0f,
		EnemyHealth && EnemyHealth->IsDead() ? TEXT("true") : TEXT("false"),
		bIsDead ? TEXT("true") : TEXT("false"),
		CanBeDamaged() ? TEXT("true") : TEXT("false"),
		GetActorEnableCollision() ? TEXT("enabled") : TEXT("disabled"),
		GetLifeSpan(),
		*GetNameSafe(CurrentTarget),
		IsUsingLightweightMovement() ? TEXT("true") : TEXT("false"),
		MovementComponent && MovementComponent->IsComponentTickEnabled() ? TEXT("enabled") : TEXT("disabled"),
		HealthBarWidgetComponent && HealthBarWidgetComponent->IsVisible() ? TEXT("true") : TEXT("false"));

	if (EnemyCapsuleComponent)
	{
		UE_LOG(LogTemp, Log, TEXT("[EnemyDamage] Capsule State Name=%s Collision=%s GenerateOverlap=%s ObjectType=%d PawnResponse=%d EnemyResponse=%d Radius=%.1f HalfHeight=%.1f"),
			*GetNameSafe(EnemyCapsuleComponent),
			*UEnum::GetValueAsString(EnemyCapsuleComponent->GetCollisionEnabled()),
			EnemyCapsuleComponent->GetGenerateOverlapEvents() ? TEXT("true") : TEXT("false"),
			static_cast<int32>(EnemyCapsuleComponent->GetCollisionObjectType()),
			static_cast<int32>(EnemyCapsuleComponent->GetCollisionResponseToChannel(ECC_Pawn)),
			static_cast<int32>(EnemyCapsuleComponent->GetCollisionResponseToChannel(ECC_GameTraceChannel1)),
			EnemyCapsuleComponent->GetScaledCapsuleRadius(),
			EnemyCapsuleComponent->GetScaledCapsuleHalfHeight());
	}

	if (MeshComponent)
	{
		UE_LOG(LogTemp, Log, TEXT("[EnemyDamage] Mesh State Name=%s Collision=%s GenerateOverlap=%s ObjectType=%d PawnResponse=%d EnemyResponse=%d AnimInstance=%s"),
			*GetNameSafe(MeshComponent),
			*UEnum::GetValueAsString(MeshComponent->GetCollisionEnabled()),
			MeshComponent->GetGenerateOverlapEvents() ? TEXT("true") : TEXT("false"),
			static_cast<int32>(MeshComponent->GetCollisionObjectType()),
			static_cast<int32>(MeshComponent->GetCollisionResponseToChannel(ECC_Pawn)),
			static_cast<int32>(MeshComponent->GetCollisionResponseToChannel(ECC_GameTraceChannel1)),
			*GetNameSafe(MeshComponent->GetAnimInstance()));
	}
}

void AEnemyBase::HandleDeath()
{
	if (bIsDead)
	{
		return;
	}

	bIsDead = true;
	ClearMark();
	HideMarkIndicator();
	HideHealthBar();
	SpawnExperiencePickup();
	UpdateAnimationBudgetSignificance();
	CurrentTarget = nullptr;
	StopEnemyBehavior();
	StopBehaviorUpdates();
	StopSeparationUpdates();
	StopEnemyMovement();

	StopEnemyMovement();

	SetActorEnableCollision(false);
	OnEnemyDeath();

	if (!DeathMontage)
	{
		DestroyAfterDeath();
		return;
	}

	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("Enemy death montage skipped: AnimInstance invalid for %s"), *GetNameSafe(this));
		DestroyAfterDeath();
		return;
	}

	const float PlayResult = AnimInstance->Montage_Play(DeathMontage);
	UE_LOG(LogTemp, Log, TEXT("Enemy death montage plays: %s Result=%.3f"), *GetNameSafe(DeathMontage), PlayResult);

	if (PlayResult <= 0.0f)
	{
		DestroyAfterDeath();
		return;
	}

	FOnMontageEnded DeathMontageEndedDelegate;
	DeathMontageEndedDelegate.BindUObject(this, &AEnemyBase::HandleDeathMontageEnded);
	AnimInstance->Montage_SetEndDelegate(DeathMontageEndedDelegate, DeathMontage);
}

void AEnemyBase::HandleHealthChanged(float CurrentHealth, float MaxHealth, float HealthPercent)
{
	UpdateHealthBarVisibility(HealthPercent);
}

void AEnemyBase::HandleDeathMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != DeathMontage)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Enemy death montage finished: %s"), *GetNameSafe(this));
	Destroy();
}

void AEnemyBase::DestroyAfterDeath()
{
	if (DeathDestroyDelay > 0.0f)
	{
		SetLifeSpan(DeathDestroyDelay);
	}
	else
	{
		Destroy();
	}
}

void AEnemyBase::HandlePlayerCharacterSwapped(ACharacterBase* OldCharacter, ACharacterBase* NewCharacter)
{
	SetTarget(NewCharacter);
}

void AEnemyBase::InitializeTargetFromCharacterManager()
{
	EnsureTargetFromCharacterManager();
}

bool AEnemyBase::EnsureTargetFromCharacterManager()
{
	if (CurrentTarget)
	{
		return true;
	}

	ASurvivorPlayerController* SurvivorController = Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	if (!SurvivorController)
	{
		return false;
	}

	ObservedCharacterManager = SurvivorController->GetCharacterManager();
	if (!ObservedCharacterManager)
	{
		return false;
	}

	SetTarget(ObservedCharacterManager->GetActiveCharacter());
	if (!ObservedCharacterManager->OnCharacterSwapped.IsAlreadyBound(this, &AEnemyBase::HandlePlayerCharacterSwapped))
	{
		ObservedCharacterManager->OnCharacterSwapped.AddDynamic(this, &AEnemyBase::HandlePlayerCharacterSwapped);
	}

	if (!CurrentTarget)
	{
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("Enemy %s target acquired: %s"), *GetNameSafe(this), *GetNameSafe(CurrentTarget));
	return true;
}

void AEnemyBase::CachePlayerExperienceComponent()
{
	if (CachedPlayerExperienceComponent)
	{
		return;
	}

	ASurvivorPlayerController* SurvivorController = Cast<ASurvivorPlayerController>(ObservedCharacterManager ? ObservedCharacterManager->GetOwner() : nullptr);
	if (!SurvivorController)
	{
		SurvivorController = Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	}

	CachedPlayerExperienceComponent = SurvivorController ? SurvivorController->GetExperienceComponent() : nullptr;
}

void AEnemyBase::SpawnExperiencePickup()
{
	if (bExperiencePickupSpawned || XPReward <= 0)
	{
		return;
	}

	bExperiencePickupSpawned = true;

	if (!ExperiencePickupClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Enemy %s could not drop XP: ExperiencePickupClass is not configured."), *GetNameSafe(this));
		return;
	}

	CachePlayerExperienceComponent();
	if (!CachedPlayerExperienceComponent || !ObservedCharacterManager || !GetWorld())
	{
		UE_LOG(LogTemp, Warning, TEXT("Enemy %s could not drop XP: shared player references invalid."), *GetNameSafe(this));
		return;
	}

	FVector SpawnLocation = GetActorLocation();
	if (ExperiencePickupSpawnScatterRadius > 0.0f)
	{
		const FVector2D RandomOffset = FMath::RandPointInCircle(ExperiencePickupSpawnScatterRadius);
		SpawnLocation.X += RandomOffset.X;
		SpawnLocation.Y += RandomOffset.Y;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AExperiencePickup* Pickup = GetWorld()->SpawnActor<AExperiencePickup>(
		ExperiencePickupClass,
		SpawnLocation,
		FRotator::ZeroRotator,
		SpawnParameters);

	if (!Pickup)
	{
		UE_LOG(LogTemp, Warning, TEXT("Enemy %s failed to spawn XP pickup class %s."), *GetNameSafe(this), *GetNameSafe(ExperiencePickupClass.Get()));
		return;
	}

	Pickup->InitializePickup(XPReward, CachedPlayerExperienceComponent, ObservedCharacterManager);
	UE_LOG(LogTemp, Log, TEXT("Enemy died: %s dropped %d XP"), *GetNameSafe(this), XPReward);
}

void AEnemyBase::InitializeHealthBar()
{
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

void AEnemyBase::StartBehaviorUpdates()
{
	if (!GetWorld() || BehaviorUpdateInterval <= 0.0f || bIsDead)
	{
		return;
	}

	const float InitialDelay = FMath::FRandRange(0.0f, BehaviorUpdateInterval);
	GetWorld()->GetTimerManager().SetTimer(
		BehaviorUpdateTimerHandle,
		this,
		&AEnemyBase::HandleBehaviorUpdateTimer,
		BehaviorUpdateInterval,
		true,
		InitialDelay);
}

void AEnemyBase::StopBehaviorUpdates()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BehaviorUpdateTimerHandle);
	}
}

void AEnemyBase::HandleBehaviorUpdateTimer()
{
	UpdateCharacterMovementProfilingState();
	UpdateAnimationBudgetSignificance();

	if (IsEnemyCharacterMovementProfilingDisabled())
	{
		StopEnemyMovement();
		return;
	}

	UpdateEnemyBehavior(BehaviorUpdateInterval);
}

void AEnemyBase::StartSeparationUpdates()
{
	if (!GetWorld() || bIsDead || !IsUsingLightweightMovement() || !bUseEnemySeparation || SeparationRadius <= 0.0f || SeparationUpdateInterval <= 0.0f)
	{
		CachedEnemySeparationVector = FVector::ZeroVector;
		return;
	}

	const float InitialDelay = FMath::FRandRange(0.0f, SeparationUpdateInterval);
	GetWorld()->GetTimerManager().SetTimer(
		SeparationUpdateTimerHandle,
		this,
		&AEnemyBase::HandleSeparationUpdateTimer,
		SeparationUpdateInterval,
		true,
		InitialDelay);
}

void AEnemyBase::StopSeparationUpdates()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SeparationUpdateTimerHandle);
	}

	CachedEnemySeparationVector = FVector::ZeroVector;
}

void AEnemyBase::HandleSeparationUpdateTimer()
{
	if (bIsDead || !IsUsingLightweightMovement() || !bUseEnemySeparation)
	{
		CachedEnemySeparationVector = FVector::ZeroVector;
		return;
	}

	UpdateCachedEnemySeparation();
}

void AEnemyBase::ApplyDesiredMovementInput(float DeltaSeconds)
{
	if (!bIsDead && bHasDesiredMovementDirection && !IsEnemyCharacterMovementProfilingDisabled())
	{
		SmoothFaceTarget(DeltaSeconds);
		RequestEnemyMovement(DesiredMovementDirection);
	}
}

void AEnemyBase::StopDesiredMovement()
{
	DesiredMovementDirection = FVector::ZeroVector;
	DesiredDirectMovementDirection = FVector::ZeroVector;
	CachedNavSteeringDirection = FVector::ZeroVector;
	bHasDesiredMovementDirection = false;
}

void AEnemyBase::InitializeEnemyMovementMode()
{
	if (LightweightMovementComponent)
	{
		LightweightMovementComponent->SetMoveSpeed(MoveSpeed);
		LightweightMovementComponent->SetMovementEnabled(bUseLightweightMovement);
	}

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->MaxWalkSpeed = MoveSpeed;

		if (bUseLightweightMovement)
		{
			MovementComponent->StopMovementImmediately();
			MovementComponent->DisableMovement();
			MovementComponent->SetComponentTickEnabled(false);
		}
	}

	if (UCapsuleComponent* EnemyCapsule = GetCapsuleComponent())
	{
		ConfigureEnemyCapsuleCollisionDefaults();
	}

	if (bUseLightweightMovement)
	{
		if (USkeletalMeshComponent* MeshComponent = GetMesh())
		{
			MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			MeshComponent->SetGenerateOverlapEvents(false);
		}
	}
}

void AEnemyBase::ConfigureEnemyCapsuleCollisionDefaults()
{
	if (UCapsuleComponent* EnemyCapsule = GetCapsuleComponent())
	{
		EnemyCapsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		EnemyCapsule->SetGenerateOverlapEvents(true);
		EnemyCapsule->SetCollisionObjectType(ECC_GameTraceChannel1);
		EnemyCapsule->SetCollisionResponseToAllChannels(ECR_Block);
		EnemyCapsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		EnemyCapsule->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);
	}
}

void AEnemyBase::ConfigureEnemyMovementDefaults()
{
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->bOrientRotationToMovement = false;
		MovementComponent->bUseControllerDesiredRotation = false;
		MovementComponent->bUseRVOAvoidance = false;
	}
}

void AEnemyBase::InitializeCrowdSpread()
{
	const uint32 Seed = GetTypeHash(GetFName()) ^ GetUniqueID();
	const FRandomStream RandomStream(Seed);
	const float BiasSign = RandomStream.FRand() < 0.5f ? -1.0f : 1.0f;
	CrowdSpreadBias = BiasSign * RandomStream.FRandRange(0.35f, 1.0f);
}

FVector AEnemyBase::ApplyCrowdSpreadToDirection(const FVector& DirectDirection) const
{
	if (!IsUsingLightweightMovement() || !bUseCrowdSpread || CrowdSpreadStrength <= 0.0f)
	{
		return DirectDirection;
	}

	FVector SafeDirectDirection = DirectDirection;
	SafeDirectDirection.Z = 0.0f;
	if (!SafeDirectDirection.Normalize())
	{
		return FVector::ZeroVector;
	}

	const FVector SideDirection(-SafeDirectDirection.Y, SafeDirectDirection.X, 0.0f);
	FVector SpreadDirection = SafeDirectDirection + SideDirection * CrowdSpreadBias * CrowdSpreadStrength;
	SpreadDirection.Z = 0.0f;
	if (!SpreadDirection.Normalize())
	{
		return SafeDirectDirection;
	}

	return SpreadDirection;
}

FVector AEnemyBase::ApplyEnemySeparationToDirection(const FVector& MovementDirection) const
{
	if (!IsUsingLightweightMovement() || !bUseEnemySeparation || SeparationStrength <= 0.0f || CachedEnemySeparationVector.IsNearlyZero())
	{
		return MovementDirection;
	}

	FVector SafeMovementDirection = MovementDirection;
	SafeMovementDirection.Z = 0.0f;
	if (!SafeMovementDirection.Normalize())
	{
		return MovementDirection;
	}

	FVector SeparationContribution = CachedEnemySeparationVector.GetClampedToMaxSize(MaxSeparationContribution) * SeparationStrength;
	SeparationContribution.Z = 0.0f;

	FVector FinalDirection = SafeMovementDirection + SeparationContribution;
	FinalDirection.Z = 0.0f;
	if (!FinalDirection.Normalize())
	{
		return SafeMovementDirection;
	}

	if (FVector::DotProduct(FinalDirection, SafeMovementDirection) < 0.35f)
	{
		FinalDirection = (SafeMovementDirection * 0.65f + FinalDirection * 0.35f).GetSafeNormal();
	}

	return FinalDirection;
}

FVector AEnemyBase::ApplyLightweightNavSteeringToDirection(const FVector& DirectDirection)
{
	if (!IsUsingLightweightMovement() || !bUseLightweightNavSteering || !LightweightMovementComponent || !LightweightMovementComponent->WasLastMoveBlockedByWorldGeometry())
	{
		CachedNavSteeringDirection = FVector::ZeroVector;
		return DirectDirection;
	}

	UpdateLightweightNavSteeringDirection(DirectDirection);
	return CachedNavSteeringDirection.IsNearlyZero() ? DirectDirection : CachedNavSteeringDirection;
}

void AEnemyBase::UpdateLightweightNavSteeringDirection(const FVector& DirectDirection)
{
	if (!GetWorld() || !CurrentTarget)
	{
		CachedNavSteeringDirection = FVector::ZeroVector;
		return;
	}

	const float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime < NextNavSteeringUpdateTime && !CachedNavSteeringDirection.IsNearlyZero())
	{
		return;
	}

	NextNavSteeringUpdateTime = CurrentTime + FMath::Max(0.05f, NavSteeringUpdateInterval);

	UNavigationPath* NavigationPath = UNavigationSystemV1::FindPathToLocationSynchronously(
		this,
		GetActorLocation(),
		CurrentTarget->GetActorLocation(),
		this);

	if (!NavigationPath || !NavigationPath->IsValid() || NavigationPath->PathPoints.Num() < 2)
	{
		CachedNavSteeringDirection = DirectDirection;
		return;
	}

	const int32 PathPointIndex = FMath::Clamp(NavSteeringPathPointLookAhead, 1, NavigationPath->PathPoints.Num() - 1);
	FVector NavDirection = NavigationPath->PathPoints[PathPointIndex] - GetActorLocation();
	NavDirection.Z = 0.0f;
	if (!NavDirection.Normalize())
	{
		CachedNavSteeringDirection = DirectDirection;
		return;
	}

	CachedNavSteeringDirection = NavDirection;

	if (bDebugLightweightNavSteering)
	{
		const FVector DebugStart = GetActorLocation() + FVector(0.0f, 0.0f, 55.0f);
		DrawDebugLine(GetWorld(), DebugStart, DebugStart + CachedNavSteeringDirection * 220.0f, FColor::Purple, false, NavSteeringUpdateInterval, 0, 3.0f);
		DrawDebugSphere(GetWorld(), NavigationPath->PathPoints[PathPointIndex], 35.0f, 12, FColor::Purple, false, NavSteeringUpdateInterval, 0, 2.0f);
	}
}

void AEnemyBase::UpdateCachedEnemySeparation()
{
	CachedEnemySeparationVector = FVector::ZeroVector;

	if (!GetWorld() || SeparationRadius <= 0.0f)
	{
		return;
	}

	TArray<FOverlapResult> OverlapResults;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemySeparation), false, this);
	QueryParams.AddIgnoredActor(this);

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_GameTraceChannel1);

	GetWorld()->OverlapMultiByObjectType(
		OverlapResults,
		GetActorLocation(),
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(SeparationRadius),
		QueryParams);

	const FVector MyLocation = GetActorLocation();
	FVector Separation = FVector::ZeroVector;
	int32 NeighborCount = 0;

	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AEnemyBase* OtherEnemy = Cast<AEnemyBase>(OverlapResult.GetActor());
		if (!OtherEnemy || OtherEnemy == this || OtherEnemy->IsDead())
		{
			continue;
		}

		FVector Away = MyLocation - OtherEnemy->GetActorLocation();
		Away.Z = 0.0f;
		const float Distance = Away.Size2D();
		if (Distance > SeparationRadius)
		{
			continue;
		}

		if (Distance <= KINDA_SMALL_NUMBER)
		{
			const uint32 Seed = GetUniqueID() ^ OtherEnemy->GetUniqueID();
			const float Angle = static_cast<float>(Seed % 360);
			Away = FVector(FMath::Cos(FMath::DegreesToRadians(Angle)), FMath::Sin(FMath::DegreesToRadians(Angle)), 0.0f);
		}
		else
		{
			Away /= Distance;
		}

		const float Weight = FMath::Clamp(1.0f - (Distance / SeparationRadius), 0.0f, 1.0f);
		Separation += Away * Weight;
		++NeighborCount;
	}

	if (NeighborCount > 0)
	{
		CachedEnemySeparationVector = Separation.GetClampedToMaxSize(MaxSeparationContribution);
	}

	if (CVarDebugEnemySeparation.GetValueOnGameThread() != 0 && GetWorld() && (GetUniqueID() % 24 == 0))
	{
		const FVector DebugStart = GetActorLocation() + FVector(0.0f, 0.0f, 35.0f);
		DrawDebugSphere(GetWorld(), GetActorLocation(), SeparationRadius, 16, FColor::Blue, false, SeparationUpdateInterval, 0, 1.0f);
		DrawDebugLine(GetWorld(), DebugStart, DebugStart + CachedEnemySeparationVector * 180.0f, FColor::Magenta, false, SeparationUpdateInterval, 0, 2.0f);
	}
}

void AEnemyBase::RequestEnemyMovement(const FVector& WorldDirection)
{
	if (IsUsingLightweightMovement())
	{
		LightweightMovementComponent->RequestMove(WorldDirection);
		return;
	}

	AddMovementInput(WorldDirection);
}

void AEnemyBase::StopEnemyMovement()
{
	StopDesiredMovement();

	if (LightweightMovementComponent)
	{
		LightweightMovementComponent->StopMovement();
	}

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
	}
}

FVector AEnemyBase::GetEnemyMovementVelocity() const
{
	if (IsUsingLightweightMovement())
	{
		return LightweightMovementComponent->GetCurrentVelocity();
	}

	if (const UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		return MovementComponent->Velocity;
	}

	return FVector::ZeroVector;
}

bool AEnemyBase::IsUsingLightweightMovement() const
{
	return bUseLightweightMovement && LightweightMovementComponent && LightweightMovementComponent->IsMovementEnabled();
}

bool AEnemyBase::IsEnemyCharacterMovementProfilingDisabled() const
{
	return CVarDisableEnemyCharacterMovementForProfiling.GetValueOnGameThread() != 0;
}

void AEnemyBase::UpdateCharacterMovementProfilingState()
{
	const bool bShouldDisableMovement = IsEnemyCharacterMovementProfilingDisabled();
	if (bUseLightweightMovement)
	{
		bCharacterMovementDisabledForProfiling = true;
		if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
		{
			MovementComponent->StopMovementImmediately();
			MovementComponent->DisableMovement();
			MovementComponent->SetComponentTickEnabled(false);
		}
		return;
	}

	if (bCharacterMovementDisabledForProfiling == bShouldDisableMovement)
	{
		return;
	}

	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	if (!MovementComponent)
	{
		bCharacterMovementDisabledForProfiling = bShouldDisableMovement;
		return;
	}

	if (bShouldDisableMovement)
	{
		StopEnemyMovement();
		MovementComponent->DisableMovement();
		MovementComponent->SetComponentTickEnabled(false);
	}
	else if (!bIsDead)
	{
		MovementComponent->SetComponentTickEnabled(true);
		MovementComponent->SetMovementMode(MOVE_Walking);
		MovementComponent->MaxWalkSpeed = MoveSpeed;
	}

	bCharacterMovementDisabledForProfiling = bShouldDisableMovement;
}

bool AEnemyBase::IsEnemyAnimationProfilingDisabled() const
{
	return CVarDisableEnemyAnimationForProfiling.GetValueOnGameThread() != 0;
}

void AEnemyBase::UpdateAnimationProfilingState()
{
	const bool bShouldDisableAnimation = IsEnemyAnimationProfilingDisabled();
	if (bAnimationDisabledForProfiling == bShouldDisableAnimation)
	{
		return;
	}

	USkeletalMeshComponent* MeshComponent = GetMesh();
	if (!MeshComponent)
	{
		bAnimationDisabledForProfiling = bShouldDisableAnimation;
		return;
	}

	if (bShouldDisableAnimation)
	{
		MeshComponent->bPauseAnims = true;
		MeshComponent->bNoSkeletonUpdate = true;
		MeshComponent->SetComponentTickEnabled(false);
	}
	else
	{
		MeshComponent->bPauseAnims = false;
		MeshComponent->bNoSkeletonUpdate = false;
		MeshComponent->SetComponentTickEnabled(true);
	}

	bAnimationDisabledForProfiling = bShouldDisableAnimation;
}

void AEnemyBase::InitializeAnimationBudgeting()
{
	if (!bUseAnimationBudgetAllocator || bAnimationBudgetInitialized || !GetWorld())
	{
		return;
	}

	USkeletalMeshComponentBudgeted* BudgetedMesh = Cast<USkeletalMeshComponentBudgeted>(GetMesh());
	if (!BudgetedMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("Enemy %s mesh is not USkeletalMeshComponentBudgeted. Animation budgeting unavailable."), *GetNameSafe(this));
		return;
	}

	BudgetedMesh->SetAutoRegisterWithBudgetAllocator(true);
	BudgetedMesh->SetAutoCalculateSignificance(false);
	BudgetedMesh->SetShouldUseActorRenderedFlag(true);
	BudgetedMesh->bComponentUseFixedSkelBounds = bUseFixedSkelBoundsForEnemies;

	bool bAllocatorEnabled = false;
	if (IAnimationBudgetAllocator* AnimationBudgetAllocator = IAnimationBudgetAllocator::Get(GetWorld()))
	{
		FAnimationBudgetAllocatorParameters BudgetParameters;
		BudgetParameters.BudgetInMs = AnimationBudgetMs;
		BudgetParameters.MinQuality = 0.0f;
		BudgetParameters.MaxTickRate = 15;
		BudgetParameters.InterpolationMaxRate = 8;
		BudgetParameters.MaxInterpolatedComponents = 96;
		BudgetParameters.MaxTickedOffsreenComponents = 4;
		BudgetParameters.AutoCalculatedSignificanceMaxDistance = AnimationBudgetMaxSignificanceDistance;
		BudgetParameters.AutoCalculatedSignificanceMaxDistanceSqr = FMath::Square(AnimationBudgetMaxSignificanceDistance);

		AnimationBudgetAllocator->SetParameters(BudgetParameters);
		bAllocatorEnabled = AnimationBudgetAllocator->GetEnabled();
	}

	bAnimationBudgetInitialized = true;
	UpdateAnimationBudgetSignificance();
	LogAnimationBudgetSetup(BudgetedMesh, bAllocatorEnabled);
}

void AEnemyBase::UpdateAnimationBudgetSignificance()
{
	if (!bUseAnimationBudgetAllocator)
	{
		return;
	}

	USkeletalMeshComponentBudgeted* BudgetedMesh = Cast<USkeletalMeshComponentBudgeted>(GetMesh());
	if (!BudgetedMesh)
	{
		return;
	}

	float Significance = 0.0f;
	const bool bForceHighPriority = ShouldForceHighAnimationBudgetSignificance();
	const bool bNeverSkip = bForceHighPriority;
	const bool bTickEvenIfNotRendered = bNeverSkip;
	const bool bAllowReducedWork = !bNeverSkip;
	const bool bForceInterpolate = false;

	if (CurrentTarget)
	{
		const float Distance = FVector::Dist2D(GetActorLocation(), CurrentTarget->GetActorLocation());
		const float SafeMaxDistance = FMath::Max(AnimationBudgetHighSignificanceDistance + 1.0f, AnimationBudgetMaxSignificanceDistance);

		if (Distance <= AnimationBudgetHighSignificanceDistance)
		{
			Significance = 1.0f;
		}
		else
		{
			Significance = FMath::Clamp(
				1.0f - ((Distance - AnimationBudgetHighSignificanceDistance) / (SafeMaxDistance - AnimationBudgetHighSignificanceDistance)),
				0.0f,
				1.0f);
		}
	}

	if (bNeverSkip)
	{
		Significance = 1.0f;
	}

	BudgetedMesh->SetComponentSignificance(Significance, bNeverSkip, bTickEvenIfNotRendered, bAllowReducedWork, bForceInterpolate);
}

void AEnemyBase::LogAnimationBudgetSetup(USkeletalMeshComponentBudgeted* BudgetedMesh, bool bAllocatorEnabled) const
{
	if (CVarLogEnemyAnimationBudgetSetup.GetValueOnGameThread() == 0)
	{
		return;
	}

	bool bAutoRegister = false;
	if (BudgetedMesh)
	{
		if (const FBoolProperty* AutoRegisterProperty = FindFProperty<FBoolProperty>(BudgetedMesh->GetClass(), TEXT("bAutoRegisterWithBudgetAllocator")))
		{
			bAutoRegister = AutoRegisterProperty->GetPropertyValue_InContainer(BudgetedMesh);
		}
	}

	const float DistanceToTarget = CurrentTarget ? FVector::Dist2D(GetActorLocation(), CurrentTarget->GetActorLocation()) : -1.0f;
	const bool bForceHighPriority = ShouldForceHighAnimationBudgetSignificance();
	const bool bNeverSkip = bForceHighPriority;
	const bool bIsHighSignificanceByDistance = DistanceToTarget >= 0.0f && DistanceToTarget <= AnimationBudgetHighSignificanceDistance;
	const float SafeMaxDistance = FMath::Max(AnimationBudgetHighSignificanceDistance + 1.0f, AnimationBudgetMaxSignificanceDistance);
	const float EstimatedSignificance = DistanceToTarget < 0.0f
		? 0.0f
		: (bForceHighPriority || bIsHighSignificanceByDistance ? 1.0f : FMath::Clamp(1.0f - ((DistanceToTarget - AnimationBudgetHighSignificanceDistance) / (SafeMaxDistance - AnimationBudgetHighSignificanceDistance)), 0.0f, 1.0f));

	UE_LOG(LogTemp, Log, TEXT("Enemy Animation Budget Setup: Enemy=%s MeshClass=%s IsBudgetedComponent=%s AutoRegisterWithBudgetAllocator=%s AllocatorEnabled=%s AutoCalculateSignificance=%s DistanceToTarget=%.2f EstimatedSignificance=%.3f NeverSkip=%s ForceHighPriority=%s HighSignificanceByDistance=%s BudgetInMs=%.2f MaxSignificanceDistance=%.2f HighSignificanceDistance=%.2f"),
		*GetNameSafe(this),
		*GetNameSafe(BudgetedMesh ? BudgetedMesh->GetClass() : nullptr),
		BudgetedMesh ? TEXT("true") : TEXT("false"),
		bAutoRegister ? TEXT("true") : TEXT("false"),
		bAllocatorEnabled ? TEXT("true") : TEXT("false"),
		(BudgetedMesh && BudgetedMesh->GetAutoCalculateSignificance()) ? TEXT("true") : TEXT("false"),
		DistanceToTarget,
		EstimatedSignificance,
		bNeverSkip ? TEXT("true") : TEXT("false"),
		bForceHighPriority ? TEXT("true") : TEXT("false"),
		bIsHighSignificanceByDistance ? TEXT("true") : TEXT("false"),
		AnimationBudgetMs,
		AnimationBudgetMaxSignificanceDistance,
		AnimationBudgetHighSignificanceDistance);
}

bool AEnemyBase::ShouldForceHighAnimationBudgetSignificance() const
{
	return false;
}

void AEnemyBase::UpdateEnemyBehavior(float DeltaSeconds)
{
	if (IsEnemyCharacterMovementProfilingDisabled())
	{
		StopEnemyMovement();
		return;
	}

	if (bIsDead || IsPlayerTargetDead() || ShouldSkipMovement())
	{
		StopEnemyMovement();
		return;
	}

	if (!EnsureTargetFromCharacterManager())
	{
		StopEnemyMovement();
		return;
	}

	MoveTowardCurrentTarget();
}

bool AEnemyBase::ShouldSkipMovement() const
{
	return false;
}

void AEnemyBase::StopEnemyBehavior()
{
}

void AEnemyBase::MoveTowardCurrentTarget()
{
	if (!CurrentTarget)
	{
		return;
	}

	FVector ToTarget = CurrentTarget->GetActorLocation() - GetActorLocation();
	ToTarget.Z = 0.0f;
	if (ToTarget.SizeSquared2D() <= FMath::Square(StopDistance))
	{
		StopEnemyMovement();
		return;
	}

	if (ToTarget.Normalize())
	{
		DesiredDirectMovementDirection = ToTarget;
		const FVector SteeringDirection = ApplyLightweightNavSteeringToDirection(ToTarget);
		DesiredMovementDirection = ApplyEnemySeparationToDirection(ApplyCrowdSpreadToDirection(SteeringDirection));
		bHasDesiredMovementDirection = true;

		if (bDebugCrowdSpread && GetWorld() && IsUsingLightweightMovement())
		{
			const FVector DebugStart = GetActorLocation() + FVector(0.0f, 0.0f, 40.0f);
			DrawDebugLine(GetWorld(), DebugStart, DebugStart + DesiredDirectMovementDirection * 160.0f, FColor::Green, false, 0.0f, 0, 2.0f);
			DrawDebugLine(GetWorld(), DebugStart, DebugStart + DesiredMovementDirection * 160.0f, FColor::Cyan, false, 0.0f, 0, 2.0f);
		}
	}
}

void AEnemyBase::FaceTarget()
{
	if (!CurrentTarget)
	{
		return;
	}

	FVector ToTarget = CurrentTarget->GetActorLocation() - GetActorLocation();
	ToTarget.Z = 0.0f;
	if (ToTarget.Normalize())
	{
		SetActorRotation(FRotator(0.0f, ToTarget.Rotation().Yaw, 0.0f));
	}
}

void AEnemyBase::SmoothFaceTarget(float DeltaSeconds)
{
	if (!CurrentTarget || DeltaSeconds <= 0.0f || EnemyRotationSpeed <= 0.0f)
	{
		return;
	}

	FVector ToTarget = CurrentTarget->GetActorLocation() - GetActorLocation();
	ToTarget.Z = 0.0f;
	if (!ToTarget.Normalize())
	{
		return;
	}

	const FRotator CurrentRotation = GetActorRotation();
	const FRotator TargetRotation(0.0f, ToTarget.Rotation().Yaw, 0.0f);
	const FRotator NewRotation = FMath::RInterpConstantTo(CurrentRotation, TargetRotation, DeltaSeconds, EnemyRotationSpeed);
	SetActorRotation(FRotator(0.0f, NewRotation.Yaw, 0.0f));
}

bool AEnemyBase::IsPlayerTargetDead() const
{
	ASurvivorPlayerController* SurvivorController = Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	return SurvivorController && SurvivorController->IsPlayerDead();
}
