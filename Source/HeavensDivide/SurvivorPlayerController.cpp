// Copyright Epic Games, Inc. All Rights Reserved.

#include "SurvivorPlayerController.h"

#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "EnemySpawner.h"
#include "ExperienceComponent.h"
#include "FinalBossBase.h"
#include "EnemyBase.h"
#include "EnemyStatusEffectComponent.h"
#include "GameOverWidget.h"
#include "HealthComponent.h"
#include "HeavensDivideGameUserSettings.h"
#include "Interactable.h"
#include "InactiveCharacterAssistComponent.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "LevelUpWidget.h"
#include "PlayerCameraRig.h"
#include "PlayerHUDWidget.h"
#include "PlayerUpgradeComponent.h"
#include "NinjaCharacter.h"
#include "SamuraiCharacter.h"
#include "SharedPlayerStatsComponent.h"
#include "ShadowClone.h"
#include "SynergyMetaProgressionSubsystem.h"
#include "RunTravelSubsystem.h"
#include "AutoAttackComponent.h"
#include "CharacterStatsComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/OverlapResult.h"
#include "TimerManager.h"
#include "EngineUtils.h"
#include "UpgradeDefinition.h"
#include "UObject/ConstructorHelpers.h"

static TAutoConsoleVariable<int32> CVarHDLogDash(
	TEXT("hd.LogDash"),
	0,
	TEXT("Logs player dash start, block, and end events when enabled."));

static TAutoConsoleVariable<int32> CVarHDLogDashCharges(
	TEXT("hd.LogDashCharges"),
	0,
	TEXT("Logs player dash charge state changes when enabled."));

static TAutoConsoleVariable<int32> CVarHDLogHandoff(
	TEXT("hd.LogHandoff"),
	0,
	TEXT("Logs Handoff attack speed buff application and expiration when enabled."));

namespace HandoffBuffIds
{
	static const FName ModifierId(TEXT("Handoff_AttackSpeed"));
	static const FName SourceId(TEXT("Handoff"));
}

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
	case EUpgradeCategory::Cursed:
		return TEXT("Blood Pact");
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
	InteractAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_Interact_Runtime"));
	InteractAction->ValueType = EInputActionValueType::Boolean;
	AimAction = CreateDefaultSubobject<UInputAction>(TEXT("IA_Aim_Runtime"));
	AimAction->ValueType = EInputActionValueType::Axis2D;
	ShadowCloneClass = AShadowClone::StaticClass();
	GameOverWidgetClass = UGameOverWidget::StaticClass();
	static ConstructorHelpers::FClassFinder<UGameOverWidget> GameOverWidgetBlueprint(
		TEXT("/Game/HeavensDivide/Blueprints/UI/WBP_GameOver"));
	if (GameOverWidgetBlueprint.Succeeded()) GameOverWidgetClass = GameOverWidgetBlueprint.Class;
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
		if (InactiveCharacterAssistComponent)
		{
			InactiveCharacterAssistComponent->RefreshAssistEffectState();
		}
		CharacterManager->OnCharacterSwapped.AddDynamic(this, &ASurvivorPlayerController::HandleCharacterSwapped);
		if (const ACharacterBase* ActiveCharacter = CharacterManager->GetActiveCharacter())
		{
			LastValidControllerAimDirection = ActiveCharacter->GetVisualForwardVector();
			LastValidControllerAimDirection.Z = 0.0f;
			LastValidControllerAimDirection.Normalize();
		}
	}
	InitializePlayerCameraRig();
	ResolveRunTimeSource();
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
	ApplyDashChargeStats();
	if (URunTravelSubsystem* TravelState = GetGameInstance() ? GetGameInstance()->GetSubsystem<URunTravelSubsystem>() : nullptr)
	{
		if (TravelState->RestoreToController(this) && GetWorld())
		{
			GetWorldTimerManager().SetTimerForNextTick(this, &ASurvivorPlayerController::StartRestoredBossArenaCombat);
		}
	}

	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (InputSubsystem && DefaultMappingContext)
	{
		auto EnsureMapping = [this](UInputAction* Action, const FKey Key)
		{
			if (Action && !DefaultMappingContext->GetMappings().ContainsByPredicate([Action, Key](const FEnhancedActionKeyMapping& Mapping)
			{
				return Mapping.Action == Action && Mapping.Key == Key;
			}))
			{
				DefaultMappingContext->MapKey(Action, Key);
			}
		};

		EnsureMapping(MoveAction, EKeys::Gamepad_Left2D);
		EnsureMapping(DashAction, EKeys::Gamepad_FaceButton_Right);
		EnsureMapping(SwapAction, EKeys::Gamepad_FaceButton_Top);
		EnsureMapping(InteractAction, EKeys::Gamepad_FaceButton_Bottom);
		EnsureMapping(AimAction, EKeys::Gamepad_Right2D);
		InputSubsystem->AddMappingContext(DefaultMappingContext, 0);
	}
}

void ASurvivorPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyAllShadowClones();
	if (CharacterManager)
	{
		RemoveHandoffBuff(CharacterManager->GetSamurai());
		RemoveHandoffBuff(CharacterManager->GetNinja());
	}

	Super::EndPlay(EndPlayReason);
}

void ASurvivorPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	UpdateManualTargetingInput();
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

bool ASurvivorPlayerController::TryBeginObjective(AActor* ObjectiveActor)
{
	if (!IsValid(ObjectiveActor) || ActiveObjective.IsValid())
	{
		return false;
	}

	ActiveObjective = ObjectiveActor;
	return true;
}

void ASurvivorPlayerController::EndObjective(AActor* ObjectiveActor)
{
	if (ObjectiveActor && ActiveObjective.Get() == ObjectiveActor)
	{
		ActiveObjective.Reset();
	}
}

bool ASurvivorPlayerController::IsAnyObjectiveActive() const
{
	return ActiveObjective.IsValid();
}

AActor* ASurvivorPlayerController::GetActiveObjective() const
{
	return ActiveObjective.Get();
}

float ASurvivorPlayerController::GetRunTimeSeconds() const
{
	if (RunTimeSource) return RunTimeSource->GetRunTimeSeconds();
	if (const URunTravelSubsystem* Travel = GetGameInstance() ? GetGameInstance()->GetSubsystem<URunTravelSubsystem>() : nullptr) return Travel->GetCapturedRunTimeSeconds();
	return 0.0f;
}

void ASurvivorPlayerController::StartRestoredBossArenaCombat()
{
	AFinalBossBase* BossToStart = nullptr;
	int32 BossCount = 0;
	for (TActorIterator<AFinalBossBase> It(GetWorld()); It; ++It)
	{
		++BossCount;
		if (!BossToStart) BossToStart = *It;
	}
	UE_LOG(LogTemp, Warning, TEXT("[BossStartup] RunStateRestored BossCount=%d Boss=%s Player=%s"), BossCount, *GetNameSafe(BossToStart), *GetNameSafe(GetPawn()));
	if (BossToStart) BossToStart->StartBossCombat();
}

bool ASurvivorPlayerController::CanSwap() const
{
	return !bIsPlayerDead
		&& !bIsDashing
		&& !bLevelUpSelectionActive
		&& !bSwapLocked
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
	bPendingNinjaShadowClone = ActiveCharacter->IsA<ANinjaCharacter>()
		&& PlayerUpgradeComponent && PlayerUpgradeComponent->HasUpgradeId(TEXT("ShadowStep"));
	if (bPendingNinjaShadowClone)
	{
		PendingShadowCloneTransform = ActiveCharacter->GetActorTransform();
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

	PlayerHealthComponent->ApplyDamage(DamageAmount);
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
	if (EnhancedInputComponent && InteractAction)
	{
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &ASurvivorPlayerController::Interact);
	}
	if (EnhancedInputComponent && AimAction)
	{
		EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Triggered, this, &ASurvivorPlayerController::Aim);
	}

	InputComponent->BindKey(EKeys::E, IE_Pressed, this, &ASurvivorPlayerController::Interact);
}

void ASurvivorPlayerController::RestoreRunTravelDashCharges(int32 Charges)
{
	CurrentDashCharges = FMath::Clamp(Charges, 0, MaxDashCharges);
	BroadcastDashChargesChanged();
	if (CurrentDashCharges < MaxDashCharges) StartDashRechargeIfNeeded(); else StopDashRecharge();
}

void ASurvivorPlayerController::RefreshRunTravelDerivedStats()
{
	ApplySharedPlayerStats();
}

void ASurvivorPlayerController::ShowBossHealthBar(AFinalBossBase* Boss)
{
	ActiveBossHealthBar = Boss;
	if (PlayerHUDWidget) PlayerHUDWidget->ShowBossHealthBar(Boss);
}

void ASurvivorPlayerController::HideBossHealthBar(AFinalBossBase* Boss)
{
	if (Boss && ActiveBossHealthBar.IsValid() && ActiveBossHealthBar.Get() != Boss) return;
	ActiveBossHealthBar.Reset();
	if (PlayerHUDWidget) PlayerHUDWidget->HideBossHealthBar(Boss);
}

void ASurvivorPlayerController::Move(const FInputActionValue& Value)
{
	if (bIsPlayerDead || bLevelUpSelectionActive)
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
	if (bLevelUpSelectionActive) return;
	if (!CanSwap())
	{
		UE_LOG(LogTemp, Log, TEXT("Swapping Disabled"));
		return;
	}

	if (CharacterManager)
	{
		bPlayerInitiatedSwapPending = true;
		CharacterManager->SwapCharacter();
		bPlayerInitiatedSwapPending = false;
	}
}

void ASurvivorPlayerController::Dash(const FInputActionValue& Value)
{
	if (bLevelUpSelectionActive) return;
	TryDash();
}

void ASurvivorPlayerController::Interact()
{
	if (bLevelUpSelectionActive) return;
	if (bIsPlayerDead || bLevelUpSelectionActive || !GetWorld())
	{
		return;
	}

	APawn* InteractingPawn = GetPawn();
	if (!InteractingPawn)
	{
		return;
	}

	AActor* BestInteractable = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Candidate = *It;
		if (!Candidate || !Candidate->GetClass()->ImplementsInterface(UInteractable::StaticClass())
			|| !IInteractable::Execute_CanInteract(Candidate, InteractingPawn))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared2D(InteractingPawn->GetActorLocation(), Candidate->GetActorLocation());
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestInteractable = Candidate;
		}
	}

	if (BestInteractable)
	{
		IInteractable::Execute_Interact(BestInteractable, InteractingPawn);
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
	if (AFinalBossBase* Boss = ActiveBossHealthBar.Get()) PlayerHUDWidget->ShowBossHealthBar(Boss);
}

void ASurvivorPlayerController::ResolveRunTimeSource()
{
	RunTimeSource = nullptr;
	if (!GetWorld())
	{
		return;
	}

	for (TActorIterator<AEnemySpawner> It(GetWorld()); It; ++It)
	{
		RunTimeSource = *It;
		break;
	}
}

void ASurvivorPlayerController::HandleCharacterSwapped(ACharacterBase* OldCharacter, ACharacterBase* NewCharacter)
{
	if (bIsPlayerDead)
	{
		return;
	}
	if (bSuppressSwapEffects)
	{
		ApplySharedMoveSpeedToParty();
		SetCameraFollowTarget(NewCharacter);
		return;
	}
	const bool bWasPlayerInitiatedSwap = bPlayerInitiatedSwapPending;

	StartSwapCooldown();

	if (PlayerUpgradeComponent && PlayerUpgradeComponent->GetSpecialEffectLevel(EUpgradeSpecialEffect::SwapRestoresDashCharge) > 0)
	{
		RestoreDashCharge(1, TEXT("Swap restored dash charge"));
	}

	ApplyHandoffBuff(NewCharacter);
	ApplySharedMoveSpeedToParty();
	SetCameraFollowTarget(NewCharacter);
	if (bWasPlayerInitiatedSwap) TryTriggerHemotoxicReaction(NewCharacter);
}

void ASurvivorPlayerController::Aim(const FInputActionValue& Value)
{
	if (bIsPlayerDead || bLevelUpSelectionActive || IsAutoTargetingEnabled()) return;
	ApplyControllerAimInput(Value.Get<FVector2D>());
}

void ASurvivorPlayerController::TryTriggerHemotoxicReaction(ACharacterBase* NewCharacter)
{
	if (!NewCharacter || !PlayerUpgradeComponent || !GetWorld()) return;
	const UUpgradeDefinition* Upgrade = PlayerUpgradeComponent->GetAcquiredUpgradeWithSpecialEffect(EUpgradeSpecialEffect::HemotoxicReaction);
	if (!Upgrade) return;

	const float Radius = FMath::Max(0.0f, Upgrade->HemotoxicReactionRadius);
	const float Multiplier = FMath::Max(0.0f, Upgrade->HemotoxicReactionMultiplier);
	FCollisionObjectQueryParams ObjectTypes;
	ObjectTypes.AddObjectTypesToQuery(ECC_Pawn);
	ObjectTypes.AddObjectTypesToQuery(ECC_GameTraceChannel1);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(HemotoxicReaction), false, NewCharacter);
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, NewCharacter->GetActorLocation(), FQuat::Identity, ObjectTypes, FCollisionShape::MakeSphere(Radius), QueryParams);

	TSet<AEnemyBase*> Processed;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AEnemyBase* Enemy = Cast<AEnemyBase>(Overlap.GetActor());
		if (!Enemy || Enemy->IsDead() || Processed.Contains(Enemy)) continue;
		Processed.Add(Enemy);
		UEnemyStatusEffectComponent* Statuses = Enemy->GetStatusEffectComponent();
		if (!Statuses || !Statuses->HasStatus(EEnemyStatusEffect::Bleed) || !Statuses->HasStatus(EEnemyStatusEffect::Poison)) continue;

		const int32 BleedStacks = Statuses->GetStatusStacks(EEnemyStatusEffect::Bleed);
		const int32 PoisonStacks = Statuses->GetStatusStacks(EEnemyStatusEffect::Poison);
		const float RemainingDamage = Statuses->CalculateRemainingStatusDamage(EEnemyStatusEffect::Bleed)
			+ Statuses->CalculateRemainingStatusDamage(EEnemyStatusEffect::Poison);
		const float ReactionDamage = RemainingDamage * Multiplier;
		Statuses->ConsumeStatus(EEnemyStatusEffect::Bleed);
		Statuses->ConsumeStatus(EEnemyStatusEffect::Poison);
		if (ReactionDamage <= 0.0f || !Enemy->ApplyPlayerDamage(ReactionDamage, EPlayerAttackSource::Other)) continue;
		OnHemotoxicReactionTriggered.Broadcast(Enemy, Enemy->GetActorLocation(), ReactionDamage, BleedStacks, PoisonStacks);
	}
}

void ASurvivorPlayerController::HandlePlayerDeath()
{
	if (bIsPlayerDead)
	{
		return;
	}

	bIsPlayerDead = true;
	DestroyAllShadowClones();
	const float FinalRunTimeSeconds = GetRunTimeSeconds();
	if (RunTimeSource)
	{
		RunTimeSource->FreezeRunTime();
		RunTimeSource->SetSpawningEnabled(false);
	}
	CloseLevelUpWidget();
	PendingLevelUpChoices = 0;
	PendingBloodShrineRewards = 0;
	PendingTwinSoulRewards = 0;
	PendingTwinSoulDiscoveries = 0;
	PendingSamuraiTrialRewards = 0;
	PendingNinjaTrialRewards = 0;
	bLevelUpSelectionActive = false;
	bCurrentSelectionIsBloodShrineReward = false;
	bCurrentSelectionIsTwinSoulReward = false;
	bCurrentSelectionIsTwinSoulDiscovery = false;
	bCurrentSelectionIsSamuraiTrialReward = false;
	bCurrentSelectionIsNinjaTrialReward = false;
	bLevelUpTimeDilationApplied = false;
	StopDashRecharge();
	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);
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
	PresentGameOver(FinalRunTimeSeconds);
}

void ASurvivorPlayerController::SetSwapLocked(bool bLocked)
{
	bSwapLocked = bLocked;
}

bool ASurvivorPlayerController::ForceSamuraiActive()
{
	if (!CharacterManager || !CharacterManager->GetSamurai()) return false;
	if (CharacterManager->GetActiveCharacter() == CharacterManager->GetSamurai()) return true;
	if (InactiveCharacterAssistComponent) InactiveCharacterAssistComponent->DeactivateAssistEffect(true);
	bSuppressSwapEffects = true;
	CharacterManager->SwapCharacter();
	bSuppressSwapEffects = false;
	if (InactiveCharacterAssistComponent) InactiveCharacterAssistComponent->RefreshAssistEffectState();
	return CharacterManager->GetActiveCharacter() == CharacterManager->GetSamurai();
}

bool ASurvivorPlayerController::ForceNinjaActive()
{
	if(!CharacterManager||!CharacterManager->GetNinja())return false;
	if(CharacterManager->GetActiveCharacter()==CharacterManager->GetNinja())return true;
	if(InactiveCharacterAssistComponent)InactiveCharacterAssistComponent->DeactivateAssistEffect(true);
	bSuppressSwapEffects=true;
	CharacterManager->SwapCharacter();
	bSuppressSwapEffects=false;
	if(InactiveCharacterAssistComponent)InactiveCharacterAssistComponent->RefreshAssistEffectState();
	return CharacterManager->GetActiveCharacter()==CharacterManager->GetNinja();
}

void ASurvivorPlayerController::PresentGameOver(float FinalRunTimeSeconds)
{
	if (bGameOverPresented) return;
	bGameOverPresented = true;
	if (UWorld* World = GetWorld()) UGameplayStatics::SetGlobalTimeDilation(World, 0.0f);
	GameOverWidget = GameOverWidgetClass ? CreateWidget<UGameOverWidget>(this, GameOverWidgetClass) : nullptr;
	if (!GameOverWidget) return;
	GameOverWidget->InitializeGameOver(this, FinalRunTimeSeconds);
	GameOverWidget->AddToViewport(1000);
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(GameOverWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	GameOverWidget->SetKeyboardFocus();
}

void ASurvivorPlayerController::HandlePlayerLevelUp(int32 NewLevel)
{
	++PendingLevelUpChoices;
	UE_LOG(LogTemp, Log, TEXT("LEVEL UP RECEIVED: NewLevel=%d"), NewLevel);
	UE_LOG(LogTemp, Log, TEXT("Pending selections = %d"), PendingLevelUpChoices);

	if (!bLevelUpSelectionActive)
	{
		StartNextUpgradeSelection();
	}
}

void ASurvivorPlayerController::RequestBloodShrineUpgradeReward(int32 UpgradeChoiceCount)
{
	if (bIsPlayerDead)
	{
		return;
	}

	BloodShrineRewardChoiceCount = FMath::Max(1, UpgradeChoiceCount);
	++PendingBloodShrineRewards;
	if (!bLevelUpSelectionActive)
	{
		StartNextUpgradeSelection();
	}
}

void ASurvivorPlayerController::RequestTwinSoulSynergyReward(int32 UpgradeChoiceCount)
{
	if (bIsPlayerDead)
	{
		return;
	}

	TwinSoulRewardChoiceCount = FMath::Max(1, UpgradeChoiceCount);
	++PendingTwinSoulRewards;
	if (!bLevelUpSelectionActive)
	{
		StartNextUpgradeSelection();
	}
}

void ASurvivorPlayerController::RequestTwinSoulCompletionRewards(int32 NormalChoiceCount, int32 DiscoveryChoiceCount)
{
	if (bIsPlayerDead) return;
	TwinSoulRewardChoiceCount = FMath::Max(1, NormalChoiceCount);
	TwinSoulDiscoveryChoiceCount = FMath::Max(1, DiscoveryChoiceCount);
	++PendingTwinSoulRewards;

	USynergyMetaProgressionSubsystem* MetaSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<USynergyMetaProgressionSubsystem>() : nullptr;
	const bool bHasLockedCandidates = PlayerUpgradeComponent
		&& PlayerUpgradeComponent->GetLockedSynergyDiscoveryCandidates().Num() > 0;
	if (MetaSubsystem)
	{
		if (!bHasLockedCandidates)
		{
			MetaSubsystem->ResetTwinSoulDiscoveryProgress();
		}
		else if (MetaSubsystem->RecordTwinSoulCompletion())
		{
			++PendingTwinSoulDiscoveries;
		}
	}

	if (!bLevelUpSelectionActive) StartNextUpgradeSelection();
}

void ASurvivorPlayerController::RequestSamuraiTrialUpgradeReward(int32 UpgradeChoiceCount)
{
	if (bIsPlayerDead) return;
	SamuraiTrialRewardChoiceCount = FMath::Max(1, UpgradeChoiceCount);
	++PendingSamuraiTrialRewards;
	if (!bLevelUpSelectionActive) StartNextUpgradeSelection();
}

void ASurvivorPlayerController::RequestNinjaTrialUpgradeReward(int32 UpgradeChoiceCount)
{
	if (bIsPlayerDead) return;
	NinjaTrialRewardChoiceCount = FMath::Max(1, UpgradeChoiceCount);
	++PendingNinjaTrialRewards;
	if (!bLevelUpSelectionActive) StartNextUpgradeSelection();
}

void ASurvivorPlayerController::HandleLevelUpSelectionCompleted()
{
	const bool bCompletedTwinSoulReward = bCurrentSelectionIsTwinSoulReward;
	const bool bCompletedTwinSoulDiscovery = bCurrentSelectionIsTwinSoulDiscovery;
	const bool bCompletedSamuraiTrialReward = bCurrentSelectionIsSamuraiTrialReward;
	const bool bCompletedNinjaTrialReward = bCurrentSelectionIsNinjaTrialReward;
	if (bCurrentSelectionIsTwinSoulDiscovery)
	{
		PendingTwinSoulDiscoveries = FMath::Max(0, PendingTwinSoulDiscoveries - 1);
		if (USynergyMetaProgressionSubsystem* MetaSubsystem = GetGameInstance()
			? GetGameInstance()->GetSubsystem<USynergyMetaProgressionSubsystem>() : nullptr)
		{
			MetaSubsystem->ConsumeTwinSoulDiscoveryProgress();
		}
	}
	else if (bCurrentSelectionIsSamuraiTrialReward)
	{
		PendingSamuraiTrialRewards = FMath::Max(0, PendingSamuraiTrialRewards - 1);
	}
	else if (bCurrentSelectionIsNinjaTrialReward)
	{
		PendingNinjaTrialRewards = FMath::Max(0, PendingNinjaTrialRewards - 1);
	}
	else if (bCurrentSelectionIsTwinSoulReward)
	{
		PendingTwinSoulRewards = FMath::Max(0, PendingTwinSoulRewards - 1);
	}
	else if (bCurrentSelectionIsBloodShrineReward)
	{
		PendingBloodShrineRewards = FMath::Max(0, PendingBloodShrineRewards - 1);
	}
	else
	{
		PendingLevelUpChoices = FMath::Max(0, PendingLevelUpChoices - 1);
	}
	bCurrentSelectionIsBloodShrineReward = false;
	bCurrentSelectionIsTwinSoulReward = false;
	bCurrentSelectionIsTwinSoulDiscovery = false;
	bCurrentSelectionIsSamuraiTrialReward = false;
	bCurrentSelectionIsNinjaTrialReward = false;
	if (bCompletedSamuraiTrialReward && PendingSamuraiTrialRewards == 0) OnSamuraiTrialRewardCompleted.Broadcast();
	if (bCompletedNinjaTrialReward && PendingNinjaTrialRewards == 0) OnNinjaTrialRewardCompleted.Broadcast();
	if ((bCompletedTwinSoulReward || bCompletedTwinSoulDiscovery)
		&& PendingTwinSoulRewards == 0 && PendingTwinSoulDiscoveries == 0)
	{
		OnTwinSoulRewardCompleted.Broadcast();
	}

	if (PendingLevelUpChoices > 0 || PendingBloodShrineRewards > 0 || PendingTwinSoulRewards > 0 || PendingTwinSoulDiscoveries > 0 || PendingSamuraiTrialRewards > 0 || PendingNinjaTrialRewards > 0)
	{
		StartNextUpgradeSelection();
		return;
	}

	bLevelUpSelectionActive = false;
	CloseLevelUpWidget();
	ResumeAfterLevelUpSelection();
}

void ASurvivorPlayerController::StartNextUpgradeSelection()
{
	if (PendingLevelUpChoices > 0)
	{
		StartNextLevelUpSelection();
		return;
	}

	if (PendingTwinSoulRewards > 0)
	{
		bCurrentSelectionIsBloodShrineReward = false;
		bCurrentSelectionIsTwinSoulReward = true;
		bLevelUpSelectionActive = true;
		if (!PlayerUpgradeComponent
			|| !PlayerUpgradeComponent->BeginDirectCategoryUpgradeSelection(EUpgradeCategory::Synergy, TwinSoulRewardChoiceCount)
			|| !EnsureLevelUpWidget())
		{
			HandleLevelUpSelectionCompleted();
			return;
		}

		PauseForLevelUpSelection();
		LevelUpWidget->InitializeDirectUpgradeWidget(this);
		return;
	}

	if (PendingTwinSoulDiscoveries > 0)
	{
		bCurrentSelectionIsBloodShrineReward = false;
		bCurrentSelectionIsTwinSoulReward = false;
		bCurrentSelectionIsTwinSoulDiscovery = true;
		bLevelUpSelectionActive = true;
		if (!PlayerUpgradeComponent
			|| !PlayerUpgradeComponent->BeginSynergyDiscoverySelection(TwinSoulDiscoveryChoiceCount)
			|| !EnsureLevelUpWidget())
		{
			bCurrentSelectionIsTwinSoulDiscovery = false;
			PendingTwinSoulDiscoveries = FMath::Max(0, PendingTwinSoulDiscoveries - 1);
			if (PendingTwinSoulRewards == 0 && PendingTwinSoulDiscoveries == 0) OnTwinSoulRewardCompleted.Broadcast();
			StartNextUpgradeSelection();
			return;
		}

		PauseForLevelUpSelection();
		LevelUpWidget->InitializeSynergyDiscoveryWidget(this);
		return;
	}

	if (PendingSamuraiTrialRewards > 0)
	{
		bCurrentSelectionIsBloodShrineReward = false;
		bCurrentSelectionIsTwinSoulReward = false;
		bCurrentSelectionIsTwinSoulDiscovery = false;
		bCurrentSelectionIsSamuraiTrialReward = true;
		bLevelUpSelectionActive = true;
		if (!PlayerUpgradeComponent
			|| !PlayerUpgradeComponent->BeginDirectCategoryUpgradeSelection(EUpgradeCategory::SamuraiTrial, SamuraiTrialRewardChoiceCount)
			|| !EnsureLevelUpWidget())
		{
			HandleLevelUpSelectionCompleted();
			return;
		}
		PauseForLevelUpSelection();
		LevelUpWidget->InitializeDirectUpgradeWidget(this);
		return;
	}

	if (PendingNinjaTrialRewards > 0)
	{
		bCurrentSelectionIsBloodShrineReward = false;
		bCurrentSelectionIsTwinSoulReward = false;
		bCurrentSelectionIsTwinSoulDiscovery = false;
		bCurrentSelectionIsSamuraiTrialReward = false;
		bCurrentSelectionIsNinjaTrialReward = true;
		bLevelUpSelectionActive = true;
		if (!PlayerUpgradeComponent
			|| !PlayerUpgradeComponent->BeginDirectCategoryUpgradeSelection(EUpgradeCategory::NinjaTrial, NinjaTrialRewardChoiceCount)
			|| !EnsureLevelUpWidget())
		{
			HandleLevelUpSelectionCompleted();
			return;
		}
		PauseForLevelUpSelection();
		LevelUpWidget->InitializeDirectUpgradeWidget(this);
		return;
	}

	if (PendingBloodShrineRewards <= 0)
	{
		bLevelUpSelectionActive = false;
		CloseLevelUpWidget();
		ResumeAfterLevelUpSelection();
		return;
	}

	bCurrentSelectionIsBloodShrineReward = true;
	bLevelUpSelectionActive = true;
	if (!PlayerUpgradeComponent
		|| !PlayerUpgradeComponent->BeginDirectCategoryUpgradeSelection(EUpgradeCategory::Cursed, BloodShrineRewardChoiceCount))
	{
		UE_LOG(LogTemp, Warning, TEXT("Blood Shrine reward skipped: no currently eligible Blood Pact upgrades."));
		HandleLevelUpSelectionCompleted();
		return;
	}

	if (!EnsureLevelUpWidget())
	{
		UE_LOG(LogTemp, Warning, TEXT("Blood Shrine reward skipped: upgrade widget unavailable."));
		HandleLevelUpSelectionCompleted();
		return;
	}

	PauseForLevelUpSelection();
	LevelUpWidget->InitializeDirectUpgradeWidget(this);
}

void ASurvivorPlayerController::StartNextLevelUpSelection()
{
	bCurrentSelectionIsBloodShrineReward = false;
	bCurrentSelectionIsTwinSoulReward = false;
	bCurrentSelectionIsTwinSoulDiscovery = false;
	bCurrentSelectionIsSamuraiTrialReward = false;
	bCurrentSelectionIsNinjaTrialReward = false;
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
	if (UWorld* World = GetWorld())
	{
		if (!bLevelUpTimeDilationApplied)
		{
			PreviousLevelUpGlobalTimeDilation = World->GetWorldSettings()->GetEffectiveTimeDilation();
			bLevelUpTimeDilationApplied = true;
		}

		UGameplayStatics::SetGlobalTimeDilation(World, 0.0f);
	}

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
	if (UWorld* World = GetWorld())
	{
		if (bLevelUpTimeDilationApplied)
		{
			UGameplayStatics::SetGlobalTimeDilation(World, PreviousLevelUpGlobalTimeDilation);
			bLevelUpTimeDilationApplied = false;
		}
	}

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

void ASurvivorPlayerController::ApplyHandoffBuff(ACharacterBase* NewActiveCharacter)
{
	if (!NewActiveCharacter || !PlayerUpgradeComponent || PlayerUpgradeComponent->GetSpecialEffectLevel(EUpgradeSpecialEffect::Handoff) <= 0)
	{
		return;
	}

	UUpgradeDefinition* HandoffUpgrade = PlayerUpgradeComponent->GetAcquiredUpgradeWithSpecialEffect(EUpgradeSpecialEffect::Handoff);
	const float AttackSpeedBonus = HandoffUpgrade ? FMath::Max(0.0f, HandoffUpgrade->HandoffAttackSpeedBonus) : 0.4f;
	const float Duration = HandoffUpgrade ? FMath::Max(0.0f, HandoffUpgrade->HandoffDuration) : 3.0f;
	UCharacterStatsComponent* CharacterStats = NewActiveCharacter->GetCharacterStats();
	FTimerHandle* HandoffTimerHandle = GetHandoffTimerHandleForCharacter(NewActiveCharacter);
	if (!CharacterStats || !HandoffTimerHandle)
	{
		return;
	}

	FCharacterStatModifier Modifier;
	Modifier.ModifierId = HandoffBuffIds::ModifierId;
	Modifier.SourceId = HandoffBuffIds::SourceId;
	Modifier.Stat = ECharacterStatType::AttackSpeedMultiplier;
	Modifier.Operation = EStatModifierOperation::AddPercent;
	Modifier.Value = AttackSpeedBonus;
	CharacterStats->AddModifier(Modifier);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(*HandoffTimerHandle);
		if (Duration > 0.0f)
		{
			FTimerDelegate ExpireDelegate;
			ExpireDelegate.BindUObject(this, &ASurvivorPlayerController::RemoveHandoffBuff, NewActiveCharacter);
			World->GetTimerManager().SetTimer(*HandoffTimerHandle, ExpireDelegate, Duration, false);
		}
		else
		{
			RemoveHandoffBuff(NewActiveCharacter);
		}
	}

	if (CVarHDLogHandoff.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[Handoff] Applied to %s AttackSpeedBonus=%.2f Duration=%.2f"),
			*GetNameSafe(NewActiveCharacter),
			AttackSpeedBonus,
			Duration);
	}
}

void ASurvivorPlayerController::RemoveHandoffBuff(ACharacterBase* BuffedCharacter)
{
	UCharacterStatsComponent* CharacterStats = BuffedCharacter ? BuffedCharacter->GetCharacterStats() : nullptr;
	if (CharacterStats)
	{
		CharacterStats->RemoveModifier(HandoffBuffIds::ModifierId);
	}

	if (FTimerHandle* HandoffTimerHandle = GetHandoffTimerHandleForCharacter(BuffedCharacter))
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(*HandoffTimerHandle);
		}
	}

	if (CVarHDLogHandoff.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[Handoff] Expired on %s"), *GetNameSafe(BuffedCharacter));
	}
}

FTimerHandle* ASurvivorPlayerController::GetHandoffTimerHandleForCharacter(const ACharacterBase* TargetCharacter)
{
	if (!CharacterManager || !TargetCharacter)
	{
		return nullptr;
	}

	if (TargetCharacter == CharacterManager->GetSamurai())
	{
		return &SamuraiHandoffTimerHandle;
	}

	if (TargetCharacter == CharacterManager->GetNinja())
	{
		return &NinjaHandoffTimerHandle;
	}

	return nullptr;
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
	if (!IsAutoTargetingEnabled() && bControllerIsActiveTargetingDevice)
	{
		FVector AimDirection;
		if (GetCursorAttackDirection(ControlledCharacter->GetActorLocation(), AimDirection))
		{
			ControlledCharacter->SetFacingTarget(ControlledCharacter->GetActorLocation() + AimDirection * 1000.0f);
		}
		return;
	}

	FVector MouseWorldPosition;
	if (GetMouseWorldPosition(MouseWorldPosition))
	{
		ControlledCharacter->SetFacingTarget(MouseWorldPosition);
	}
}

bool ASurvivorPlayerController::IsAutoTargetingEnabled() const
{
	const UHeavensDivideGameUserSettings* Settings = UHeavensDivideGameUserSettings::GetHeavensDivideGameUserSettings();
	return !Settings || Settings->IsAutoTargetingEnabled();
}

void ASurvivorPlayerController::SetAutoTargetingEnabled(bool bEnabled)
{
	if (UHeavensDivideGameUserSettings* Settings = UHeavensDivideGameUserSettings::GetHeavensDivideGameUserSettings())
	{
		Settings->SetAutoTargetingEnabled(bEnabled);
	}
}

bool ASurvivorPlayerController::GetCursorAttackDirection(const FVector& AttackOrigin, FVector& OutDirection) const
{
	if (bControllerIsActiveTargetingDevice)
	{
		OutDirection = LastValidControllerAimDirection;
		OutDirection.Z = 0.0f;
		if (OutDirection.Normalize())
		{
			return true;
		}
	}

	FVector CursorWorldPosition;
	if (GetMouseWorldPosition(CursorWorldPosition))
	{
		OutDirection = CursorWorldPosition - AttackOrigin;
		OutDirection.Z = 0.0f;
		if (OutDirection.Normalize())
		{
			return true;
		}
	}

	const ACharacterBase* ActiveCharacter = CharacterManager ? CharacterManager->GetActiveCharacter() : nullptr;
	OutDirection = ActiveCharacter ? ActiveCharacter->GetVisualForwardVector() : FVector::ForwardVector;
	OutDirection.Z = 0.0f;
	return OutDirection.Normalize();
}

void ASurvivorPlayerController::UpdateManualTargetingInput()
{
	if (bIsPlayerDead || bLevelUpSelectionActive || IsAutoTargetingEnabled())
	{
		return;
	}

	float MouseDeltaX = 0.0f;
	float MouseDeltaY = 0.0f;
	GetInputMouseDelta(MouseDeltaX, MouseDeltaY);
	const bool bMouseButtonPressed = WasInputKeyJustPressed(EKeys::LeftMouseButton)
		|| WasInputKeyJustPressed(EKeys::RightMouseButton)
		|| WasInputKeyJustPressed(EKeys::MiddleMouseButton);
	if (!FMath::IsNearlyZero(MouseDeltaX) || !FMath::IsNearlyZero(MouseDeltaY) || bMouseButtonPressed)
	{
		bControllerIsActiveTargetingDevice = false;
	}

	const FVector2D StickInput(GetInputAnalogKeyState(EKeys::Gamepad_RightX), GetInputAnalogKeyState(EKeys::Gamepad_RightY));
	ApplyControllerAimInput(StickInput);
}

void ASurvivorPlayerController::ApplyControllerAimInput(const FVector2D& StickInput)
{
	if (StickInput.SizeSquared() < FMath::Square(FMath::Clamp(ControllerAimDeadzone, 0.0f, 1.0f)))
	{
		return;
	}

	FRotator CameraRotation = PlayerCameraManager ? PlayerCameraManager->GetCameraRotation() : GetControlRotation();
	CameraRotation.Pitch = 0.0f;
	CameraRotation.Roll = 0.0f;
	FVector CameraForward = CameraRotation.Vector();
	CameraForward.Z = 0.0f;
	CameraForward.Normalize();
	FVector CameraRight = FRotationMatrix(CameraRotation).GetUnitAxis(EAxis::Y);
	CameraRight.Z = 0.0f;
	CameraRight.Normalize();

	FVector WorldAimDirection = CameraRight * StickInput.X - CameraForward * StickInput.Y;
	WorldAimDirection.Z = 0.0f;
	if (WorldAimDirection.Normalize())
	{
		LastValidControllerAimDirection = WorldAimDirection;
		bControllerIsActiveTargetingDevice = true;
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

	const ACharacterBase* ActiveCharacter = CharacterManager ? CharacterManager->GetActiveCharacter() : nullptr;
	const float GameplayPlaneZ = ActiveCharacter ? ActiveCharacter->GetActorLocation().Z : 0.0f;
	if (!FMath::IsNearlyZero(WorldDirection.Z))
	{
		const float DistanceAlongRay = (GameplayPlaneZ - WorldLocation.Z) / WorldDirection.Z;
		if (DistanceAlongRay > 0.0f)
		{
			OutWorldPosition = WorldLocation + WorldDirection * DistanceAlongRay;
			return true;
		}
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
	SpawnShadowCloneForCompletedDash();
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

void ASurvivorPlayerController::SpawnShadowCloneForCompletedDash()
{
	const bool bShouldSpawn = bPendingNinjaShadowClone && !bIsPlayerDead && GetWorld() && ShadowCloneClass;
	bPendingNinjaShadowClone = false;
	if (!bShouldSpawn) return;

	ANinjaCharacter* Ninja = CharacterManager ? Cast<ANinjaCharacter>(CharacterManager->GetActiveCharacter()) : nullptr;
	if (!Ninja) return;
	ActiveShadowClones.RemoveAll([](const TWeakObjectPtr<AShadowClone>& Clone) { return !Clone.IsValid(); });
	while (ActiveShadowClones.Num() >= FMath::Max(1, MaxActiveShadowClones))
	{
		if (AShadowClone* Oldest = ActiveShadowClones[0].Get()) Oldest->Destroy();
		ActiveShadowClones.RemoveAt(0);
	}

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = Ninja;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AShadowClone* Clone = GetWorld()->SpawnActor<AShadowClone>(ShadowCloneClass, PendingShadowCloneTransform, Params);
	if (!Clone) return;
	const int32 BonusAttacks = FMath::Max(0, FMath::RoundToInt(PlayerUpgradeComponent->GetAccumulatedUpgradeMagnitude(TEXT("MultipleStrikes"))));
	Clone->InitializeShadowClone(Ninja, this, 1 + BonusAttacks);
	ActiveShadowClones.Add(Clone);
	OnShadowCloneSpawned.Broadcast(Clone);
}

void ASurvivorPlayerController::DestroyAllShadowClones()
{
	for (const TWeakObjectPtr<AShadowClone>& Clone : ActiveShadowClones)
	{
		if (Clone.IsValid()) Clone->Destroy();
	}
	ActiveShadowClones.Reset();
	bPendingNinjaShadowClone = false;
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

