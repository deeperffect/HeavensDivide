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
	if (StatusEffectComponent)
	{
		StatusEffectComponent->TransferBleedOnDeath();
		StatusEffectComponent->ClearAllStatuses();
	}
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

void AEnemyBase::UpdateAnimationProfilingState()
{
	const bool bShouldDisableAnimation = CVarDisableEnemyAnimationForProfiling.GetValueOnGameThread() != 0;
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

bool AEnemyBase::IsPlayerTargetDead() const
{
	return CachedSurvivorController && CachedSurvivorController->IsPlayerDead();
}
