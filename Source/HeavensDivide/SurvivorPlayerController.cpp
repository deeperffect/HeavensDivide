// Copyright Epic Games, Inc. All Rights Reserved.

#include "SurvivorPlayerController.h"

#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "ExperienceComponent.h"
#include "HealthComponent.h"
#include "InactiveCharacterAssistComponent.h"
#include "InputActionValue.h"
#include "LevelUpWidget.h"
#include "PlayerCameraRig.h"
#include "PlayerHUDWidget.h"
#include "PlayerUpgradeComponent.h"
#include "NinjaCharacter.h"
#include "SamuraiCharacter.h"
#include "SharedPlayerStatsComponent.h"
#include "AutoAttackComponent.h"
#include "CharacterStatsComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

static TAutoConsoleVariable<int32> CVarHDLogDash(
	TEXT("hd.LogDash"),
	0,
	TEXT("Logs player dash start, block, and end events when enabled."));

static TAutoConsoleVariable<int32> CVarHDLogDashCharges(
	TEXT("hd.LogDashCharges"),
	0,
	TEXT("Logs player dash charge state changes when enabled."));

namespace
{
FString UpgradeCategoryToLogString(EUpgradeCategory Category)
{
	switch (Category)
	{
	case EUpgradeCategory::Samurai:
		return TEXT("Samurai");
	case EUpgradeCategory::Ninja:
		return TEXT("Ninja");
	case EUpgradeCategory::Global:
		return TEXT("Global");
	case EUpgradeCategory::Synergy:
		return TEXT("Synergy");
	default:
		return TEXT("Unknown");
	}
}
}

ASurvivorPlayerController::ASurvivorPlayerController()
{
	bAutoManageActiveCameraTarget = false;
	CharacterManager = CreateDefaultSubobject<UCharacterManagerComponent>(TEXT("CharacterManager"));
	PlayerHealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("PlayerHealthComponent"));
	ExperienceComponent = CreateDefaultSubobject<UExperienceComponent>(TEXT("ExperienceComponent"));
	SharedPlayerStatsComponent = CreateDefaultSubobject<USharedPlayerStatsComponent>(TEXT("SharedPlayerStatsComponent"));
	PlayerUpgradeComponent = CreateDefaultSubobject<UPlayerUpgradeComponent>(TEXT("PlayerUpgradeComponent"));
	InactiveCharacterAssistComponent = CreateDefaultSubobject<UInactiveCharacterAssistComponent>(TEXT("InactiveCharacterAssistComponent"));
}

void ASurvivorPlayerController::BeginPlay()
{
	Super::BeginPlay();

	ConfigureInputMode();
	if (CharacterManager)
	{
		CharacterManager->InitializeParty();
		if (PlayerUpgradeComponent)
		{
			PlayerUpgradeComponent->RebuildAllUpgradeModifiers();
		}
		CharacterManager->OnCharacterSwapped.AddDynamic(this, &ASurvivorPlayerController::HandleCharacterSwapped);
	}
	InitializePlayerCameraRig();
	InitializePlayerHUD();

	if (PlayerHealthComponent)
	{
		PlayerHealthComponent->OnDeath.AddDynamic(this, &ASurvivorPlayerController::HandlePlayerDeath);
		BasePlayerMaxHealth = PlayerHealthComponent->GetMaxHealth();
	}
	StartHPRegeneration();

	if (ExperienceComponent)
	{
		ExperienceComponent->OnLevelUp.AddDynamic(this, &ASurvivorPlayerController::HandlePlayerLevelUp);
	}

	if (SharedPlayerStatsComponent)
	{
		SharedPlayerStatsComponent->OnStatsChanged.AddDynamic(this, &ASurvivorPlayerController::HandleSharedPlayerStatsChanged);
		ApplySharedPlayerStats();
	}
	ApplyDashChargeStats();

	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (InputSubsystem && DefaultMappingContext)
	{
		InputSubsystem->AddMappingContext(DefaultMappingContext, 0);
	}
}

void ASurvivorPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	UpdateMouseFacingTarget();
	HandleDashStep(DeltaTime);
}

void ASurvivorPlayerController::SetCameraFollowTarget(ACharacterBase* NewFollowTarget)
{
	if (PlayerCameraRig)
	{
		PlayerCameraRig->SetFollowTarget(NewFollowTarget);
		SetViewTarget(PlayerCameraRig);
	}
}

ACharacterBase* ASurvivorPlayerController::GetCameraFollowTarget() const
{
	return PlayerCameraRig ? PlayerCameraRig->GetFollowTarget() : nullptr;
}

UCharacterManagerComponent* ASurvivorPlayerController::GetCharacterManager() const
{
	return CharacterManager;
}

UHealthComponent* ASurvivorPlayerController::GetPlayerHealthComponent() const
{
	return PlayerHealthComponent;
}

UExperienceComponent* ASurvivorPlayerController::GetExperienceComponent() const
{
	return ExperienceComponent;
}

USharedPlayerStatsComponent* ASurvivorPlayerController::GetSharedPlayerStats() const
{
	return SharedPlayerStatsComponent;
}

UPlayerUpgradeComponent* ASurvivorPlayerController::GetPlayerUpgrades() const
{
	return PlayerUpgradeComponent;
}

bool ASurvivorPlayerController::IsPlayerDead() const
{
	return bIsPlayerDead;
}

bool ASurvivorPlayerController::CanSwap() const
{
	return !bIsPlayerDead
		&& !bIsDashing
		&& !bLevelUpSelectionActive
		&& bCanSwap
		&& CharacterManager
		&& CharacterManager->GetActiveCharacter()
		&& CharacterManager->GetInactiveCharacter();
}

float ASurvivorPlayerController::GetSwapCooldownRemaining() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}

	return FMath::Max(0.0f, World->GetTimerManager().GetTimerRemaining(SwapCooldownTimerHandle));
}

float ASurvivorPlayerController::GetSwapCooldownDuration() const
{
	return FMath::Max(0.0f, SwapCooldown);
}

float ASurvivorPlayerController::GetSwapCooldownProgress() const
{
	const float CooldownDuration = GetSwapCooldownDuration();
	if (CooldownDuration <= KINDA_SMALL_NUMBER || CanSwap())
	{
		return 0.0f;
	}

	return FMath::Clamp(GetSwapCooldownRemaining() / CooldownDuration, 0.0f, 1.0f);
}

void ASurvivorPlayerController::ResetSwapCooldown()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SwapCooldownTimerHandle);
	}

	const bool bWasOnCooldown = !bCanSwap;
	bCanSwap = true;
	if (bWasOnCooldown)
	{
		OnSwapCooldownFinished.Broadcast();
	}
}

bool ASurvivorPlayerController::TryDash()
{
	if (!CanDash())
	{
		if (CVarHDLogDash.GetValueOnGameThread() != 0)
		{
			UE_LOG(LogTemp, Log, TEXT("Dash blocked - charges=%d/%d recharge remaining=%.2f bIsDashing=%s"),
				CurrentDashCharges,
				MaxDashCharges,
				GetDashRechargeRemaining(),
				bIsDashing ? TEXT("true") : TEXT("false"));
		}
		return false;
	}

	ACharacterBase* ActiveCharacter = CharacterManager ? CharacterManager->GetActiveCharacter() : nullptr;
	if (!ActiveCharacter)
	{
		return false;
	}

	ActiveDashDirection = GetDashDirection(ActiveCharacter);
	if (ActiveDashDirection.IsNearlyZero())
	{
		return false;
	}

	DashElapsedTime = 0.0f;
	bIsDashing = true;
	ConsumeDashCharge();
	if (UCharacterMovementComponent* MovementComponent = ActiveCharacter->GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
	}
	ActiveCharacter->StartDashVisual(DashDuration, ActiveDashDirection);
	OnDashStarted.Broadcast();

	if (CVarHDLogDash.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogTemp, Log, TEXT("Dash started Direction=%s Distance=%.1f Duration=%.2f RechargeTime=%.2f"),
			*ActiveDashDirection.ToString(),
			DashDistance,
			DashDuration,
			DashRechargeTime);
	}

	return true;
}

bool ASurvivorPlayerController::CanDash() const
{
	if (bIsPlayerDead || bIsDashing || bLevelUpSelectionActive || !GetWorld())
	{
		return false;
	}

	const ACharacterBase* ActiveCharacter = CharacterManager ? CharacterManager->GetActiveCharacter() : nullptr;
	return ActiveCharacter
		&& ActiveCharacter->GetCharacterMode() == ECharacterMode::Active
		&& HasDashCharge();
}

float ASurvivorPlayerController::GetDashCooldownRemaining() const
{
	return HasDashCharge() ? 0.0f : GetDashRechargeRemaining();
}

int32 ASurvivorPlayerController::GetCurrentDashCharges() const
{
	return CurrentDashCharges;
}

int32 ASurvivorPlayerController::GetMaxDashCharges() const
{
	return MaxDashCharges;
}

bool ASurvivorPlayerController::HasDashCharge() const
{
	return CurrentDashCharges > 0;
}

float ASurvivorPlayerController::GetDashRechargeRemaining() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}

	return FMath::Max(0.0f, World->GetTimerManager().GetTimerRemaining(DashRechargeTimerHandle));
}

float ASurvivorPlayerController::GetDashRechargeNormalized() const
{
	const UWorld* World = GetWorld();
	if (!World || DashRechargeTime <= KINDA_SMALL_NUMBER || !World->GetTimerManager().IsTimerActive(DashRechargeTimerHandle))
	{
		return CurrentDashCharges >= MaxDashCharges ? 1.0f : 0.0f;
	}

	return 1.0f - FMath::Clamp(GetDashRechargeRemaining() / DashRechargeTime, 0.0f, 1.0f);
}

void ASurvivorPlayerController::DebugGrantXP(int32 Amount)
{
#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("DebugGrantXP: Amount=%d"), Amount);
	if (ExperienceComponent)
	{
		ExperienceComponent->AddXP(Amount);
	}
#else
	UE_LOG(LogTemp, Warning, TEXT("DebugGrantXP is disabled in shipping builds."));
#endif
}

void ASurvivorPlayerController::ApplyDamageToPlayer(float DamageAmount)
{
	if (!PlayerHealthComponent || PlayerHealthComponent->IsDead())
	{
		return;
	}

	const float DodgeChance = GetActiveDodgeChance();
	if (DodgeChance > 0.0f && FMath::FRand() < DodgeChance)
	{
		OnDamageDodged.Broadcast(DamageAmount);
		return;
	}

	const float DamageReduction = GetActiveDamageReduction();
	const float FinalDamage = DamageAmount * (1.0f - DamageReduction);
	PlayerHealthComponent->ApplyDamage(FinalDamage);
}

float ASurvivorPlayerController::GetActiveDamageReduction() const
{
	const ASamuraiCharacter* ActiveSamurai = CharacterManager ? Cast<ASamuraiCharacter>(CharacterManager->GetActiveCharacter()) : nullptr;
	const UCharacterStatsComponent* SamuraiStats = ActiveSamurai ? ActiveSamurai->GetCharacterStats() : nullptr;
	return SamuraiStats ? SamuraiStats->GetFinalDamageReduction() : 0.0f;
}

float ASurvivorPlayerController::GetActiveHPRegenPerSecond() const
{
	const ASamuraiCharacter* ActiveSamurai = CharacterManager ? Cast<ASamuraiCharacter>(CharacterManager->GetActiveCharacter()) : nullptr;
	const UCharacterStatsComponent* SamuraiStats = ActiveSamurai ? ActiveSamurai->GetCharacterStats() : nullptr;
	return SamuraiStats ? SamuraiStats->GetFinalHPRegenPerSecond() : 0.0f;
}

float ASurvivorPlayerController::GetActiveDodgeChance() const
{
	const ANinjaCharacter* ActiveNinja = CharacterManager ? Cast<ANinjaCharacter>(CharacterManager->GetActiveCharacter()) : nullptr;
	const UCharacterStatsComponent* NinjaStats = ActiveNinja ? ActiveNinja->GetCharacterStats() : nullptr;
	return NinjaStats ? NinjaStats->GetFinalDodgeChance() : 0.0f;
}

void ASurvivorPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);
	if (EnhancedInputComponent && MoveAction)
	{
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ASurvivorPlayerController::Move);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &ASurvivorPlayerController::StopMoveInput);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Canceled, this, &ASurvivorPlayerController::StopMoveInput);
	}

	if (EnhancedInputComponent && SwapAction)
	{
		EnhancedInputComponent->BindAction(SwapAction, ETriggerEvent::Started, this, &ASurvivorPlayerController::Swap);
	}

	if (EnhancedInputComponent && DashAction)
	{
		EnhancedInputComponent->BindAction(DashAction, ETriggerEvent::Started, this, &ASurvivorPlayerController::Dash);
	}
}

void ASurvivorPlayerController::Move(const FInputActionValue& Value)
{
	if (bIsPlayerDead)
	{
		return;
	}

	ACharacterBase* ControlledCharacter = CharacterManager ? CharacterManager->GetActiveCharacter() : nullptr;
	if (!ControlledCharacter)
	{
		return;
	}

	LastMovementInput = Value.Get<FVector2D>();
	if (!bIsDashing)
	{
		ControlledCharacter->MoveCharacter(LastMovementInput);
	}
}

void ASurvivorPlayerController::StopMoveInput(const FInputActionValue& Value)
{
	LastMovementInput = FVector2D::ZeroVector;
}

void ASurvivorPlayerController::Swap(const FInputActionValue& Value)
{
	if (!CanSwap())
	{
		UE_LOG(LogTemp, Log, TEXT("Swapping Disabled"));
		return;
	}

	if (CharacterManager)
	{
		CharacterManager->SwapCharacter();
	}
}

void ASurvivorPlayerController::Dash(const FInputActionValue& Value)
{
	TryDash();
}

void ASurvivorPlayerController::ConfigureInputMode()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
}

void ASurvivorPlayerController::InitializePlayerCameraRig()
{
	if (PlayerCameraRig || !GetWorld())
	{
		return;
	}

	const TSubclassOf<APlayerCameraRig> CameraRigClass = PlayerCameraRigClass ? PlayerCameraRigClass : TSubclassOf<APlayerCameraRig>(APlayerCameraRig::StaticClass());
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const ACharacterBase* ActiveCharacter = CharacterManager ? CharacterManager->GetActiveCharacter() : nullptr;
	const FVector CameraRigLocation = ActiveCharacter ? ActiveCharacter->GetActorLocation() : GetFocalLocation();
	PlayerCameraRig = GetWorld()->SpawnActor<APlayerCameraRig>(CameraRigClass, CameraRigLocation, FRotator::ZeroRotator, SpawnParameters);

	if (PlayerCameraRig)
	{
		PlayerCameraRig->SetFollowTarget(CharacterManager ? CharacterManager->GetActiveCharacter() : nullptr);
		SetViewTarget(PlayerCameraRig);
	}
}

void ASurvivorPlayerController::InitializePlayerHUD()
{
	if (PlayerHUDWidget || !PlayerHUDClass)
	{
		return;
	}

	PlayerHUDWidget = CreateWidget<UPlayerHUDWidget>(this, PlayerHUDClass);
	if (!PlayerHUDWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("SurvivorPlayerController: failed to create PlayerHUDWidget."));
		return;
	}

	PlayerHUDWidget->AddToViewport();
	PlayerHUDWidget->InitializeFromPlayerController(this);
}

void ASurvivorPlayerController::HandleCharacterSwapped(ACharacterBase* OldCharacter, ACharacterBase* NewCharacter)
{
	if (bIsPlayerDead)
	{
		return;
	}

	StartSwapCooldown();

	if (PlayerUpgradeComponent && PlayerUpgradeComponent->GetSpecialEffectLevel(EUpgradeSpecialEffect::SwapRestoresDashCharge) > 0)
	{
		RestoreDashCharge(1, TEXT("Swap restored dash charge"));
	}

	ApplySharedMoveSpeedToParty();
	SetCameraFollowTarget(NewCharacter);
}

void ASurvivorPlayerController::HandlePlayerDeath()
{
	if (bIsPlayerDead)
	{
		return;
	}

	bIsPlayerDead = true;
	UE_LOG(LogTemp, Log, TEXT("Player Death Triggered"));

	if (InactiveCharacterAssistComponent)
	{
		InactiveCharacterAssistComponent->DeactivateAssistEffect(true);
	}

	ACharacterBase* ActiveCharacter = CharacterManager ? CharacterManager->GetActiveCharacter() : nullptr;
	UE_LOG(LogTemp, Log, TEXT("Active Character = %s"), *GetNameSafe(ActiveCharacter));

	if (ActiveCharacter)
	{
		ActiveCharacter->StopPlayerGameplay();
		UE_LOG(LogTemp, Log, TEXT("Player Input Disabled"));

		if (UAutoAttackComponent* AutoAttackComponent = ActiveCharacter->FindComponentByClass<UAutoAttackComponent>())
		{
			AutoAttackComponent->StopAutoAttack();
			UE_LOG(LogTemp, Log, TEXT("Auto Attack Disabled"));
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("Auto Attack Disabled"));
		}

		ActiveCharacter->PlayDeathMontage();
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("Player Input Disabled"));
		UE_LOG(LogTemp, Log, TEXT("Auto Attack Disabled"));
	}

	UE_LOG(LogTemp, Log, TEXT("Swapping Disabled"));
}

void ASurvivorPlayerController::HandlePlayerLevelUp(int32 NewLevel)
{
	++PendingLevelUpChoices;
	UE_LOG(LogTemp, Log, TEXT("LEVEL UP RECEIVED: NewLevel=%d"), NewLevel);
	UE_LOG(LogTemp, Log, TEXT("Pending selections = %d"), PendingLevelUpChoices);

	if (!bLevelUpSelectionActive)
	{
		StartNextLevelUpSelection();
	}
}

void ASurvivorPlayerController::HandleLevelUpSelectionCompleted()
{
	PendingLevelUpChoices = FMath::Max(0, PendingLevelUpChoices - 1);
	UE_LOG(LogTemp, Log, TEXT("Pending selections remaining = %d"), PendingLevelUpChoices);

	if (PendingLevelUpChoices > 0)
	{
		StartNextLevelUpSelection();
		return;
	}

	bLevelUpSelectionActive = false;
	CloseLevelUpWidget();
	ResumeAfterLevelUpSelection();
	UE_LOG(LogTemp, Log, TEXT("LEVEL-UP FLOW COMPLETE"));
	UE_LOG(LogTemp, Log, TEXT("Gameplay resumed"));
}

void ASurvivorPlayerController::StartNextLevelUpSelection()
{
	if (PendingLevelUpChoices <= 0)
	{
		bLevelUpSelectionActive = false;
		CloseLevelUpWidget();
		ResumeAfterLevelUpSelection();
		return;
	}

	if (!PlayerUpgradeComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("Level-up selection skipped: PlayerUpgradeComponent missing."));
		HandleLevelUpSelectionCompleted();
		return;
	}

	bLevelUpSelectionActive = true;

	if (!PlayerUpgradeComponent->BeginUpgradeSelection(2))
	{
		UE_LOG(LogTemp, Warning, TEXT("Level-up selection skipped: no eligible categories."));
		HandleLevelUpSelectionCompleted();
		return;
	}

	if (!EnsureLevelUpWidget())
	{
		UE_LOG(LogTemp, Warning, TEXT("Level-up selection skipped: LevelUpWidget could not be created."));
		HandleLevelUpSelectionCompleted();
		return;
	}

	PauseForLevelUpSelection();

	const TArray<EUpgradeCategory> CategoryChoices = PlayerUpgradeComponent->GetCurrentCategoryChoices();
	UE_LOG(LogTemp, Log, TEXT("CATEGORY OFFER:"));
	for (const EUpgradeCategory Category : CategoryChoices)
	{
		UE_LOG(LogTemp, Log, TEXT("  %s"), *UpgradeCategoryToLogString(Category));
	}

	LevelUpWidget->InitializeLevelUpWidget(this);
}

bool ASurvivorPlayerController::EnsureLevelUpWidget()
{
	if (LevelUpWidget)
	{
		if (!LevelUpWidget->IsInViewport())
		{
			LevelUpWidget->AddToViewport(100);
		}
		return true;
	}

	if (!LevelUpWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("LevelUpWidgetClass is not assigned on SurvivorPlayerController."));
		return false;
	}

	LevelUpWidget = CreateWidget<ULevelUpWidget>(this, LevelUpWidgetClass);
	if (!LevelUpWidget)
	{
		return false;
	}

	LevelUpWidget->OnSelectionCompleted.AddDynamic(this, &ASurvivorPlayerController::HandleLevelUpSelectionCompleted);
	LevelUpWidget->AddToViewport(100);
	return true;
}

void ASurvivorPlayerController::PauseForLevelUpSelection()
{
	SetPause(true);
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
}

void ASurvivorPlayerController::ResumeAfterLevelUpSelection()
{
	SetPause(false);
	ConfigureInputMode();
}

void ASurvivorPlayerController::CloseLevelUpWidget()
{
	if (LevelUpWidget)
	{
		LevelUpWidget->RemoveFromParent();
	}
}

void ASurvivorPlayerController::StartSwapCooldown()
{
	if (!GetWorld() || SwapCooldown <= 0.0f)
	{
		bCanSwap = true;
		OnSwapCooldownFinished.Broadcast();
		return;
	}

	bCanSwap = false;
	GetWorldTimerManager().ClearTimer(SwapCooldownTimerHandle);
	GetWorldTimerManager().SetTimer(
		SwapCooldownTimerHandle,
		this,
		&ASurvivorPlayerController::HandleSwapCooldownFinished,
		SwapCooldown,
		false);
	OnSwapCooldownStarted.Broadcast(SwapCooldown);
}

void ASurvivorPlayerController::HandleSwapCooldownFinished()
{
	bCanSwap = true;
	OnSwapCooldownFinished.Broadcast();
}

void ASurvivorPlayerController::HandleSharedPlayerStatsChanged()
{
	ApplySharedPlayerStats();
}

void ASurvivorPlayerController::ApplySharedPlayerStats()
{
	ApplySharedMoveSpeedToParty();
	ApplySharedHealthStats();
	ApplyDashChargeStats();
}

void ASurvivorPlayerController::ApplyDashChargeStats()
{
	const int32 PreviousMaxCharges = MaxDashCharges;
	const int32 PreviousCurrentCharges = CurrentDashCharges;
	MaxDashCharges = SharedPlayerStatsComponent ? SharedPlayerStatsComponent->GetFinalMaxDashCharges() : 1;
	MaxDashCharges = FMath::Max(1, MaxDashCharges);

	if (PreviousMaxCharges <= 0)
	{
		CurrentDashCharges = MaxDashCharges;
	}
	else if (MaxDashCharges > PreviousMaxCharges)
	{
		CurrentDashCharges += MaxDashCharges - PreviousMaxCharges;
	}

	CurrentDashCharges = FMath::Clamp(CurrentDashCharges, 0, MaxDashCharges);

	if (PreviousMaxCharges != MaxDashCharges || PreviousCurrentCharges != CurrentDashCharges)
	{
		if (CVarHDLogDashCharges.GetValueOnGameThread() != 0)
		{
			UE_LOG(LogTemp, Log, TEXT("Dash upgrade applied Max Charges: %d -> %d Current Charges: %d -> %d"),
				PreviousMaxCharges,
				MaxDashCharges,
				PreviousCurrentCharges,
				CurrentDashCharges);
		}
		BroadcastDashChargesChanged();
	}

	if (CurrentDashCharges >= MaxDashCharges)
	{
		StopDashRecharge();
	}
	else
	{
		StartDashRechargeIfNeeded();
	}
}

void ASurvivorPlayerController::ApplySharedMoveSpeedToParty()
{
	const float MoveSpeedMultiplier = SharedPlayerStatsComponent ? SharedPlayerStatsComponent->GetFinalMoveSpeedMultiplier() : 1.0f;

	if (!CharacterManager)
	{
		return;
	}

	if (ACharacterBase* Samurai = CharacterManager->GetSamurai())
	{
		Samurai->ApplySharedMoveSpeedMultiplier(MoveSpeedMultiplier);
	}

	if (ACharacterBase* Ninja = CharacterManager->GetNinja())
	{
		Ninja->ApplySharedMoveSpeedMultiplier(MoveSpeedMultiplier);
	}
}

void ASurvivorPlayerController::ApplySharedHealthStats()
{
	if (!PlayerHealthComponent || !SharedPlayerStatsComponent)
	{
		return;
	}

	if (BasePlayerMaxHealth <= 0.0f)
	{
		BasePlayerMaxHealth = PlayerHealthComponent->GetMaxHealth();
	}

	PlayerHealthComponent->SetMaxHealthPreservePercent(BasePlayerMaxHealth * SharedPlayerStatsComponent->GetFinalMaxHealthMultiplier());
}

void ASurvivorPlayerController::UpdateMouseFacingTarget()
{
	if (bIsPlayerDead)
	{
		return;
	}

	ACharacterBase* ControlledCharacter = CharacterManager ? CharacterManager->GetActiveCharacter() : nullptr;
	if (!ControlledCharacter)
	{
		return;
	}

	FVector MouseWorldPosition;
	if (GetMouseWorldPosition(MouseWorldPosition))
	{
		ControlledCharacter->SetFacingTarget(MouseWorldPosition);
	}
}

bool ASurvivorPlayerController::GetMouseWorldPosition(FVector& OutWorldPosition) const
{
	FHitResult HitResult;
	if (GetHitResultUnderCursor(ECC_Visibility, false, HitResult) && HitResult.GetActor() != GetPawn())
	{
		OutWorldPosition = HitResult.ImpactPoint;
		return true;
	}

	FVector WorldLocation;
	FVector WorldDirection;
	if (!DeprojectMousePositionToWorld(WorldLocation, WorldDirection))
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SurvivorMouseWorldPosition), false, GetPawn());
	const FVector TraceEnd = WorldLocation + WorldDirection * 100000.0f;

	if (GetWorld()->LineTraceSingleByChannel(HitResult, WorldLocation, TraceEnd, ECC_Visibility, QueryParams))
	{
		OutWorldPosition = HitResult.ImpactPoint;
		return true;
	}

	return false;
}

FVector ASurvivorPlayerController::GetDashDirection(const ACharacterBase* ActiveCharacter) const
{
	if (!ActiveCharacter)
	{
		return FVector::ZeroVector;
	}

	FVector DashDirection = FVector::RightVector * -LastMovementInput.X + FVector::ForwardVector * -LastMovementInput.Y;
	DashDirection.Z = 0.0f;
	if (!DashDirection.Normalize())
	{
		DashDirection = ActiveCharacter->GetVisualForwardVector();
		DashDirection.Z = 0.0f;
		DashDirection.Normalize();
	}

	return DashDirection;
}

void ASurvivorPlayerController::HandleDashStep(float DeltaTime)
{
	if (!bIsDashing)
	{
		return;
	}

	if (DashDuration <= KINDA_SMALL_NUMBER)
	{
		FinishDash();
		return;
	}

	ACharacterBase* ActiveCharacter = CharacterManager ? CharacterManager->GetActiveCharacter() : nullptr;
	if (!ActiveCharacter)
	{
		FinishDash();
		return;
	}

	const float StepTime = FMath::Min(DeltaTime, DashDuration - DashElapsedTime);
	if (StepTime <= KINDA_SMALL_NUMBER)
	{
		FinishDash();
		return;
	}

	const float DashSpeed = DashDistance / DashDuration;
	const FVector StartLocation = ActiveCharacter->GetActorLocation();
	const FVector DesiredLocation = StartLocation + ActiveDashDirection * DashSpeed * StepTime;

	FHitResult HitResult;
	ActiveCharacter->SetActorLocation(DesiredLocation, true, &HitResult);
	DashElapsedTime += StepTime;

	if (HitResult.bBlockingHit || DashElapsedTime >= DashDuration - KINDA_SMALL_NUMBER)
	{
		FinishDash();
	}
}

void ASurvivorPlayerController::FinishDash()
{
	if (!bIsDashing)
	{
		return;
	}

	bIsDashing = false;
	DashElapsedTime = 0.0f;
	ActiveDashDirection = FVector::ZeroVector;
	if (ACharacterBase* ActiveCharacter = CharacterManager ? CharacterManager->GetActiveCharacter() : nullptr)
	{
		if (UCharacterMovementComponent* MovementComponent = ActiveCharacter->GetCharacterMovement())
		{
			MovementComponent->StopMovementImmediately();
		}
		ActiveCharacter->EndDashVisual();
	}
	OnDashEnded.Broadcast();

	if (CVarHDLogDash.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogTemp, Log, TEXT("Dash ended"));
	}
}

void ASurvivorPlayerController::ConsumeDashCharge()
{
	const int32 PreviousCharges = CurrentDashCharges;
	CurrentDashCharges = FMath::Clamp(CurrentDashCharges - 1, 0, MaxDashCharges);
	BroadcastDashChargesChanged();
	StartDashRechargeIfNeeded();

	if (CVarHDLogDashCharges.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogTemp, Log, TEXT("Dash consumed Charges: %d -> %d Recharge Remaining: %.2f"),
			PreviousCharges,
			CurrentDashCharges,
			GetDashRechargeRemaining());
	}
}

void ASurvivorPlayerController::RestoreDashCharge(int32 ChargeAmount, const TCHAR* RestoreReason)
{
	if (ChargeAmount <= 0)
	{
		return;
	}

	const int32 PreviousCharges = CurrentDashCharges;
	CurrentDashCharges = FMath::Clamp(CurrentDashCharges + ChargeAmount, 0, MaxDashCharges);
	if (CurrentDashCharges == PreviousCharges)
	{
		return;
	}

	BroadcastDashChargesChanged();

	if (CurrentDashCharges >= MaxDashCharges)
	{
		StopDashRecharge();
	}
	else
	{
		StartDashRechargeIfNeeded();
	}

	if (CVarHDLogDashCharges.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogTemp, Log, TEXT("%s Charges: %d -> %d"),
			RestoreReason ? RestoreReason : TEXT("Dash charge restored"),
			PreviousCharges,
			CurrentDashCharges);
	}
}

void ASurvivorPlayerController::StartDashRechargeIfNeeded()
{
	if (!GetWorld() || CurrentDashCharges >= MaxDashCharges || DashRechargeTime <= 0.0f)
	{
		return;
	}

	if (GetWorldTimerManager().IsTimerActive(DashRechargeTimerHandle))
	{
		return;
	}

	GetWorldTimerManager().SetTimer(
		DashRechargeTimerHandle,
		this,
		&ASurvivorPlayerController::HandleDashRechargeTimerElapsed,
		DashRechargeTime,
		false);
	OnDashRechargeStarted.Broadcast();
}

void ASurvivorPlayerController::StopDashRecharge()
{
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(DashRechargeTimerHandle);
	}
}

void ASurvivorPlayerController::HandleDashRechargeTimerElapsed()
{
	StopDashRecharge();
	RestoreDashCharge(1, TEXT("Dash charge restored"));
	OnDashRechargeCompleted.Broadcast();

	if (CurrentDashCharges < MaxDashCharges)
	{
		StartDashRechargeIfNeeded();
	}
}

void ASurvivorPlayerController::BroadcastDashChargesChanged()
{
	OnDashChargesChanged.Broadcast(CurrentDashCharges, MaxDashCharges);
}

void ASurvivorPlayerController::StartHPRegeneration()
{
	UWorld* World = GetWorld();
	if (!World || HPRegenTickInterval <= 0.0f)
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		HPRegenTimerHandle,
		this,
		&ASurvivorPlayerController::HandleHPRegenerationTimerElapsed,
		HPRegenTickInterval,
		true);
}

void ASurvivorPlayerController::StopHPRegeneration()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HPRegenTimerHandle);
	}
}

void ASurvivorPlayerController::HandleHPRegenerationTimerElapsed()
{
	if (!PlayerHealthComponent || PlayerHealthComponent->IsDead())
	{
		StopHPRegeneration();
		return;
	}

	const float RegenPerSecond = GetActiveHPRegenPerSecond();
	if (RegenPerSecond <= 0.0f || PlayerHealthComponent->GetCurrentHealth() >= PlayerHealthComponent->GetMaxHealth())
	{
		return;
	}

	PlayerHealthComponent->Heal(RegenPerSecond * FMath::Max(0.0f, HPRegenTickInterval));
}
