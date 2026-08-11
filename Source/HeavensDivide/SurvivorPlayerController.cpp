// Copyright Epic Games, Inc. All Rights Reserved.

#include "SurvivorPlayerController.h"

#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "ExperienceComponent.h"
#include "HealthComponent.h"
#include "InputActionValue.h"
#include "LevelUpWidget.h"
#include "PlayerCameraRig.h"
#include "PlayerHUDWidget.h"
#include "PlayerUpgradeComponent.h"
#include "NinjaCharacter.h"
#include "SamuraiCharacter.h"
#include "SharedPlayerStatsComponent.h"
#include "AutoAttackComponent.h"

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

	if (ExperienceComponent)
	{
		ExperienceComponent->OnLevelUp.AddDynamic(this, &ASurvivorPlayerController::HandlePlayerLevelUp);
	}

	if (SharedPlayerStatsComponent)
	{
		SharedPlayerStatsComponent->OnStatsChanged.AddDynamic(this, &ASurvivorPlayerController::HandleSharedPlayerStatsChanged);
		ApplySharedPlayerStats();
	}

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

void ASurvivorPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);
	if (EnhancedInputComponent && MoveAction)
	{
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ASurvivorPlayerController::Move);
	}

	if (EnhancedInputComponent && SwapAction)
	{
		EnhancedInputComponent->BindAction(SwapAction, ETriggerEvent::Started, this, &ASurvivorPlayerController::Swap);
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

	ControlledCharacter->MoveCharacter(Value.Get<FVector2D>());
}

void ASurvivorPlayerController::Swap(const FInputActionValue& Value)
{
	if (bIsPlayerDead)
	{
		UE_LOG(LogTemp, Log, TEXT("Swapping Disabled"));
		return;
	}

	if (CharacterManager)
	{
		CharacterManager->SwapCharacter();
	}
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

	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	if (LevelUpWidget)
	{
		InputMode.SetWidgetToFocus(LevelUpWidget->TakeWidget());
	}
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

void ASurvivorPlayerController::HandleSharedPlayerStatsChanged()
{
	ApplySharedPlayerStats();
}

void ASurvivorPlayerController::ApplySharedPlayerStats()
{
	ApplySharedMoveSpeedToParty();
	ApplySharedHealthStats();
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
