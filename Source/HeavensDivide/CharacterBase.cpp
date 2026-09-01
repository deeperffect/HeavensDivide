// Copyright Epic Games, Inc. All Rights Reserved.

#include "CharacterBase.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "CharacterStatsComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ACharacterBase::ACharacterBase()
{
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = false;

	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(RootComponent);
	GetMesh()->SetupAttachment(VisualRoot);

	CharacterStatsComponent = CreateDefaultSubobject<UCharacterStatsComponent>(TEXT("CharacterStatsComponent"));
}

void ACharacterBase::BeginPlay()
{
	Super::BeginPlay();

	if (VisualRoot && GetMesh() && GetMesh()->GetAttachParent() != VisualRoot)
	{
		GetMesh()->AttachToComponent(VisualRoot, FAttachmentTransformRules::KeepRelativeTransform);
	}

	if (const UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		BaseMaxWalkSpeed = MovementComponent->MaxWalkSpeed;
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

UCharacterStatsComponent* ACharacterBase::GetCharacterStats() const
{
	return CharacterStatsComponent;
}

void ACharacterBase::ApplySharedMoveSpeedMultiplier(float MoveSpeedMultiplier)
{
	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	if (!MovementComponent)
	{
		return;
	}

	if (BaseMaxWalkSpeed <= 0.0f)
	{
		BaseMaxWalkSpeed = MovementComponent->MaxWalkSpeed;
	}

	MovementComponent->MaxWalkSpeed = BaseMaxWalkSpeed * FMath::Max(0.0f, MoveSpeedMultiplier);
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

void ACharacterBase::StartDashVisual(float GameplayDashDuration, FVector DashDirection)
{
	bIsDashing = true;

	DashDirection.Z = 0.0f;
	if (bFaceDashDirection && DashDirection.Normalize())
	{
		SetVisualFacingRotation(FRotator(0.0f, DashDirection.Rotation().Yaw, 0.0f));
	}

	if (!DashMontage)
	{
		return;
	}

	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("Dash montage skipped: AnimInstance invalid for %s"), *GetNameSafe(this));
		return;
	}

	const float MontageLength = DashMontage->GetPlayLength();
	const float DesiredPlayRate = GameplayDashDuration > KINDA_SMALL_NUMBER ? MontageLength / GameplayDashDuration : 1.0f;
	const float PlayRate = FMath::Clamp(DesiredPlayRate, MinDashMontagePlayRate, FMath::Max(MinDashMontagePlayRate, MaxDashMontagePlayRate));
	const float PlayResult = AnimInstance->Montage_Play(DashMontage, PlayRate);
	UE_LOG(LogTemp, Log, TEXT("Dash montage played: Character=%s Montage=%s PlayRate=%.2f Result=%.3f"),
		*GetNameSafe(this),
		*GetNameSafe(DashMontage),
		PlayRate,
		PlayResult);
}

void ACharacterBase::EndDashVisual()
{
	bIsDashing = false;
}

bool ACharacterBase::IsDashing() const
{
	return bIsDashing;
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
	if (bIsDashing && bFaceDashDirection)
	{
		return;
	}

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
