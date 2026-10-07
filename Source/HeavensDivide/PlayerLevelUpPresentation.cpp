#include "SurvivorPlayerController.h"
#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "CombatAudio.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

void ASurvivorPlayerController::BeginLevelUpPresentation()
{
	if (bLevelUpPresentationActive || !GetWorld() || !IsRunInProgress() || bIsPlayerDead) return;

	bLevelUpPresentationActive = true;
	LevelUpPresentationRemaining = FMath::IsFinite(LevelUpPresentationDuration)
		? FMath::Max(0.0f, LevelUpPresentationDuration) : 0.0f;
	CloseLevelUpWidget();
	LevelUpAudio = UCombatAudioLibrary::PlayEvent(this, TEXT("LevelUp"), FVector::ZeroVector, true);

	ACharacterBase* ActiveCharacter = CharacterManager ? CharacterManager->GetActiveCharacter() : nullptr;
	if (LevelUpVFX && ActiveCharacter && LevelUpPresentationRemaining > 0.0f)
	{
		const FVector Feet = ActiveCharacter->GetActorLocation()
			- FVector(0.0f, 0.0f, ActiveCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		LevelUpEffect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), LevelUpVFX,
			Feet + LevelUpVFXOffset, FRotator::ZeroRotator,
			FVector(FMath::Max(0.01f, LevelUpVFXScale)), false, false, ENCPoolMethod::None, false);
		if (LevelUpEffect)
		{
			// Follow movement without inheriting character facing or scale. Keep the
			// world-settings owner so swapping away cannot hide the burst.
			LevelUpEffect->SetAbsolute(false, true, true);
			LevelUpEffect->AttachToComponent(ActiveCharacter->GetRootComponent(),
				FAttachmentTransformRules::KeepWorldTransform);
			// Keep the burst in sync with its real-time delay during combat slow motion.
			LevelUpEffect->SetForceSolo(true);
			LevelUpEffect->Activate(true);
			LevelUpEffect->SetComponentTickEnabled(false);
		}
	}

	if (LevelUpPresentationRemaining <= 0.0f) UpdateLevelUpPresentation(0.0f);
}

void ASurvivorPlayerController::UpdateLevelUpPresentation(float RealDeltaTime)
{
	if (!bLevelUpPresentationActive) return;
	if (!GetWorld() || !IsRunInProgress() || bIsPlayerDead)
	{
		CancelLevelUpPresentation();
		return;
	}
	if (UGameplayStatics::IsGamePaused(this)) return;

	const float Step = FMath::IsFinite(RealDeltaTime) ? FMath::Max(0.0f, RealDeltaTime) : 0.0f;
	if (IsValid(LevelUpEffect))
	{
		if (ACharacterBase* ActiveCharacter = CharacterManager ? CharacterManager->GetActiveCharacter() : nullptr)
		{
			if (LevelUpEffect->GetAttachParent() != ActiveCharacter->GetRootComponent())
			{
				LevelUpEffect->AttachToComponent(ActiveCharacter->GetRootComponent(),
					FAttachmentTransformRules::KeepWorldTransform);
			}
			const FVector Feet = ActiveCharacter->GetActorLocation()
				- FVector(0.0f, 0.0f, ActiveCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
			LevelUpEffect->SetWorldLocation(Feet + LevelUpVFXOffset);
		}
		if (Step > 0.0f) LevelUpEffect->TickComponent(Step, LEVELTICK_All, nullptr);
	}
	LevelUpPresentationRemaining -= Step;
	if (LevelUpPresentationRemaining <= 0.0f)
	{
		// Let the sound's tail finish even when the menu is ready to open.
		LevelUpAudio = nullptr;
		CancelLevelUpPresentation();
		// Reserve selection input only now, when the menu is about to open.
		bLevelUpSelectionActive = true;
		StartNextUpgradeSelection();
	}
}

void ASurvivorPlayerController::CancelLevelUpPresentation()
{
	bLevelUpPresentationActive = false;
	LevelUpPresentationRemaining = 0.0f;
	if (IsValid(LevelUpEffect)) LevelUpEffect->DestroyComponent();
	LevelUpEffect = nullptr;
	if (IsValid(LevelUpAudio)) LevelUpAudio->Stop();
	LevelUpAudio = nullptr;
}
