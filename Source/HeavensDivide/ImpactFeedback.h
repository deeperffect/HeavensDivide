#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Camera/CameraShakeBase.h"
#include "ImpactFeedback.generated.h"

class UNiagaraSystem;
class USoundBase;
class AEnemyBase;

USTRUCT(BlueprintType)
struct HEAVENSDIVIDE_API FImpactFeedbackData
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Impact") TObjectPtr<UNiagaraSystem> HitNiagaraSystem;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Impact") TObjectPtr<USoundBase> HitSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Impact") FVector NiagaraScale = FVector::OneVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Impact") FRotator NiagaraRotationOffset = FRotator::ZeroRotator;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Impact") bool bUseSurfaceNormalRotation = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Impact") bool bEnableCameraShake = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Impact") TSubclassOf<UCameraShakeBase> CameraShakeClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Impact", meta=(ClampMin="0")) float CameraShakeScale = 1.0f;
};

/** Native starting preset; derive a Blueprint to edit the pattern's timing and amplitudes. */
UCLASS(Blueprintable)
class HEAVENSDIVIDE_API USamuraiImpactCameraShake : public UCameraShakeBase
{
	GENERATED_BODY()
public:
	USamuraiImpactCameraShake(const FObjectInitializer& ObjectInitializer);
};

UCLASS()
class HEAVENSDIVIDE_API UImpactFeedbackLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** Use bPlayCameraShake=false for each target of a multi-target attack, then request one shake. */
	UFUNCTION(BlueprintCallable, Category="Combat|Impact", meta=(WorldContext="WorldContextObject"))
	static void PlayImpactFeedback(const UObject* WorldContextObject, const FImpactFeedbackData& Feedback,
		FVector Location, FVector Normal, bool bPlayCameraShake = true);
	UFUNCTION(BlueprintCallable, Category="Combat|Impact", meta=(WorldContext="WorldContextObject"))
	static void PlayGameplayCameraShake(const UObject* WorldContextObject, TSubclassOf<UCameraShakeBase> ShakeClass, float BaseScale = 1.0f);
};
