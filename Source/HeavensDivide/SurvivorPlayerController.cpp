// Copyright Epic Games, Inc. All Rights Reserved.

#include "SurvivorPlayerController.h"

#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "HealthComponent.h"
#include "InputActionValue.h"
#include "PlayerCameraRig.h"
#include "PlayerHUDWidget.h"
#include "AutoAttackComponent.h"

ASurvivorPlayerController::ASurvivorPlayerController()
{
	bAutoManageActiveCameraTarget = false;
	CharacterManager = CreateDefaultSubobject<UCharacterManagerComponent>(TEXT("CharacterManager"));
	PlayerHealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("PlayerHealthComponent"));
}

void ASurvivorPlayerController::BeginPlay()
{
	Super::BeginPlay();

	ConfigureInputMode();
	if (CharacterManager)
	{
		CharacterManager->InitializeParty();
		CharacterManager->OnCharacterSwapped.AddDynamic(this, &ASurvivorPlayerController::HandleCharacterSwapped);
	}
	InitializePlayerCameraRig();
	InitializePlayerHUD();

	if (PlayerHealthComponent)
	{
		PlayerHealthComponent->OnDeath.AddDynamic(this, &ASurvivorPlayerController::HandlePlayerDeath);
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

bool ASurvivorPlayerController::IsPlayerDead() const
{
	return bIsPlayerDead;
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
