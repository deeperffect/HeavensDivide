// Copyright Epic Games, Inc. All Rights Reserved.

#include "CharacterBase.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
const TCHAR* CharacterModeToString(ECharacterMode Mode)
{
	switch (Mode)
	{
	case ECharacterMode::Active:
		return TEXT("Active");
	case ECharacterMode::Inactive:
		return TEXT("Inactive");
	case ECharacterMode::Assisting:
		return TEXT("Assisting");
	default:
		return TEXT("Unknown");
	}
}
}

ACharacterBase::ACharacterBase()
{
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = false;

	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(RootComponent);
	GetMesh()->SetupAttachment(VisualRoot);
}

void ACharacterBase::BeginPlay()
{
	Super::BeginPlay();

	if (VisualRoot && GetMesh() && GetMesh()->GetAttachParent() != VisualRoot)
	{
		GetMesh()->AttachToComponent(VisualRoot, FAttachmentTransformRules::KeepRelativeTransform);
	}
}

void ACharacterBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateMouseFacing(DeltaSeconds);
}

void ACharacterBase::MoveCharacter(FVector2D Input)
{
	if (CharacterMode != ECharacterMode::Active)
	{
		return;
	}

	if (!FMath::IsNearlyZero(Input.X))
	{
		AddMovementInput(FVector::RightVector, -Input.X);
	}

	if (!FMath::IsNearlyZero(Input.Y))
	{
		AddMovementInput(FVector::ForwardVector, -Input.Y);
	}
}

void ACharacterBase::SetFacingTarget(FVector WorldTarget)
{
	if (CharacterMode != ECharacterMode::Active)
	{
		return;
	}

	FacingTarget = WorldTarget;
	bHasFacingTarget = true;
}

void ACharacterBase::SetFacingOverrideTarget(FVector WorldTarget)
{
	if (CharacterMode != ECharacterMode::Active)
	{
		return;
	}

	FacingOverrideTarget = WorldTarget;
	bHasFacingOverride = true;
}

void ACharacterBase::ClearFacingOverride()
{
	bHasFacingOverride = false;
}

void ACharacterBase::SetCharacterMode(ECharacterMode NewMode)
{
	const ECharacterMode OldMode = CharacterMode;
	UE_LOG(LogTemp, Log, TEXT("SetCharacterMode: %s %s -> %s"),
		*GetNameSafe(this),
		CharacterModeToString(OldMode),
		CharacterModeToString(NewMode));

	CharacterMode = NewMode;

	switch (CharacterMode)
	{
	case ECharacterMode::Active:
		SetActorHiddenInGame(false);
		if (VisualRoot)
		{
			VisualRoot->SetVisibility(true, true);
			VisualRoot->SetHiddenInGame(false, true);
		}
		if (USkeletalMeshComponent* MeshComponent = GetMesh())
		{
			MeshComponent->SetVisibility(true, true);
			MeshComponent->SetHiddenInGame(false, true);
		}
		SetActorEnableCollision(true);
		SetActorTickEnabled(true);
		break;

	case ECharacterMode::Inactive:
		SetActorHiddenInGame(true);
		SetActorEnableCollision(false);
		SetActorTickEnabled(false);
		bHasFacingTarget = false;
		bHasFacingOverride = false;
		if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
		{
			MovementComponent->StopMovementImmediately();
		}
		break;

	case ECharacterMode::Assisting:
		SetActorHiddenInGame(false);
		if (VisualRoot)
		{
			VisualRoot->SetVisibility(true, true);
			VisualRoot->SetHiddenInGame(false, true);
		}
		if (USkeletalMeshComponent* MeshComponent = GetMesh())
		{
			MeshComponent->SetVisibility(true, true);
			MeshComponent->SetHiddenInGame(false, true);
		}
		SetActorEnableCollision(false);
		SetActorTickEnabled(true);
		break;
	}

	OnCharacterModeChanged.Broadcast(OldMode, CharacterMode);
}

void ACharacterBase::LogVisibilityState(const FString& Context) const
{
	const USceneComponent* CurrentVisualRoot = VisualRoot;
	const USkeletalMeshComponent* MeshComponent = GetMesh();

	UE_LOG(LogTemp, Log, TEXT("%s: %s Mode=%s ActorHidden=%s ActorLocation=%s ActorScale=%s"),
		*Context,
		*GetNameSafe(this),
		CharacterModeToString(CharacterMode),
		IsHidden() ? TEXT("true") : TEXT("false"),
		*GetActorLocation().ToString(),
		*GetActorScale3D().ToString());

	UE_LOG(LogTemp, Log, TEXT("%s: %s VisualRoot Visible=%s HiddenInGame=%s"),
		*Context,
		*GetNameSafe(this),
		(CurrentVisualRoot && CurrentVisualRoot->IsVisible()) ? TEXT("true") : TEXT("false"),
		(CurrentVisualRoot && CurrentVisualRoot->bHiddenInGame) ? TEXT("true") : TEXT("false"));

	UE_LOG(LogTemp, Log, TEXT("%s: %s Mesh Visible=%s HiddenInGame=%s WorldLocation=%s RelativeScale=%s"),
		*Context,
		*GetNameSafe(this),
		(MeshComponent && MeshComponent->IsVisible()) ? TEXT("true") : TEXT("false"),
		(MeshComponent && MeshComponent->bHiddenInGame) ? TEXT("true") : TEXT("false"),
		MeshComponent ? *MeshComponent->GetComponentLocation().ToString() : TEXT("None"),
		MeshComponent ? *MeshComponent->GetRelativeScale3D().ToString() : TEXT("None"));
}

ECharacterMode ACharacterBase::GetCharacterMode() const
{
	return CharacterMode;
}

FRotator ACharacterBase::GetVisualFacingRotation() const
{
	return VisualRoot ? VisualRoot->GetComponentRotation() : GetActorRotation();
}

FVector ACharacterBase::GetVisualForwardVector() const
{
	return VisualRoot ? VisualRoot->GetForwardVector() : GetActorForwardVector();
}

void ACharacterBase::SetVisualFacingRotation(FRotator NewRotation)
{
	if (VisualRoot)
	{
		VisualRoot->SetWorldRotation(FRotator(0.0f, NewRotation.Yaw, 0.0f));
	}
}

void ACharacterBase::StopPlayerGameplay()
{
	bHasFacingTarget = false;
	bHasFacingOverride = false;

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
		MovementComponent->DisableMovement();
	}
}

void ACharacterBase::PlayDeathMontage()
{
	if (!DeathMontage)
	{
		UE_LOG(LogTemp, Log, TEXT("Player death montage not configured: %s"), *GetNameSafe(this));
		return;
	}

	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("Player death montage skipped: AnimInstance invalid for %s"), *GetNameSafe(this));
		return;
	}

	const float PlayResult = AnimInstance->Montage_Play(DeathMontage);
	UE_LOG(LogTemp, Log, TEXT("Player death montage played: Character=%s Montage=%s Result=%.3f"),
		*GetNameSafe(this),
		*GetNameSafe(DeathMontage),
		PlayResult);
}

FVector2D ACharacterBase::GetLocalMovementVector() const
{
	const FVector Velocity2D = FVector(GetVelocity().X, GetVelocity().Y, 0.0f);

	return FVector2D(
		FVector::DotProduct(Velocity2D, VisualRoot->GetRightVector()),
		FVector::DotProduct(Velocity2D, VisualRoot->GetForwardVector()));
}

float ACharacterBase::GetLocalForwardSpeed() const
{
	return GetLocalMovementVector().Y;
}

float ACharacterBase::GetLocalRightSpeed() const
{
	return GetLocalMovementVector().X;
}

void ACharacterBase::UpdateMouseFacing(float DeltaSeconds)
{
	if (CharacterMode != ECharacterMode::Active || (!bHasFacingTarget && !bHasFacingOverride) || !VisualRoot)
	{
		return;
	}

	const FVector TargetLocation = bHasFacingOverride ? FacingOverrideTarget : FacingTarget;
	FVector ToMouse = TargetLocation - GetActorLocation();
	ToMouse.Z = 0.0f;
	if (ToMouse.SizeSquared2D() <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FRotator CurrentRotation = VisualRoot->GetComponentRotation();
	const FRotator TargetRotation = FRotator(0.0f, ToMouse.Rotation().Yaw, 0.0f);
	const FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaSeconds, FacingRotationSpeed);

	VisualRoot->SetWorldRotation(FRotator(0.0f, NewRotation.Yaw, 0.0f));
}
