// Copyright Epic Games, Inc. All Rights Reserved.

#include "InactiveCharacterAssistComponent.h"

#include "AutoAttackComponent.h"
#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnemyBase.h"
#include "HAL/IConsoleManager.h"
#include "NinjaCharacter.h"
#include "PlayerUpgradeComponent.h"
#include "SurvivorPlayerController.h"
#include "TimerManager.h"
#include "UpgradeDefinition.h"

static TAutoConsoleVariable<int32> CVarHDLogSynergyAssist(
	TEXT("hd.LogSynergyAssist"),
	0,
	TEXT("Logs inactive character synergy assist events when enabled."));

UInactiveCharacterAssistComponent::UInactiveCharacterAssistComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UInactiveCharacterAssistComponent::BeginPlay()
{
	Super::BeginPlay();

	SurvivorController = Cast<ASurvivorPlayerController>(GetOwner());
	PlayerUpgrades = SurvivorController ? SurvivorController->GetPlayerUpgrades() : nullptr;

	if (PlayerUpgrades)
	{
		PlayerUpgrades->OnUpgradeAcquired.AddUniqueDynamic(this, &UInactiveCharacterAssistComponent::HandleUpgradeAcquired);
	}

	if (UCharacterManagerComponent* CharacterManager = SurvivorController ? SurvivorController->GetCharacterManager() : nullptr)
	{
		CharacterManager->OnCharacterSwapped.AddUniqueDynamic(this, &UInactiveCharacterAssistComponent::HandleCharacterSwapped);
	}

	RefreshAssistEffectState();
}

void UInactiveCharacterAssistComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAssistTimer();
	FinishCurrentAssist();

	if (PlayerUpgrades)
	{
		PlayerUpgrades->OnUpgradeAcquired.RemoveDynamic(this, &UInactiveCharacterAssistComponent::HandleUpgradeAcquired);
	}

	if (UCharacterManagerComponent* CharacterManager = SurvivorController ? SurvivorController->GetCharacterManager() : nullptr)
	{
		CharacterManager->OnCharacterSwapped.RemoveDynamic(this, &UInactiveCharacterAssistComponent::HandleCharacterSwapped);
	}

	Super::EndPlay(EndPlayReason);
}

void UInactiveCharacterAssistComponent::RefreshAssistEffectState()
{
	if (HasAssistUpgrade())
	{
		StartAssistTimer();
	}
	else
	{
		StopAssistTimer();
	}
}

void UInactiveCharacterAssistComponent::HandleUpgradeAcquired(UUpgradeDefinition* Upgrade, int32 NewLevel)
{
	RefreshAssistEffectState();
}

void UInactiveCharacterAssistComponent::HandleCharacterSwapped(ACharacterBase* OldCharacter, ACharacterBase* NewCharacter)
{
	if (bAssistActive && CurrentAssistCharacter == NewCharacter)
	{
		CurrentAssistCharacter = nullptr;
		bAssistActive = false;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(AssistCleanupTimerHandle);
		}

		if (CVarHDLogSynergyAssist.GetValueOnGameThread() != 0)
		{
			UE_LOG(LogTemp, Log, TEXT("[TagTeam] Assist character became active through swap; cleanup canceled."));
		}
	}
}

void UInactiveCharacterAssistComponent::StartAssistTimer()
{
	UWorld* World = GetWorld();
	if (!World || World->GetTimerManager().IsTimerActive(AssistTimerHandle))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		AssistTimerHandle,
		this,
		&UInactiveCharacterAssistComponent::HandleAssistTimerElapsed,
		FMath::Max(0.1f, AssistInterval),
		true);
}

void UInactiveCharacterAssistComponent::StopAssistTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AssistTimerHandle);
		World->GetTimerManager().ClearTimer(AssistCleanupTimerHandle);
	}
}

void UInactiveCharacterAssistComponent::HandleAssistTimerElapsed()
{
	if (bAssistActive || !HasAssistUpgrade())
	{
		return;
	}

	UCharacterManagerComponent* CharacterManager = SurvivorController ? SurvivorController->GetCharacterManager() : nullptr;
	ACharacterBase* ActiveCharacter = CharacterManager ? CharacterManager->GetActiveCharacter() : nullptr;
	ACharacterBase* AssistCharacter = CharacterManager ? CharacterManager->GetInactiveCharacter() : nullptr;
	if (!ActiveCharacter || !AssistCharacter || AssistCharacter->GetCharacterMode() != ECharacterMode::Inactive)
	{
		return;
	}

	UAutoAttackComponent* AssistAttack = AssistCharacter->FindComponentByClass<UAutoAttackComponent>();
	if (!AssistAttack)
	{
		return;
	}

	const bool bRangedAssist = AssistAttack->IsProjectileAttack();
	AEnemyBase* TargetEnemy = bRangedAssist
		? AssistAttack->FindAssistTarget()
		: AssistAttack->FindAssistTargetNearLocation(ActiveCharacter->GetActorLocation(), MaxMeleeAssistTargetDistance);
	if (!TargetEnemy)
	{
		if (!bRangedAssist && CVarHDLogSynergyAssist.GetValueOnGameThread() != 0)
		{
			UE_LOG(LogTemp, Log, TEXT("[Samurai Assist] ActivePlayerLocation=%s SearchRadius=%.0f No target within range. Assist skipped"),
				*ActiveCharacter->GetActorLocation().ToString(),
				MaxMeleeAssistTargetDistance);
		}
		return;
	}

	FVector AssistLocation = FVector::ZeroVector;
	if (bRangedAssist)
	{
		AssistLocation = GetRangedAssistLocation(ActiveCharacter, AssistCharacter);
	}
	else if (!TryFindMeleeAssistLocation(AssistCharacter, ActiveCharacter, TargetEnemy, AssistLocation))
	{
		if (CVarHDLogSynergyAssist.GetValueOnGameThread() != 0)
		{
			UE_LOG(LogTemp, Log, TEXT("[TagTeam] AssistType=Melee Target=%s PlacementResult=Failed Assist skipped"),
				*GetNameSafe(TargetEnemy));
		}
		return;
	}

	AssistCharacter->SetActorLocation(AssistLocation, false, nullptr, ETeleportType::TeleportPhysics);
	AssistCharacter->SetCharacterMode(ECharacterMode::Assisting);

	if (!bRangedAssist)
	{
		FVector ToTarget = TargetEnemy->GetActorLocation() - AssistCharacter->GetActorLocation();
		ToTarget.Z = 0.0f;
		if (ToTarget.Normalize())
		{
			AssistCharacter->SetVisualFacingRotation(FRotator(0.0f, ToTarget.Rotation().Yaw, 0.0f));
		}

		if (!AssistAttack->IsTargetInCurrentMeleeReach(TargetEnemy))
		{
			AssistCharacter->SetCharacterMode(ECharacterMode::Inactive);
			if (CVarHDLogSynergyAssist.GetValueOnGameThread() != 0)
			{
				UE_LOG(LogTemp, Log, TEXT("[TagTeam] AssistType=Melee Target=%s PlacementResult=OutOfRange Assist skipped"),
					*GetNameSafe(TargetEnemy));
			}
			return;
		}
	}

	float ExpectedDuration = 0.0f;
	if (!AssistAttack->TryStartAssistAttackAtTarget(TargetEnemy, ExpectedDuration))
	{
		AssistCharacter->SetCharacterMode(ECharacterMode::Inactive);
		return;
	}

	CurrentAssistCharacter = AssistCharacter;
	bAssistActive = true;
	OnAssistStarted.Broadcast(AssistCharacter, ActiveCharacter, TargetEnemy);
	OnAssistAttackExecuted.Broadcast(AssistCharacter, ActiveCharacter, TargetEnemy);

	if (CVarHDLogSynergyAssist.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[TagTeam] Active: %s Inactive: %s Target: %s Assist started"),
			*GetNameSafe(ActiveCharacter),
			*GetNameSafe(AssistCharacter),
			*GetNameSafe(TargetEnemy));
		if (!bRangedAssist)
		{
			const AEnemyBase* PreviousDebugTarget = LastDebugMeleeAssistTarget.Get();
			UE_LOG(LogTemp, Log, TEXT("[Samurai Assist] ActivePlayerLocation=%s SearchRadius=%.0f SelectedTarget=%s TargetDistanceFromPlayer=%.1f CurrentTargetLocation=%s CalculatedAssistLocation=%s PreviousTarget=%s TargetChanged=%s"),
				*ActiveCharacter->GetActorLocation().ToString(),
				MaxMeleeAssistTargetDistance,
				*GetNameSafe(TargetEnemy),
				FVector::Dist2D(ActiveCharacter->GetActorLocation(), TargetEnemy->GetActorLocation()),
				*TargetEnemy->GetActorLocation().ToString(),
				*AssistLocation.ToString(),
				*GetNameSafe(PreviousDebugTarget),
				PreviousDebugTarget != TargetEnemy ? TEXT("true") : TEXT("false"));
		}
	}
	if (!bRangedAssist)
	{
		LastDebugMeleeAssistTarget = TargetEnemy;
	}

	const float CleanupDelay = FMath::Max(MinimumAssistVisibleDuration, ExpectedDuration);
	GetWorld()->GetTimerManager().SetTimer(
		AssistCleanupTimerHandle,
		this,
		&UInactiveCharacterAssistComponent::FinishCurrentAssist,
		CleanupDelay,
		false);
}

void UInactiveCharacterAssistComponent::FinishCurrentAssist()
{
	ACharacterBase* AssistCharacter = CurrentAssistCharacter;
	CurrentAssistCharacter = nullptr;
	bAssistActive = false;

	if (!AssistCharacter)
	{
		return;
	}

	if (AssistCharacter->GetCharacterMode() == ECharacterMode::Assisting)
	{
		if (UAutoAttackComponent* AssistAttack = AssistCharacter->FindComponentByClass<UAutoAttackComponent>())
		{
			AssistAttack->StopAutoAttack();
		}
		AssistCharacter->SetCharacterMode(ECharacterMode::Inactive);
	}

	OnAssistEnded.Broadcast(AssistCharacter);

	if (CVarHDLogSynergyAssist.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[TagTeam] Assist ended %s returned to inactive state"), *GetNameSafe(AssistCharacter));
	}
}

bool UInactiveCharacterAssistComponent::HasAssistUpgrade() const
{
	return PlayerUpgrades && PlayerUpgrades->GetSpecialEffectLevel(EUpgradeSpecialEffect::InactiveCharacterAssist) > 0;
}

FVector UInactiveCharacterAssistComponent::GetRangedAssistLocation(const ACharacterBase* ActiveCharacter, const ACharacterBase* AssistCharacter) const
{
	if (!ActiveCharacter || !AssistCharacter)
	{
		return FVector::ZeroVector;
	}

	const FVector LocalOffset = AssistCharacter->IsA<ANinjaCharacter>() ? NinjaAssistOffset : SamuraiAssistOffset;
	const FRotator ActiveFacing = ActiveCharacter->GetVisualFacingRotation();
	return ActiveCharacter->GetActorLocation()
		+ ActiveFacing.RotateVector(FVector(LocalOffset.X, LocalOffset.Y, 0.0f));
}

bool UInactiveCharacterAssistComponent::TryFindMeleeAssistLocation(ACharacterBase* AssistCharacter, const ACharacterBase* ActiveCharacter, const AEnemyBase* TargetEnemy, FVector& OutAssistLocation) const
{
	if (!AssistCharacter || !ActiveCharacter || !TargetEnemy)
	{
		return false;
	}

	FVector DirectionFromTargetToAssist = ActiveCharacter->GetActorLocation() - TargetEnemy->GetActorLocation();
	DirectionFromTargetToAssist.Z = 0.0f;
	if (!DirectionFromTargetToAssist.Normalize())
	{
		DirectionFromTargetToAssist = -TargetEnemy->GetActorForwardVector();
		DirectionFromTargetToAssist.Z = 0.0f;
		DirectionFromTargetToAssist.Normalize();
	}

	constexpr float CandidateAngles[] = { 0.0f, 45.0f, -45.0f, 90.0f, -90.0f };
	const FVector TargetLocation = TargetEnemy->GetActorLocation();
	for (const float CandidateAngle : CandidateAngles)
	{
		const FVector CandidateDirection = DirectionFromTargetToAssist.RotateAngleAxis(CandidateAngle, FVector::UpVector).GetSafeNormal();
		FVector CandidateLocation = TargetLocation + CandidateDirection * MeleeAssistDistance;
		CandidateLocation.Z = AssistCharacter->GetActorLocation().Z;

		if (!IsAssistLocationBlocked(AssistCharacter, CandidateLocation))
		{
			OutAssistLocation = CandidateLocation;
			return true;
		}
	}

	return false;
}

bool UInactiveCharacterAssistComponent::IsAssistLocationBlocked(const ACharacterBase* AssistCharacter, const FVector& AssistLocation) const
{
	const UWorld* World = GetWorld();
	const UCapsuleComponent* Capsule = AssistCharacter ? AssistCharacter->GetCapsuleComponent() : nullptr;
	if (!World || !Capsule)
	{
		return true;
	}

	const FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TagTeamAssistPlacement), false, AssistCharacter);
	return World->OverlapBlockingTestByChannel(
		AssistLocation,
		FQuat::Identity,
		ECC_WorldStatic,
		CapsuleShape,
		QueryParams);
}
