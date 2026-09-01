// Copyright Epic Games, Inc. All Rights Reserved.

#include "CharacterManagerComponent.h"

#include "CharacterBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NinjaCharacter.h"
#include "SamuraiCharacter.h"
#include "SurvivorPlayerController.h"

UCharacterManagerComponent::UCharacterManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCharacterManagerComponent::InitializeParty()
{
	if (SamuraiCharacter || NinjaCharacter)
	{
		return;
	}

	APlayerController* OwningController = Cast<APlayerController>(GetOwner());
	if (!OwningController || !GetWorld())
	{
		return;
	}

	if (!SamuraiClass || !NinjaClass)
	{
		UE_LOG(LogTemp, Error, TEXT("CharacterManagerComponent: SamuraiClass and NinjaClass must both be assigned to Blueprint character classes."));
		return;
	}

	if (SamuraiClass == ASamuraiCharacter::StaticClass() || NinjaClass == ANinjaCharacter::StaticClass())
	{
		UE_LOG(LogTemp, Error, TEXT("CharacterManagerComponent: Do not assign native C++ character classes. Assign BP_Samurai and BP_Ninja instead. SamuraiClass=%s NinjaClass=%s"),
			*GetNameSafe(SamuraiClass.Get()),
			*GetNameSafe(NinjaClass.Get()));
		return;
	}

	const FTransform SpawnTransform = GetInitialSpawnTransform();
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = OwningController;
	SpawnParameters.Instigator = OwningController->GetPawn();
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	SamuraiCharacter = GetWorld()->SpawnActor<ASamuraiCharacter>(SamuraiClass, SpawnTransform, SpawnParameters);
	NinjaCharacter = GetWorld()->SpawnActor<ANinjaCharacter>(NinjaClass, SpawnTransform, SpawnParameters);

	if (!SamuraiCharacter || !NinjaCharacter)
	{
		UE_LOG(LogTemp, Error, TEXT("CharacterManagerComponent: Failed to spawn configured party. Samurai=%s Ninja=%s"),
			*GetNameSafe(SamuraiCharacter),
			*GetNameSafe(NinjaCharacter));
		return;
	}

	ActiveCharacter = SamuraiCharacter;
	ApplyInitialCharacterModes();

	if (ActiveCharacter)
	{
		OwningController->Possess(ActiveCharacter);
	}
}

ACharacterBase* UCharacterManagerComponent::GetActiveCharacter() const
{
	return ActiveCharacter;
}

ACharacterBase* UCharacterManagerComponent::GetInactiveCharacter() const
{
	if (SamuraiCharacter && SamuraiCharacter != ActiveCharacter)
	{
		return SamuraiCharacter;
	}

	if (NinjaCharacter && NinjaCharacter != ActiveCharacter)
	{
		return NinjaCharacter;
	}

	return nullptr;
}

ASamuraiCharacter* UCharacterManagerComponent::GetSamurai() const
{
	return SamuraiCharacter;
}

ANinjaCharacter* UCharacterManagerComponent::GetNinja() const
{
	return NinjaCharacter;
}

void UCharacterManagerComponent::SwapCharacter()
{
	if (bIsSwapInProgress)
	{
		UE_LOG(LogTemp, Warning, TEXT("SWAP FAILED: swap already in progress."));
		return;
	}

	ASurvivorPlayerController* OwningController = Cast<ASurvivorPlayerController>(GetOwner());
	ACharacterBase* OldCharacter = ActiveCharacter;
	ACharacterBase* NewCharacter = GetInactiveCharacter();

	if (!OwningController || !OldCharacter || !NewCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("SWAP FAILED: invalid references. Controller=%s Old=%s New=%s"),
			*GetNameSafe(OwningController),
			*GetNameSafe(OldCharacter),
			*GetNameSafe(NewCharacter));
		return;
	}

	bIsSwapInProgress = true;

	const FVector OldLocation = OldCharacter->GetActorLocation();
	const FRotator OldVisualRotation = OldCharacter->GetVisualFacingRotation();
	const FVector OldVelocity = OldCharacter->GetVelocity();

	NewCharacter->SetActorLocation(OldLocation, false, nullptr, ETeleportType::TeleportPhysics);
	NewCharacter->SetVisualFacingRotation(OldVisualRotation);

	NewCharacter->SetCharacterMode(ECharacterMode::Active);

	if (UCharacterMovementComponent* NewMovement = NewCharacter->GetCharacterMovement())
	{
		NewMovement->Velocity = FVector(OldVelocity.X, OldVelocity.Y, NewMovement->Velocity.Z);
	}

	OwningController->Possess(NewCharacter);

	if (OwningController->GetPawn() != NewCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("SWAP FAILED: possession did not result in NewCharacter."));
		NewCharacter->SetCharacterMode(ECharacterMode::Inactive);
		bIsSwapInProgress = false;
		return;
	}

	ActiveCharacter = NewCharacter;
	OwningController->SetCameraFollowTarget(NewCharacter);

	OldCharacter->SetCharacterMode(ECharacterMode::Inactive);

	OnCharacterSwapped.Broadcast(OldCharacter, NewCharacter);
	bIsSwapInProgress = false;
}

FTransform UCharacterManagerComponent::GetInitialSpawnTransform() const
{
	APlayerController* OwningController = Cast<APlayerController>(GetOwner());
	if (OwningController)
	{
		if (AGameModeBase* GameMode = UGameplayStatics::GetGameMode(OwningController))
		{
			if (AActor* PlayerStart = GameMode->FindPlayerStart(OwningController))
			{
				return PlayerStart->GetActorTransform();
			}
		}

		if (AActor* StartSpot = OwningController->StartSpot.Get())
		{
			return StartSpot->GetActorTransform();
		}

		if (APawn* ExistingPawn = OwningController->GetPawn())
		{
			return ExistingPawn->GetActorTransform();
		}

		return FTransform(OwningController->GetControlRotation(), OwningController->GetFocalLocation());
	}

	return FTransform::Identity;
}

void UCharacterManagerComponent::ApplyInitialCharacterModes()
{
	if (SamuraiCharacter)
	{
		SamuraiCharacter->SetCharacterMode(SamuraiCharacter == ActiveCharacter ? ECharacterMode::Active : ECharacterMode::Inactive);
	}

	if (NinjaCharacter)
	{
		NinjaCharacter->SetCharacterMode(NinjaCharacter == ActiveCharacter ? ECharacterMode::Active : ECharacterMode::Inactive);
	}
}
