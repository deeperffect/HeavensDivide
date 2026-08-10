// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemyBase.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AnimationBudgetAllocatorParameters.h"
#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "EnemyHealthBarWidget.h"
#include "EnemyLightweightMovementComponent.h"
#include "ExperiencePickup.h"
#include "ExperienceComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HealthComponent.h"
#include "IAnimationBudgetAllocator.h"
#include "Kismet/GameplayStatics.h"
#include "SkeletalMeshComponentBudgeted.h"
#include "SurvivorPlayerController.h"
#include "TimerManager.h"
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
	InitializeTargetFromCharacterManager();
	CachePlayerExperienceComponent();
	InitializeAnimationBudgeting();
	UpdateCharacterMovementProfilingState();
	UpdateAnimationProfilingState();
	StartBehaviorUpdates();
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

UHealthComponent* AEnemyBase::GetHealthComponent() const
{
	return HealthComponent;
}

void AEnemyBase::HandleDeath()
{
	if (bIsDead)
	{
		return;
	}

	bIsDead = true;
	SpawnExperiencePickup();
	UpdateAnimationBudgetSignificance();
	CurrentTarget = nullptr;
	StopEnemyBehavior();
	StopBehaviorUpdates();
	StopEnemyMovement();

	if (HealthBarWidgetComponent)
	{
		UpdateHealthBarVisibility(0.0f);
	}

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
		EnemyCapsule->SetCollisionObjectType(ECC_GameTraceChannel1);
		EnemyCapsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		EnemyCapsule->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);
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
		DesiredMovementDirection = ApplyCrowdSpreadToDirection(ToTarget);
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
