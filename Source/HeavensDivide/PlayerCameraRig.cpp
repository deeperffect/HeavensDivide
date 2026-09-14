// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlayerCameraRig.h"

#include "Camera/CameraComponent.h"
#include "CharacterBase.h"
#include "Components/SceneComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "SwapPresentationComponent.h"
#include "Misc/App.h"

APlayerCameraRig::APlayerCameraRig()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;

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
	CameraBoom->AddTickPrerequisiteActor(this);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom);
	Camera->bUsePawnControlRotation = false;
}

void APlayerCameraRig::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if(bSwapFocusActive)
	{
		UpdateSwapFocus(static_cast<float>(FApp::GetDeltaTime()));
	}
	else if (IsValid(FollowTarget))
	{
		SetActorLocation(FollowTarget->GetActorLocation());
	}
}

void APlayerCameraRig::SetFollowTarget(ACharacterBase* NewFollowTarget)
{
	if(FollowTarget!=NewFollowTarget) StopSwapFocus();
	if(IsValid(FollowTarget) && FollowTarget->SwapPresentation)
		RemoveTickPrerequisiteComponent(FollowTarget->SwapPresentation);
	FollowTarget = NewFollowTarget;
	if(IsValid(FollowTarget) && FollowTarget->SwapPresentation)
		AddTickPrerequisiteComponent(FollowTarget->SwapPresentation);

	if (FollowTarget)
	{
		SetActorLocation(FollowTarget->GetActorLocation());
	}
}

ACharacterBase* APlayerCameraRig::GetFollowTarget() const
{
	return FollowTarget;
}

void APlayerCameraRig::StartSwapFocus()
{
	StopSwapFocus();
	if(!bEnableSwapFocus || !IsValid(FollowTarget) || !Camera || !CameraBoom) return;
	OriginalFOV=Camera->FieldOfView;
	OriginalOrthoWidth=Camera->OrthoWidth;
	OriginalVignette=Camera->PostProcessSettings.VignetteIntensity;
	bOriginalVignetteOverride=Camera->PostProcessSettings.bOverride_VignetteIntensity;
	bOriginalCameraLag=CameraBoom->bEnableCameraLag;
	CameraBoom->bEnableCameraLag=false;
	SwapFocusElapsed=0;
	bSwapFocusActive=true;
	UpdateSwapFocus(0);
}

void APlayerCameraRig::UpdateSwapFocus(float RealDelta)
{
	if(!bSwapFocusActive) return;
	if(!bEnableSwapFocus || !IsValid(FollowTarget)) { StopSwapFocus(); return; }
	SwapFocusElapsed+=FMath::Max(0.f,RealDelta);
	const float In=FMath::Max(.01f,SwapZoomInDuration);
	const float Out=FMath::Max(.01f,SwapZoomOutDuration);
	if(SwapFocusElapsed>=In+Out) { StopSwapFocus(); return; }
	const bool bZoomingIn=SwapFocusElapsed<In;
	const float Alpha=FMath::Clamp(bZoomingIn ? SwapFocusElapsed/In : (SwapFocusElapsed-In)/Out,0.f,1.f);
	const float Smooth=Alpha*Alpha*(3.f-2.f*Alpha);
	const float Weight=bZoomingIn ? Smooth : 1.f-Smooth;
	Camera->SetFieldOfView(FMath::Lerp(OriginalFOV,FMath::Max(5.f,OriginalFOV*(1.f-FMath::Clamp(SwapZoomAmount,0.f,.5f))),Weight));
	Camera->SetOrthoWidth(OriginalOrthoWidth*(1.f-FMath::Clamp(SwapZoomAmount,0.f,.5f)*Weight));
	if(SwapVignetteStrength>0)
	{
		Camera->PostProcessSettings.bOverride_VignetteIntensity=true;
		Camera->PostProcessSettings.VignetteIntensity=FMath::Clamp(OriginalVignette+SwapVignetteStrength*Weight,0.f,1.f);
	}
	// Follow the arriving visual, then blend back to the normal actor anchor.
	// Sampling its current position also follows movement after the freeze ends.
	const FVector Anchor=FollowTarget->GetActorLocation();
	const FVector Visual=FollowTarget->GetVisualRoot() ? FollowTarget->GetVisualRoot()->GetComponentLocation() : Anchor;
	SetActorLocation(FMath::Lerp(Anchor,Visual,Weight));
}

void APlayerCameraRig::StopSwapFocus()
{
	if(!bSwapFocusActive) return;
	bSwapFocusActive=false;
	Camera->SetFieldOfView(OriginalFOV);
	Camera->SetOrthoWidth(OriginalOrthoWidth);
	Camera->PostProcessSettings.VignetteIntensity=OriginalVignette;
	Camera->PostProcessSettings.bOverride_VignetteIntensity=bOriginalVignetteOverride;
	CameraBoom->bEnableCameraLag=bOriginalCameraLag;
	if(IsValid(FollowTarget)) SetActorLocation(FollowTarget->GetActorLocation());
}

void APlayerCameraRig::EndPlay(const EEndPlayReason::Type Reason)
{
	StopSwapFocus();
	Super::EndPlay(Reason);
}
