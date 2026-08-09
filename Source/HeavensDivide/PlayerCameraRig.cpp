// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlayerCameraRig.h"

#include "Camera/CameraComponent.h"
#include "CharacterBase.h"
#include "Components/SceneComponent.h"
#include "GameFramework/SpringArmComponent.h"

APlayerCameraRig::APlayerCameraRig()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(Root);
	CameraBoom->TargetArmLength = 900.0f;
	CameraBoom->SetRelativeRotation(FRotator(-60.0f, 180.0f, 0.0f));
	CameraBoom->SetUsingAbsoluteRotation(true);
	CameraBoom->bInheritPitch = false;
	CameraBoom->bInheritYaw = false;
	CameraBoom->bInheritRoll = false;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bUsePawnControlRotation = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom);
	Camera->bUsePawnControlRotation = false;
}

void APlayerCameraRig::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (FollowTarget)
	{
		SetActorLocation(FollowTarget->GetActorLocation());
	}
}

void APlayerCameraRig::SetFollowTarget(ACharacterBase* NewFollowTarget)
{
	FollowTarget = NewFollowTarget;

	if (FollowTarget)
	{
		SetActorLocation(FollowTarget->GetActorLocation());
	}
}

ACharacterBase* APlayerCameraRig::GetFollowTarget() const
{
	return FollowTarget;
}
