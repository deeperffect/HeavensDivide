// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemyBase.h"

#include "HealingPickupDropSubsystem.h"

#include "Animation/AnimInstance.h"
#include "EnemyDeathComponent.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "AnimationBudgetAllocatorParameters.h"
#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "EnemyHealthBarWidget.h"
#include "EnemyLightweightMovementComponent.h"
#include "EnemyMarkIndicatorWidget.h"
#include "EnemyStatusEffectComponent.h"
#include "EnemyStatusIndicatorWidget.h"
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
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInterface.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NinjaCharacter.h"
#include "SamuraiCharacter.h"
#include "SkeletalMeshComponentBudgeted.h"
#include "Stats/Stats.h"
#include "SurvivorPlayerController.h"
#include "TimerManager.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "UObject/UnrealType.h"

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

static TAutoConsoleVariable<int32> CVarDebugEnemyPathFallback(
	TEXT("hd.DebugEnemyPathFallback"),
	0,
	TEXT("When set to 1, draws enemy lightweight obstacle path fallback debug."));

static TAutoConsoleVariable<int32> CVarDebugEnemyGroundSnap(
	TEXT("hd.DebugEnemyGroundSnap"),
	0,
	TEXT("When set to 1, logs suspicious one-time enemy ground snap corrections."));

static constexpr float EnemyWalkableGroundNormalZ = 0.7f;
static constexpr ECollisionChannel EnemyGroundSnapTraceChannel = ECC_GameTraceChannel2;

static bool IsBlockingWorldGeometryHit(const FHitResult& HitResult)
{
	AActor* HitActor = HitResult.GetActor();
	return HitResult.bBlockingHit
		&& HitActor
		&& !Cast<AEnemyBase>(HitActor)
		&& !Cast<ACharacterBase>(HitActor);
}

static bool IsValidEnemyGroundHit(const FHitResult& HitResult)
{
	AActor* HitActor = HitResult.GetActor();
	return HitResult.bBlockingHit
		&& HitResult.GetComponent()
		&& (!HitActor || (!Cast<AEnemyBase>(HitActor) && !Cast<ACharacterBase>(HitActor)))
		&& HitResult.ImpactNormal.Z >= EnemyWalkableGroundNormalZ;
}

static bool IsEnemyPathFallbackRequestAllowed(UWorld* World)
{
	if (!World)
	{
		return false;
	}

	constexpr uint64 MaxRequestsPerFrame = 10;
	static uint64 LastFrameNumber = 0;
	static uint64 RequestsThisFrame = 0;

	if (LastFrameNumber != GFrameCounter)
	{
		LastFrameNumber = GFrameCounter;
		RequestsThisFrame = 0;
	}

	if (RequestsThisFrame >= MaxRequestsPerFrame)
	{
		return false;
	}

	++RequestsThisFrame;
	return true;
}

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

	EnemyDeathComponent = CreateDefaultSubobject<UEnemyDeathComponent>(TEXT("EnemyDeathComponent"));
	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FlashMaterial(TEXT("/Game/HeavensDivide/Materials/M_EnemyHitFlash.M_EnemyHitFlash"));
	if (FlashMaterial.Succeeded()) HitFlashMaterial = FlashMaterial.Object;
	StatusEffectComponent = CreateDefaultSubobject<UEnemyStatusEffectComponent>(TEXT("StatusEffectComponent"));
	LightweightMovementComponent = CreateDefaultSubobject<UEnemyLightweightMovementComponent>(TEXT("LightweightMovementComponent"));
	BloodboundNiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("BloodboundNiagaraComponent"));
	BloodboundNiagaraComponent->SetupAttachment(GetMesh());
	BloodboundNiagaraComponent->SetAutoActivate(false);

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

	BleedStatusWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("BleedStatusWidgetComponent"));
	BleedStatusWidgetComponent->SetupAttachment(RootComponent);
	PoisonStatusWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("PoisonStatusWidgetComponent"));
	PoisonStatusWidgetComponent->SetupAttachment(RootComponent);
	for (UWidgetComponent* StatusWidget : { BleedStatusWidgetComponent.Get(), PoisonStatusWidgetComponent.Get() })
	{
		StatusWidget->SetWidgetSpace(EWidgetSpace::Screen);
		StatusWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		StatusWidget->SetHiddenInGame(true);
		StatusWidget->SetVisibility(false);
		StatusWidget->SetComponentTickEnabled(false);
		StatusWidget->SetWidgetClass(UEnemyStatusIndicatorWidget::StaticClass());
	}

	ConfigureEnemyCapsuleCollisionDefaults();

	const uint32 PathSeed = GetTypeHash(GetFName()) ^ GetUniqueID() ^ 0x9E3779B9;
	const FRandomStream PathRandomStream(PathSeed);
	PathFallbackRequestJitter = PathRandomStream.FRandRange(0.0f, 0.08f);
}

void AEnemyBase::InitializeHitFlash()
{
	if (!bEnableHitFlash || !HitFlashMaterial || GetNetMode() == NM_DedicatedServer) return;
	if (!HitFlashMID)
	{
		HitFlashMID = UMaterialInstanceDynamic::Create(HitFlashMaterial, this);
		HitFlashMID->SetVectorParameterValue(TEXT("FlashColor"), HitFlashColor);
		HitFlashMID->SetScalarParameterValue(TEXT("FlashOpacity"), FMath::Clamp(HitFlashOpacity, 0.0f, 1.0f));
	}
	if (HealthComponent) HealthComponent->OnDamaged.AddUniqueDynamic(this, &AEnemyBase::HandleHitFlashDamage);
}

void AEnemyBase::HandleHitFlashDamage(float DamageAmount, float CurrentHealth)
{
	if (CurrentHealth <= 0.0f || bIsDead) { EndHitFlash(); return; }
	if (bSuppressHitFlash) return;
	if (!bEnableHitFlash || DamageAmount <= 0.0f || HitFlashDuration <= 0.0f || !HitFlashMID || !GetMesh()) return;
	// Repeated hits extend the timer without caching our flash as the original overlay.
	if (GetMesh()->GetOverlayMaterial() != HitFlashMID)
		PreHitFlashOverlay = GetMesh()->GetOverlayMaterial();
	GetMesh()->SetOverlayMaterial(HitFlashMID);
	GetWorldTimerManager().SetTimer(HitFlashTimer, this, &AEnemyBase::EndHitFlash, HitFlashDuration, false);
}

void AEnemyBase::EndHitFlash()
{
	GetWorldTimerManager().ClearTimer(HitFlashTimer);
	// Do not overwrite a newer visual state installed by another reaction.
	if (HitFlashMID && GetMesh() && GetMesh()->GetOverlayMaterial() == HitFlashMID)
		GetMesh()->SetOverlayMaterial(PreHitFlashOverlay);
	PreHitFlashOverlay = nullptr;
}

void AEnemyBase::BeginPlay()
{
	Super::BeginPlay();
	InitializeHitFlash();

	SnapToGroundBeforeLightweightMovement();
	InitializeEnemyMovementMode();
	InitializeCrowdSpread();

	if (HealthComponent)
	{
		HealthComponent->OnDeath.AddDynamic(this, &AEnemyBase::HandleDeath);
		HealthComponent->OnHealthChanged.AddDynamic(this, &AEnemyBase::HandleHealthChanged);
	}

	InitializeHealthBar();
	InitializeMarkIndicator();
	InitializeStatusIndicators();
	if (StatusEffectComponent) StatusEffectComponent->OnStatusStacksChanged.AddUniqueDynamic(this, &AEnemyBase::HandleStatusStacksChanged);
	InitializeTargetFromCharacterManager();
	CachePlayerExperienceComponent();
	InitializeAnimationBudgeting();
	UpdateAnimationProfilingState();
	StartBehaviorUpdates();
	StartSeparationUpdates();

	if (bIsBloodbound)
	{
		RefreshEnemyVisualState();
	}
}

void AEnemyBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	EndHitFlash();
	if (HealthComponent) HealthComponent->OnDamaged.RemoveDynamic(this, &AEnemyBase::HandleHitFlashDamage);
	if (ObservedCharacterManager)
	{
		ObservedCharacterManager->OnCharacterSwapped.RemoveDynamic(this, &AEnemyBase::HandlePlayerCharacterSwapped);
	}

	if (HealthComponent)
	{
		HealthComponent->OnHealthChanged.RemoveDynamic(this, &AEnemyBase::HandleHealthChanged);
		HealthComponent->OnDeath.RemoveDynamic(this, &AEnemyBase::HandleDeath);
	}
	if (StatusEffectComponent)
	{
		StatusEffectComponent->OnStatusStacksChanged.RemoveDynamic(this, &AEnemyBase::HandleStatusStacksChanged);
	}

	StopEnemyBehavior();
	StopBehaviorUpdates();
	StopSeparationUpdates();

	Super::EndPlay(EndPlayReason);
}

void AEnemyBase::Tick(float DeltaSeconds)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_EnemyBase_Tick);
	Super::Tick(DeltaSeconds);

	ApplyDesiredMovementInput(DeltaSeconds);
}

FVector AEnemyBase::GetVelocity() const
{
	return GetEnemyMovementVelocity();
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

void AEnemyBase::ConfigureForStressTest(bool bDisableCombat, bool bMakeInvulnerable)
{
	bIsStressTestEnemy = true;
	bStressTestDisableCombat = bDisableCombat;
	bStressTestInvulnerable = bMakeInvulnerable;

	if (HealthComponent)
	{
		HealthComponent->SetDamageEnabled(!bStressTestInvulnerable);
	}
}

bool AEnemyBase::IsStressTestEnemy() const
{
	return bIsStressTestEnemy;
}

bool AEnemyBase::IsStressTestCombatDisabled() const
{
	return bIsStressTestEnemy && bStressTestDisableCombat;
}

bool AEnemyBase::IsStressTestInvulnerable() const
{
	return bIsStressTestEnemy && bStressTestInvulnerable;
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

void AEnemyBase::ApplySpawnInstanceModifiers(float HealthMultiplier, float DamageMultiplier, float MovementSpeedMultiplier)
{
	const float SafeHealthMultiplier = FMath::IsFinite(HealthMultiplier) ? FMath::Max(0.0f, HealthMultiplier) : 1.0f;
	const float SafeMovementMultiplier = FMath::IsFinite(MovementSpeedMultiplier) ? FMath::Max(0.0f, MovementSpeedMultiplier) : 1.0f;
	if (HealthComponent)
	{
		HealthComponent->SetMaxHealthPreservePercent(HealthComponent->GetMaxHealth() * SafeHealthMultiplier);
	}

	MoveSpeed *= SafeMovementMultiplier;
	if (LightweightMovementComponent)
	{
		LightweightMovementComponent->SetMoveSpeed(MoveSpeed);
	}
}

bool AEnemyBase::ApplyStatusDamage(float DamageAmount, EPlayerAttackSource AttackSource)
{
	TGuardValue<bool> SuppressFlash(bSuppressHitFlash, true);
	return ApplyPlayerDamage(DamageAmount, AttackSource);
}

bool AEnemyBase::ApplyPlayerDamage(float DamageAmount, EPlayerAttackSource AttackSource)
{
	if (!CanReceivePlayerDamage(AttackSource) || !HealthComponent)
	{
		return false;
	}

	const float PreviousHealth = HealthComponent->GetCurrentHealth();
	HealthComponent->ApplyDamage(DamageAmount);
	return HealthComponent->GetCurrentHealth() < PreviousHealth;
}

void AEnemyBase::GetImpactContact(FVector AttackLocation, FVector& Location, FVector& Normal) const
{
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	const FVector Center = Capsule ? Capsule->GetComponentLocation() : GetActorLocation();
	const float Radius = Capsule ? Capsule->GetScaledCapsuleRadius() : 0.0f;
	const float SegmentHalf = Capsule ? FMath::Max(0.0f, Capsule->GetScaledCapsuleHalfHeight() - Radius) : 0.0f;
	const FVector Up = Capsule ? Capsule->GetUpVector() : FVector::UpVector;
	const FVector AxisPoint = Center + Up * FMath::Clamp(FVector::DotProduct(AttackLocation - Center, Up), -SegmentHalf, SegmentHalf);
	Normal = (AttackLocation - AxisPoint).GetSafeNormal();
	if (Normal.IsNearlyZero()) Normal = -GetActorForwardVector();
	Location = AxisPoint + Normal * Radius;
}

void AEnemyBase::ApplyAttackPushback(FVector AttackOrigin, EPlayerAttackSource Source, float Distance, float Duration)
{
	if (Source != EPlayerAttackSource::Samurai || IsDead() || !HealthComponent || HealthComponent->IsDead() || !LightweightMovementComponent) return;
	LightweightMovementComponent->ApplyPushback(GetActorLocation() - AttackOrigin, Distance * FMath::Max(0.0f, PushbackMultiplier), Duration);
}

bool AEnemyBase::CanReceivePlayerDamage(EPlayerAttackSource AttackSource) const
{
	return !bIsDead
		&& (RequiredPlayerAttackSource == EPlayerAttackSource::Other || RequiredPlayerAttackSource == AttackSource);
}

EPlayerAttackSource AEnemyBase::ResolvePlayerAttackSource(const AActor* DamageSourceActor)
{
	if (Cast<ASamuraiCharacter>(DamageSourceActor))
	{
		return EPlayerAttackSource::Samurai;
	}
	if (Cast<ANinjaCharacter>(DamageSourceActor))
	{
		return EPlayerAttackSource::Ninja;
	}
	return EPlayerAttackSource::Other;
}

void AEnemyBase::ConfigureObjectiveEnemy(float MaxHealth, EPlayerAttackSource RequiredSource, UMaterialInterface* OverlayMaterial, FLinearColor OverlayTint)
{
	EndHitFlash();
	RequiredPlayerAttackSource = RequiredSource;
	bDropsXP = false;
	BloodValue = 0;
	if (HealthComponent)
	{
		HealthComponent->SetMaxHealthPreservePercent(FMath::Max(1.0f, MaxHealth));
	}

	NormalOverlayDynamicMaterial = OverlayMaterial ? UMaterialInstanceDynamic::Create(OverlayMaterial, this) : nullptr;
	if (NormalOverlayDynamicMaterial)
	{
		NormalOverlayDynamicMaterial->SetScalarParameterValue(BloodboundMaterialScalarParameterName, 1.0f);
		NormalOverlayDynamicMaterial->SetVectorParameterValue(BloodboundMaterialTintParameterName, OverlayTint);
		NormalOverlayDynamicMaterial->SetScalarParameterValue(BloodboundMaterialEmissiveParameterName, 0.75f);
	}
	if (!bIsDead && GetMesh())
	{
		GetMesh()->SetOverlayMaterial(NormalOverlayDynamicMaterial);
	}
}

void AEnemyBase::SetGameplaySuspended(bool bSuspended)
{
	if (bGameplaySuspended == bSuspended || bIsDead)
	{
		return;
	}

	bGameplaySuspended = bSuspended;
	if (bGameplaySuspended)
	{
		StopEnemyBehavior();
		StopBehaviorUpdates();
		StopSeparationUpdates();
		StopEnemyMovement();
	}
	else
	{
		StartBehaviorUpdates();
		StartSeparationUpdates();
	}
}

void AEnemyBase::MakeBloodbound(float HealthMultiplier, float DamageMultiplier, float MovementSpeedMultiplier, bool bInDropsXP)
{
	if (bIsBloodbound || bIsDead)
	{
		return;
	}
	CapturePreBloodboundState();
	bIsBloodbound = true;
	bDropsXP = bInDropsXP;
	BloodboundHealthMultiplier = FMath::IsFinite(HealthMultiplier) ? FMath::Max(0.0f, HealthMultiplier) : 1.0f;
	BloodboundDamageMultiplier = FMath::IsFinite(DamageMultiplier) ? FMath::Max(0.0f, DamageMultiplier) : 1.0f;
	BloodboundMovementSpeedMultiplier = FMath::IsFinite(MovementSpeedMultiplier) ? FMath::Max(0.0f, MovementSpeedMultiplier) : 1.0f;
	ApplySpawnInstanceModifiers(BloodboundHealthMultiplier, BloodboundDamageMultiplier, BloodboundMovementSpeedMultiplier);
	RefreshEnemyVisualState();
	OnBecameBloodbound.Broadcast(this);
}

bool AEnemyBase::ApplyStatus(EEnemyStatusEffect Status, UPlayerUpgradeComponent* SourceUpgrades, EPlayerAttackSource AttackSource)
{
	return StatusEffectComponent && StatusEffectComponent->ApplyStatus(Status, SourceUpgrades, AttackSource);
}

bool AEnemyBase::HasStatus(EEnemyStatusEffect Status) const
{
	return StatusEffectComponent && StatusEffectComponent->HasStatus(Status);
}

int32 AEnemyBase::GetStatusStacks(EEnemyStatusEffect Status) const
{
	return StatusEffectComponent ? StatusEffectComponent->GetStatusStacks(Status) : 0;
}

void AEnemyBase::InitializeStatusIndicators()
{
	if (BleedStatusWidgetComponent)
	{
		BleedStatusWidgetComponent->SetDrawSize(StatusIndicatorDrawSize);
	}
	if (PoisonStatusWidgetComponent)
	{
		PoisonStatusWidgetComponent->SetDrawSize(StatusIndicatorDrawSize);
	}
	UpdateStatusIndicatorLayout();
}

void AEnemyBase::HandleStatusStacksChanged(EEnemyStatusEffect Status, int32 StackCount)
{
	UWidgetComponent* Component = Status == EEnemyStatusEffect::Bleed ? BleedStatusWidgetComponent.Get() : PoisonStatusWidgetComponent.Get();
	if (!Component) return;
	const bool bActive = StackCount > 0;
	Component->SetHiddenInGame(!bActive);
	Component->SetVisibility(bActive);
	if (bActive)
	{
		if (UEnemyStatusIndicatorWidget* Widget = Cast<UEnemyStatusIndicatorWidget>(Component->GetUserWidgetObject()))
		{
			Widget->SetStatusPresentation(Status, StackCount, bShowStatusStackCountAtOne, StatusStackFontSize,
				Status == EEnemyStatusEffect::Bleed ? BleedStatusIcon.Get() : PoisonStatusIcon.Get());
		}
	}
	UpdateStatusIndicatorLayout();
}

void AEnemyBase::UpdateStatusIndicatorLayout()
{
	if (!HealthBarWidgetComponent) return;

	// Share the bar's projected world anchor. Canvas pivots provide fixed screen-space
	// offsets, so camera rotation/perspective cannot change icon spacing or order.
	const FVector Anchor = HealthBarWidgetComponent->GetComponentLocation();
	const FVector2D BarSize = HealthBarWidgetComponent->GetDrawSize();
	const FVector2D BarPivot = HealthBarWidgetComponent->GetPivot();
	const float Width = FMath::Max(1.0, StatusIndicatorDrawSize.X);
	const float Height = FMath::Max(1.0, StatusIndicatorDrawSize.Y);
	const float HalfSpacing = FMath::Max(0.0f, StatusIndicatorSpacing) * 0.5f;
	const float BarCenterX = (0.5f - BarPivot.X) * BarSize.X;
	const float PivotY = 1.0f + (BarPivot.Y * BarSize.Y + FMath::Max(0.0f, StatusIndicatorHealthBarGap)) / Height;
	if (BleedStatusWidgetComponent)
	{
		BleedStatusWidgetComponent->SetWorldLocation(Anchor);
		BleedStatusWidgetComponent->SetDrawAtDesiredSize(false);
		BleedStatusWidgetComponent->SetDrawSize(FVector2D(Width, Height));
		BleedStatusWidgetComponent->SetPivot(FVector2D(1.0f + (HalfSpacing - BarCenterX) / Width, PivotY));
	}
	if (PoisonStatusWidgetComponent)
	{
		PoisonStatusWidgetComponent->SetWorldLocation(Anchor);
		PoisonStatusWidgetComponent->SetDrawAtDesiredSize(false);
		PoisonStatusWidgetComponent->SetDrawSize(FVector2D(Width, Height));
		PoisonStatusWidgetComponent->SetPivot(FVector2D(-(HalfSpacing + BarCenterX) / Width, PivotY));
	}
}

bool AEnemyBase::RemoveBloodbound()
{
	if (!bIsBloodbound || bIsDead || !bHasPreBloodboundState)
	{
		return false;
	}

	RestorePreBloodboundState();
	bIsBloodbound = false;
	BloodboundHealthMultiplier = 1.0f;
	BloodboundDamageMultiplier = 1.0f;
	BloodboundMovementSpeedMultiplier = 1.0f;
	bHasPreBloodboundState = false;
	RefreshEnemyVisualState();
	return true;
}

void AEnemyBase::CapturePreBloodboundState()
{
	PreBloodboundMaxHealth = HealthComponent ? HealthComponent->GetMaxHealth() : 0.0f;
	PreBloodboundMoveSpeed = MoveSpeed;
	bPreBloodboundDropsXP = bDropsXP;
	EndHitFlash();
	PreBloodboundOverlayMaterial = GetMesh() ? GetMesh()->GetOverlayMaterial() : nullptr;
	bHasPreBloodboundState = true;
}

void AEnemyBase::RestorePreBloodboundState()
{
	if (HealthComponent && PreBloodboundMaxHealth > 0.0f)
	{
		HealthComponent->SetMaxHealthPreservePercent(PreBloodboundMaxHealth);
	}
	MoveSpeed = PreBloodboundMoveSpeed;
	if (LightweightMovementComponent)
	{
		LightweightMovementComponent->SetMoveSpeed(MoveSpeed);
	}
	bDropsXP = bPreBloodboundDropsXP;
}

void AEnemyBase::ActivateBloodboundVisuals()
{
	if (!bIsBloodbound || bIsDead)
	{
		return;
	}

	if (BloodboundNiagaraComponent)
	{
		if (BloodboundNiagaraSystem)
		{
			BloodboundNiagaraComponent->SetAsset(BloodboundNiagaraSystem);
			BloodboundNiagaraComponent->Activate(true);
		}
		else
		{
			BloodboundNiagaraComponent->DeactivateImmediate();
		}
	}

	USkeletalMeshComponent* EnemyMesh = GetMesh();
	if (!EnemyMesh)
	{
		return;
	}

	if (BloodboundOverlayMaterial && !BloodboundOverlayDynamicMaterial)
	{
		BloodboundOverlayDynamicMaterial = UMaterialInstanceDynamic::Create(BloodboundOverlayMaterial, this);
		if (BloodboundOverlayDynamicMaterial)
		{
			BloodboundOverlayDynamicMaterial->SetScalarParameterValue(BloodboundMaterialScalarParameterName, BloodboundMaterialAmount);
			BloodboundOverlayDynamicMaterial->SetVectorParameterValue(BloodboundMaterialTintParameterName, BloodboundTint);
			BloodboundOverlayDynamicMaterial->SetScalarParameterValue(BloodboundMaterialEmissiveParameterName, BloodboundEmissiveStrength);
		}
	}

	if (BloodboundOverlayDynamicMaterial)
	{
		EnemyMesh->SetOverlayMaterial(BloodboundOverlayDynamicMaterial);
	}
}

void AEnemyBase::DeactivateBloodboundVisuals()
{
	if (BloodboundNiagaraComponent)
	{
		BloodboundNiagaraComponent->DeactivateImmediate();
	}

	if (USkeletalMeshComponent* EnemyMesh = GetMesh())
	{
		if (EnemyMesh->GetOverlayMaterial() == BloodboundOverlayDynamicMaterial)
		{
			EnemyMesh->SetOverlayMaterial(PreBloodboundOverlayMaterial);
		}
	}
}

void AEnemyBase::RefreshEnemyVisualState()
{
	EndHitFlash();
	if (bIsDead)
	{
		DeactivateBloodboundVisuals();
		ClearEnemyOverlayMaterials();
		return;
	}

	if (bIsBloodbound)
	{
		ActivateBloodboundVisuals();
	}
	else
	{
		DeactivateBloodboundVisuals();
	}
}

void AEnemyBase::ClearEnemyOverlayMaterials()
{
	TArray<UMeshComponent*> MeshComponents;
	GetComponents(MeshComponents);
	for (UMeshComponent* MeshComponent : MeshComponents)
	{
		if (MeshComponent)
		{
			MeshComponent->SetOverlayMaterial(nullptr);
		}
	}
}

bool AEnemyBase::IsBloodbound() const
{
	return bIsBloodbound;
}

int32 AEnemyBase::GetBloodValue() const
{
	return FMath::Max(0, BloodValue);
}

bool AEnemyBase::ShouldDropXP() const
{
	return bDropsXP;
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
		LightweightMovementComponent && LightweightMovementComponent->IsMovementEnabled() ? TEXT("true") : TEXT("false"),
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
	if (LightweightMovementComponent) LightweightMovementComponent->CancelPushback();
	EndHitFlash();
	if (bIsDead)
	{
		return;
	}

	bIsDead = true;
	CurrentTarget = nullptr;
	StopEnemyBehavior();
	StopBehaviorUpdates();
	StopSeparationUpdates();
	ClearObstaclePath();
	StopEnemyMovement();

	SetActorEnableCollision(false);
	SetActorTickEnabled(false);
	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		AI->StopMovement();
		if (UBrainComponent* Brain = AI->GetBrainComponent()) Brain->StopLogic(TEXT("Enemy died"));
	}
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->DisableMovement();
	}
	if (GetMesh() && GetMesh()->GetAnimInstance()) GetMesh()->GetAnimInstance()->Montage_Stop(0.0f);
	if (StatusEffectComponent) StatusEffectComponent->ClearAllStatuses();
	OnEnemyDied.Broadcast(this);
	if (bIsBloodbound)
	{
		bIsBloodbound = false;
	}
	RefreshEnemyVisualState();
	if (UWorld* World = GetWorld())
	{
		if (UHealingPickupDropSubsystem* DropSubsystem = World->GetSubsystem<UHealingPickupDropSubsystem>())
		{
			DropSubsystem->NotifyEnemyDied(this);
		}
	}
	ClearMark();
	HideMarkIndicator();
	HideHealthBar();
	SpawnExperiencePickup();
	UpdateAnimationBudgetSignificance();
	OnEnemyDeath();

	if (IsValid(this) && !IsActorBeingDestroyed()) BeginDeathPresentation();
}

void AEnemyBase::HandleHealthChanged(float CurrentHealth, float MaxHealth, float HealthPercent)
{
	if (CurrentHealth <= 0.0f && !bIsDead)
	{
		UE_LOG(LogTemp, Warning, TEXT("[EnemyDamage] EnemyBase death state sync repaired for %s. Health reached %.3f but EnemyDead was false."),
			*GetNameSafe(this),
			CurrentHealth);
		HandleDeath();
		return;
	}

	UpdateHealthBarVisibility(HealthPercent);
}

void AEnemyBase::BeginDeathPresentation_Implementation()
{
	if (DropCategory == EEnemyDropCategory::Boss)
	{
		if (BossDeathCleanupDelay > 0.0f) SetLifeSpan(BossDeathCleanupDelay);
		else DestroyAfterDeath();
		return;
	}
	EnemyDeathComponent->StartDeathPresentation(FSimpleDelegate::CreateUObject(this, &AEnemyBase::DestroyAfterDeath));
}

void AEnemyBase::DestroyAfterDeath()
{
	if (bIsDead) Destroy();
}

void AEnemyBase::HandlePlayerCharacterSwapped(ACharacterBase* OldCharacter, ACharacterBase* NewCharacter)
{
	SetTarget(NewCharacter);
	ClearObstaclePath();
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

	if (!CachedSurvivorController)
	{
		CachedSurvivorController = Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	}

	if (!CachedSurvivorController)
	{
		return false;
	}

	ObservedCharacterManager = CachedSurvivorController->GetCharacterManager();
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

	return true;
}

void AEnemyBase::CachePlayerExperienceComponent()
{
	if (CachedPlayerExperienceComponent)
	{
		return;
	}

	if (!CachedSurvivorController)
	{
		CachedSurvivorController = Cast<ASurvivorPlayerController>(ObservedCharacterManager ? ObservedCharacterManager->GetOwner() : nullptr);
	}
	if (!CachedSurvivorController)
	{
		CachedSurvivorController = Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	}

	CachedPlayerExperienceComponent = CachedSurvivorController ? CachedSurvivorController->GetExperienceComponent() : nullptr;
}

void AEnemyBase::SpawnExperiencePickup()
{
	if (bExperiencePickupSpawned || XPReward <= 0 || !ShouldDropXP())
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

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const int32 XPPerPickup = FMath::Max(1, ExperiencePerPickup);
	const int32 PickupCount = FMath::DivideAndRoundUp(XPReward, XPPerPickup);
	const float ScatterRadius = PickupCount > 1
		? FMath::Max(0.0f, MultipleExperiencePickupScatterRadius)
		: FMath::Max(0.0f, ExperiencePickupSpawnScatterRadius);
	int32 RemainingXP = XPReward;
	for (int32 PickupIndex = 0; PickupIndex < PickupCount; ++PickupIndex)
	{
		FVector SpawnLocation = GetActorLocation();
		if (ScatterRadius > 0.0f)
		{
			const FVector2D RandomOffset = FMath::RandPointInCircle(ScatterRadius);
			SpawnLocation.X += RandomOffset.X;
			SpawnLocation.Y += RandomOffset.Y;
		}

		AExperiencePickup* Pickup = GetWorld()->SpawnActor<AExperiencePickup>(
			ExperiencePickupClass,
			SpawnLocation,
			FRotator::ZeroRotator,
			SpawnParameters);

		if (!Pickup)
		{
			UE_LOG(LogTemp, Warning, TEXT("Enemy %s failed to spawn XP pickup %d/%d using class %s."),
				*GetNameSafe(this), PickupIndex + 1, PickupCount, *GetNameSafe(ExperiencePickupClass.Get()));
			continue;
		}

		const int32 PickupXP = FMath::Min(XPPerPickup, RemainingXP);
		RemainingXP -= PickupXP;
		Pickup->InitializePickup(PickupXP, CachedPlayerExperienceComponent, ObservedCharacterManager);
	}
}

void AEnemyBase::InitializeHealthBar()
{
	if (!ShouldUseWorldHealthBar())
	{
		HideHealthBar();
		return;
	}
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
	if (!ShouldUseWorldHealthBar())
	{
		HideHealthBar();
		return;
	}
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
	QUICK_SCOPE_CYCLE_COUNTER(STAT_EnemyBase_BehaviorUpdate);
	UpdateAnimationProfilingState();
	UpdateAnimationBudgetSignificance();

	UpdateEnemyBehavior(BehaviorUpdateInterval);
}

void AEnemyBase::StartSeparationUpdates()
{
	if (!GetWorld() || bIsDead || !bUseEnemySeparation || SeparationRadius <= 0.0f || SeparationUpdateInterval <= 0.0f)
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
	QUICK_SCOPE_CYCLE_COUNTER(STAT_EnemyBase_SeparationUpdate);

	if (bIsDead || !bUseEnemySeparation)
	{
		CachedEnemySeparationVector = FVector::ZeroVector;
		return;
	}

	UpdateCachedEnemySeparation();
}

void AEnemyBase::ApplyDesiredMovementInput(float DeltaSeconds)
{
	if (!bIsDead && bHasDesiredMovementDirection)
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
		LightweightMovementComponent->SetMovementEnabled(true);
	}

	DisableNativeCharacterMovement();

	if (UCapsuleComponent* EnemyCapsule = GetCapsuleComponent())
	{
		ConfigureEnemyCapsuleCollisionDefaults();
	}

	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MeshComponent->SetGenerateOverlapEvents(false);
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

void AEnemyBase::SnapToGroundBeforeLightweightMovement()
{
	UWorld* World = GetWorld();
	UCapsuleComponent* EnemyCapsule = GetCapsuleComponent();
	if (!World || !EnemyCapsule)
	{
		if (LightweightMovementComponent)
		{
			LightweightMovementComponent->RefreshSpawnZ();
		}
		return;
	}

	const FVector OriginalLocation = GetActorLocation();
	const float CapsuleHalfHeight = EnemyCapsule->GetScaledCapsuleHalfHeight();
	const FVector TraceStart = OriginalLocation + FVector(0.0f, 0.0f, 150.0f);
	const FVector TraceEnd = OriginalLocation - FVector(0.0f, 0.0f, FMath::Max(500.0f, CapsuleHalfHeight + 1000.0f));

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemyBeginPlayGroundSnap), false, this);
	QueryParams.AddIgnoredActor(this);

	TArray<FHitResult> HitResults;
	World->LineTraceMultiByChannel(HitResults, TraceStart, TraceEnd, EnemyGroundSnapTraceChannel, QueryParams);

	for (const FHitResult& HitResult : HitResults)
	{
		if (!IsValidEnemyGroundHit(HitResult))
		{
			continue;
		}

		const FVector CorrectedLocation(OriginalLocation.X, OriginalLocation.Y, HitResult.Location.Z + CapsuleHalfHeight);
		const float DeltaZ = CorrectedLocation.Z - OriginalLocation.Z;
		if (!FMath::IsNearlyZero(DeltaZ, 1.0f))
		{
			SetActorLocation(CorrectedLocation, false, nullptr, ETeleportType::TeleportPhysics);
			if (FMath::Abs(DeltaZ) > 25.0f && CVarDebugEnemyGroundSnap.GetValueOnGameThread() != 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("Enemy ground snap adjusted %s OriginalZ=%.2f CorrectedZ=%.2f DeltaZ=%.2f HitActor=%s HitComponent=%s ImpactNormal=%s"),
					*GetNameSafe(this),
					OriginalLocation.Z,
					CorrectedLocation.Z,
					DeltaZ,
					*GetNameSafe(HitResult.GetActor()),
					*GetNameSafe(HitResult.GetComponent()),
					*HitResult.ImpactNormal.ToString());
			}
		}
		break;
	}

	if (LightweightMovementComponent)
	{
		LightweightMovementComponent->RefreshSpawnZ();
	}
}

void AEnemyBase::InitializeCrowdSpread()
{
	const uint32 Seed = GetTypeHash(GetFName()) ^ GetUniqueID();
	const FRandomStream RandomStream(Seed);
	const float BiasSign = RandomStream.FRand() < 0.5f ? -1.0f : 1.0f;
	CrowdSpreadBias = BiasSign * RandomStream.FRandRange(0.35f, 1.0f);
}

void AEnemyBase::UpdateObstaclePathFallback()
{
	UWorld* World = GetWorld();
	if (!World || bIsDead || !CurrentTarget || !LightweightMovementComponent)
	{
		ClearObstaclePath();
		return;
	}

	const float CurrentTime = World->GetTimeSeconds();
	const bool bBlockedByWorldGeometry = LightweightMovementComponent->WasLastMoveBlockedByWorldGeometry();
	const float DistanceToTarget = FVector::Dist2D(GetActorLocation(), CurrentTarget->GetActorLocation());

	if (!bBlockedByWorldGeometry)
	{
		BlockedByWorldGeometryStartTime = -1.0f;
		return;
	}

	if (DistanceToTarget < FMath::Max(StopDistance, MinPathFallbackTargetDistance))
	{
		ClearObstaclePath();
		return;
	}

	if (BlockedByWorldGeometryStartTime < 0.0f)
	{
		BlockedByWorldGeometryStartTime = CurrentTime;
	}

	const bool bHasPath = ObstaclePathPoints.Num() > 1 && CurrentObstaclePathIndex != INDEX_NONE;
	const bool bTargetMovedEnoughForRepath = bHasPath
		&& PathTargetRepathDistance > 0.0f
		&& FVector::DistSquared2D(LastPathTargetLocation, CurrentTarget->GetActorLocation()) >= FMath::Square(PathTargetRepathDistance);
	const bool bBlockedLongEnough = CurrentTime - BlockedByWorldGeometryStartTime >= PathFallbackBlockedTime + PathFallbackRequestJitter;

	if (bBlockedLongEnough && (!bHasPath || bTargetMovedEnoughForRepath) && CurrentTime >= NextPathFallbackRequestTime)
	{
		if (!TryBuildObstaclePath() && CVarDebugEnemyPathFallback.GetValueOnGameThread() != 0)
		{
			const FVector DebugStart = GetActorLocation() + FVector(0.0f, 0.0f, 80.0f);
			DrawDebugSphere(World, DebugStart, 35.0f, 12, FColor::Yellow, false, BehaviorUpdateInterval, 0, 2.0f);
		}
	}
	else if (!bHasPath && CVarDebugEnemyPathFallback.GetValueOnGameThread() != 0)
	{
		const FVector DebugStart = GetActorLocation() + FVector(0.0f, 0.0f, 80.0f);
		DrawDebugLine(World, DebugStart, DebugStart + FVector::UpVector * 80.0f, FColor::Orange, false, BehaviorUpdateInterval, 0, 2.0f);
	}
}

bool AEnemyBase::TryBuildObstaclePath()
{
	UWorld* World = GetWorld();
	if (!World || !CurrentTarget)
	{
		ClearObstaclePath();
		return false;
	}

	NextPathFallbackRequestTime = World->GetTimeSeconds() + FMath::Max(0.1f, PathFallbackRepathInterval) + PathFallbackRequestJitter;

	if (!IsEnemyPathFallbackRequestAllowed(World))
	{
		if (CVarDebugEnemyPathFallback.GetValueOnGameThread() != 0)
		{
			DrawDebugSphere(World, GetActorLocation() + FVector(0.0f, 0.0f, 95.0f), 45.0f, 12, FColor::Orange, false, BehaviorUpdateInterval, 0, 2.0f);
		}
		return false;
	}

	const UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavigationSystem)
	{
		ClearObstaclePath();
		return false;
	}

	FVector ProjectedStart;
	FVector ProjectedEnd;
	if (!ProjectPathFallbackLocation(GetActorLocation(), ProjectedStart) || !ProjectPathFallbackLocation(CurrentTarget->GetActorLocation(), ProjectedEnd))
	{
		ClearObstaclePath();
		return false;
	}

	UNavigationPath* NavigationPath = UNavigationSystemV1::FindPathToLocationSynchronously(this, ProjectedStart, ProjectedEnd, this);
	if (!NavigationPath || !NavigationPath->IsValid() || NavigationPath->PathPoints.Num() < 2)
	{
		ClearObstaclePath();
		if (CVarDebugEnemyPathFallback.GetValueOnGameThread() != 0)
		{
			DrawDebugSphere(World, ProjectedStart, 40.0f, 12, FColor::Red, false, PathFallbackRepathInterval, 0, 2.0f);
			DrawDebugSphere(World, ProjectedEnd, 40.0f, 12, FColor::Red, false, PathFallbackRepathInterval, 0, 2.0f);
		}
		return false;
	}

	ObstaclePathPoints = NavigationPath->PathPoints;
	CurrentObstaclePathIndex = ObstaclePathPoints.Num() > 1 ? 1 : INDEX_NONE;
	LastPathTargetLocation = CurrentTarget->GetActorLocation();
	NextDirectPathCheckTime = 0.0f;

	if (CVarDebugEnemyPathFallback.GetValueOnGameThread() != 0)
	{
		for (int32 Index = 1; Index < ObstaclePathPoints.Num(); ++Index)
		{
			DrawDebugLine(World, ObstaclePathPoints[Index - 1], ObstaclePathPoints[Index], FColor::Purple, false, PathFallbackRepathInterval, 0, 3.0f);
		}
		DrawDebugSphere(World, ObstaclePathPoints[CurrentObstaclePathIndex], 45.0f, 12, FColor::Cyan, false, PathFallbackRepathInterval, 0, 2.0f);
	}

	return true;
}

bool AEnemyBase::TryGetPathFallbackSteeringDirection(FVector& OutSteeringDirection)
{
	UWorld* World = GetWorld();
	if (!World || !CurrentTarget || ObstaclePathPoints.Num() < 2 || CurrentObstaclePathIndex == INDEX_NONE)
	{
		return false;
	}

	const float CurrentTime = World->GetTimeSeconds();
	if (CurrentTime >= NextDirectPathCheckTime)
	{
		NextDirectPathCheckTime = CurrentTime + FMath::Max(0.05f, DirectPathCheckInterval);
		if (IsDirectPathToTargetClear())
		{
			ClearObstaclePath();
			return false;
		}
	}

	const FVector CurrentLocation = GetActorLocation();
	while (ObstaclePathPoints.IsValidIndex(CurrentObstaclePathIndex)
		&& FVector::DistSquared2D(CurrentLocation, ObstaclePathPoints[CurrentObstaclePathIndex]) <= FMath::Square(PathWaypointAcceptanceRadius))
	{
		++CurrentObstaclePathIndex;
	}

	if (!ObstaclePathPoints.IsValidIndex(CurrentObstaclePathIndex))
	{
		ClearObstaclePath();
		return false;
	}

	int32 SteeringIndex = CurrentObstaclePathIndex;
	const int32 FurthestLookAheadIndex = FMath::Min(CurrentObstaclePathIndex + 3, ObstaclePathPoints.Num() - 1);
	for (int32 CandidateIndex = FurthestLookAheadIndex; CandidateIndex > CurrentObstaclePathIndex; --CandidateIndex)
	{
		if (IsPathFallbackRouteClearToLocation(ObstaclePathPoints[CandidateIndex]))
		{
			SteeringIndex = CandidateIndex;
			break;
		}
	}

	FVector ToWaypoint = ObstaclePathPoints[SteeringIndex] - CurrentLocation;
	ToWaypoint.Z = 0.0f;
	if (!ToWaypoint.Normalize())
	{
		return false;
	}

	OutSteeringDirection = ToWaypoint;

	if (CVarDebugEnemyPathFallback.GetValueOnGameThread() != 0)
	{
		const FVector DebugStart = CurrentLocation + FVector(0.0f, 0.0f, 45.0f);
		DrawDebugLine(World, DebugStart, DebugStart + OutSteeringDirection * 220.0f, FColor::Cyan, false, 0.15f, 0, 3.0f);
		DrawDebugSphere(World, ObstaclePathPoints[SteeringIndex], 35.0f, 12, FColor::Cyan, false, 0.15f, 0, 2.0f);
	}

	return true;
}

bool AEnemyBase::IsDirectPathToTargetClear() const
{
	return CurrentTarget && IsPathFallbackRouteClearToLocation(CurrentTarget->GetActorLocation());
}

bool AEnemyBase::IsPathFallbackRouteClearToLocation(const FVector& Location) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	float CapsuleRadius = 34.0f;
	float CapsuleHalfHeight = 88.0f;
	if (const UCapsuleComponent* EnemyCapsule = GetCapsuleComponent())
	{
		CapsuleRadius = EnemyCapsule->GetScaledCapsuleRadius();
		CapsuleHalfHeight = EnemyCapsule->GetScaledCapsuleHalfHeight();
	}

	TArray<FHitResult> Hits;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemyPathFallbackDirectCheck), false, this);
	QueryParams.AddIgnoredActor(this);
	if (CurrentTarget)
	{
		QueryParams.AddIgnoredActor(CurrentTarget);
	}

	const FVector StartLocation = GetActorLocation();
	FVector EndLocation = Location;
	EndLocation.Z = StartLocation.Z;
	const bool bHasHits = World->SweepMultiByChannel(
		Hits,
		StartLocation,
		EndLocation,
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight),
		QueryParams);

	if (!bHasHits)
	{
		return true;
	}

	for (const FHitResult& Hit : Hits)
	{
		if (IsBlockingWorldGeometryHit(Hit))
		{
			return false;
		}
	}

	return true;
}

bool AEnemyBase::ProjectPathFallbackLocation(const FVector& Location, FVector& OutProjectedLocation) const
{
	const UWorld* World = GetWorld();
	const UNavigationSystemV1* NavigationSystem = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
	if (!NavigationSystem)
	{
		return false;
	}

	FNavLocation NavLocation;
	if (!NavigationSystem->ProjectPointToNavigation(Location, NavLocation, PathFallbackProjectionExtent))
	{
		return false;
	}

	OutProjectedLocation = NavLocation.Location;
	return true;
}

void AEnemyBase::ClearObstaclePath()
{
	ObstaclePathPoints.Reset();
	CurrentObstaclePathIndex = INDEX_NONE;
	LastPathTargetLocation = FVector::ZeroVector;
	NextDirectPathCheckTime = 0.0f;
	BlockedByWorldGeometryStartTime = -1.0f;
}

FVector AEnemyBase::ApplyCrowdSpreadToDirection(const FVector& DirectDirection) const
{
	if (!bUseCrowdSpread || CrowdSpreadStrength <= 0.0f)
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
	if (!bUseEnemySeparation || SeparationStrength <= 0.0f || CachedEnemySeparationVector.IsNearlyZero())
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

void AEnemyBase::UpdateCachedEnemySeparation()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_EnemyBase_UpdateCachedEnemySeparation);
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
	if (!LightweightMovementComponent)
	{
		ensureMsgf(false, TEXT("Enemy %s is missing required LightweightMovementComponent."), *GetNameSafe(this));
		return;
	}

	LightweightMovementComponent->RequestMove(WorldDirection);
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
	if (LightweightMovementComponent)
	{
		return LightweightMovementComponent->GetCurrentVelocity();
	}

	return FVector::ZeroVector;
}

void AEnemyBase::DisableNativeCharacterMovement()
{
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
		MovementComponent->DisableMovement();
		MovementComponent->SetComponentTickEnabled(false);
	}
}

bool AEnemyBase::IsEnemyAnimationProfilingDisabled() const
{
	return bCachedAnimationProfilingDisabled;
}

void AEnemyBase::UpdateAnimationProfilingState()
{
	const bool bShouldDisableAnimation = CVarDisableEnemyAnimationForProfiling.GetValueOnGameThread() != 0;
	bCachedAnimationProfilingDisabled = bShouldDisableAnimation;
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
	if (ToTarget.SizeSquared2D() <= FMath::Square(GetChaseStopDistance()))
	{
		StopEnemyMovement();
		return;
	}

	if (ToTarget.Normalize())
	{
		DesiredDirectMovementDirection = ToTarget;
		UpdateObstaclePathFallback();

		FVector SteeringDirection = ToTarget;
		FVector PathSteeringDirection;
		const bool bUsingPathFallback = TryGetPathFallbackSteeringDirection(PathSteeringDirection);
		if (bUsingPathFallback)
		{
			SteeringDirection = PathSteeringDirection;
		}

		DesiredMovementDirection = ApplyEnemySeparationToDirection(ApplyCrowdSpreadToDirection(SteeringDirection));
		if (bUsingPathFallback)
		{
			DesiredMovementDirection = (PathSteeringDirection * 0.8f + DesiredMovementDirection * 0.2f).GetSafeNormal2D();
		}
		bHasDesiredMovementDirection = true;

		if (bDebugCrowdSpread && GetWorld())
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
	return CachedSurvivorController && CachedSurvivorController->IsPlayerDead();
}
