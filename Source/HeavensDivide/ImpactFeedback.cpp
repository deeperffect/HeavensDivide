#include "ImpactFeedback.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "SurvivorPlayerController.h"
#include "Shakes/PerlinNoiseCameraShakePattern.h"

USamuraiImpactCameraShake::USamuraiImpactCameraShake(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bSingleInstance = true;
	UPerlinNoiseCameraShakePattern* Pattern = CreateDefaultSubobject<UPerlinNoiseCameraShakePattern>(TEXT("ImpactPattern"));
	Pattern->Duration = 0.07f;
	Pattern->BlendInTime = 0.01f;
	Pattern->BlendOutTime = 0.04f;
	Pattern->X.Amplitude = 0.25f;
	Pattern->Y.Amplitude = 0.6f;
	Pattern->Z.Amplitude = 0.4f;
	// At the saved rig's 1600 cm distance, the unscaled amplitudes are subpixel.
	Pattern->LocationAmplitudeMultiplier = 10.0f;
	// Engine's Perlin constructor defaults this to zero, disabling Pitch/Yaw below.
	Pattern->RotationAmplitudeMultiplier = 1.0f;
	Pattern->X.Frequency = Pattern->Y.Frequency = Pattern->Z.Frequency = 35.0f;
	Pattern->Pitch.Amplitude = Pattern->Yaw.Amplitude = 0.015f;
	Pattern->Roll.Amplitude = Pattern->FOV.Amplitude = 0.0f;
	SetRootShakePattern(Pattern);
}

void UImpactFeedbackLibrary::PlayImpactFeedback(const UObject* WorldContextObject, const FImpactFeedbackData& Feedback,
	FVector Location, FVector Normal, bool bPlayCameraShake)
{
	if (!WorldContextObject || !WorldContextObject->GetWorld() || WorldContextObject->GetWorld()->GetNetMode() == NM_DedicatedServer) return;
	if (Feedback.HitNiagaraSystem)
	{
		const FQuat Rotation = (Feedback.bUseSurfaceNormalRotation && !Normal.IsNearlyZero() ? Normal.Rotation().Quaternion() : FQuat::Identity)
			* Feedback.NiagaraRotationOffset.Quaternion();
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(WorldContextObject, Feedback.HitNiagaraSystem, Location,
			Rotation.Rotator(), Feedback.NiagaraScale, true, true, ENCPoolMethod::AutoRelease);
	}
	if (Feedback.HitSound) UGameplayStatics::PlaySoundAtLocation(WorldContextObject, Feedback.HitSound, Location);
	if (bPlayCameraShake && Feedback.bEnableCameraShake)
		PlayGameplayCameraShake(WorldContextObject, Feedback.CameraShakeClass, Feedback.CameraShakeScale);
}

void UImpactFeedbackLibrary::PlayGameplayCameraShake(const UObject* WorldContextObject, TSubclassOf<UCameraShakeBase> ShakeClass, float BaseScale)
{
	if (ASurvivorPlayerController* PC = Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(WorldContextObject, 0)))
		PC->PlayGameplayCameraShake(ShakeClass, BaseScale);
}
