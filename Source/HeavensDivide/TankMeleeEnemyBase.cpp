// Copyright Epic Games, Inc. All Rights Reserved.

#include "TankMeleeEnemyBase.h"

#include "AnimNotify_EnemyAttackHit.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "CharacterBase.h"
#include "CharacterManagerComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "DrawDebugHelpers.h"
#include "HealthComponent.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "SurvivorPlayerController.h"
#include "TimerManager.h"

static TAutoConsoleVariable<int32> CVarHDDebugTankSlam(
	TEXT("hd.DebugTankSlam"),
	0,
	TEXT("Logs and draws tank slam attack start/impact events when enabled."));

ATankMeleeEnemyBase::ATankMeleeEnemyBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AttackAoERadius = 375.0f;
	AttackShape = ETankSlamAttackShape::Box;
	AttackBoxLength = 600.0f;
	AttackBoxWidth = 160.0f;
	AttackBoxForwardOffset = 300.0f;
	SlamDamageMultiplier = 1.0f;
	WindupTrackingRotationSpeed = 120.0f;
	TelegraphWindupDuration = 1.0f;
	TelegraphFillUpdateInterval = 0.025f;

	AttackTelegraphDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("AttackTelegraphDecal"));
	AttackTelegraphDecal->SetupAttachment(RootComponent);
	AttackTelegraphDecal->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	AttackTelegraphDecal->SetComponentTickEnabled(false);
	AttackTelegraphDecal->SetHiddenInGame(true);
	AttackTelegraphDecal->SetVisibility(false);
}

void ATankMeleeEnemyBase::BeginPlay()
{
	Super::BeginPlay();

	InitializeTelegraphMaterialInstance();
	UpdateAttackTelegraphSizeAndPlacement();
	HideAttackTelegraph();
}

void ATankMeleeEnemyBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bIsAttacking && bTrackPlayerDuringAttackWindup && !bAttackFacingLocked)
	{
		UpdateWindupFacing(DeltaSeconds);
	}
}

void ATankMeleeEnemyBase::CommitSlamFacing()
{
	LockAttackFacing();
}

void ATankMeleeEnemyBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearAttackFacingState();
	StopTelegraphFill();
	HideAttackTelegraph();

	Super::EndPlay(EndPlayReason);
}

void ATankMeleeEnemyBase::HandleDeath()
{
	ClearAttackFacingState();
	StopTelegraphFill();
	HideAttackTelegraph();

	Super::HandleDeath();
}

void ATankMeleeEnemyBase::UpdateEnemyBehavior(float DeltaSeconds)
{
	if (bIsAttacking)
	{
		StopEnemyMovement();
		return;
	}

	Super::UpdateEnemyBehavior(DeltaSeconds);
}

void ATankMeleeEnemyBase::StopEnemyBehavior()
{
	ClearAttackFacingState();
	StopTelegraphFill();
	HideAttackTelegraph();

	Super::StopEnemyBehavior();
}

void ATankMeleeEnemyBase::HandleAttackCommitted()
{
	Super::HandleAttackCommitted();
	StartWindupFacingTracking();
	ShowAttackTelegraph();
	StartTelegraphFill();
}

void ATankMeleeEnemyBase::HandleAttackFinished()
{
	ClearAttackFacingState();
	StopTelegraphFill();
	HideAttackTelegraph();
	Super::HandleAttackFinished();
}

void ATankMeleeEnemyBase::ExecuteAttackHit()
{
	LockAttackFacing();
	SetTelegraphFillAmount(1.0f);
	StopTelegraphFill();
	if (CVarHDDebugTankSlam.GetValueOnGameThread() != 0 && GetWorld())
	{
		const float ElapsedWindup = TelegraphFillStartTime > 0.0
			? static_cast<float>(GetWorld()->GetTimeSeconds() - TelegraphFillStartTime)
			: 0.0f;
		UE_LOG(LogTemp, Log, TEXT("[TankSlam] Impact FillAmount=1.0 ElapsedWindup=%.3f EffectiveWindupDuration=%.3f"),
			ElapsedWindup,
			ActiveTelegraphFillDuration);
	}

	if (bIsDead || IsPlayerTargetDead() || !ObservedCharacterManager)
	{
		HideAttackTelegraph();
		return;
	}

	ACharacterBase* ActivePlayerCharacter = ObservedCharacterManager->GetActiveCharacter();
	if (!ActivePlayerCharacter)
	{
		HideAttackTelegraph();
		return;
	}

	ASurvivorPlayerController* SurvivorController = Cast<ASurvivorPlayerController>(ActivePlayerCharacter->GetController());
	if (!SurvivorController)
	{
		SurvivorController = Cast<ASurvivorPlayerController>(ActivePlayerCharacter->GetOwner());
	}

	UHealthComponent* TargetHealth = SurvivorController ? SurvivorController->GetPlayerHealthComponent() : nullptr;
	if (!TargetHealth || TargetHealth->IsDead())
	{
		HideAttackTelegraph();
		return;
	}

	float PlayerDistance = 0.0f;
	const bool bHitPlayer = IsPlayerInsideSlam(ActivePlayerCharacter, PlayerDistance);
	const float SlamDamage = AttackDamage * SlamDamageMultiplier;

	if (CVarHDDebugTankSlam.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[TankSlam] Impact Tank=%s Shape=%s Radius=%.1f BoxLength=%.1f BoxWidth=%.1f BoxForwardOffset=%.1f Player=%s PlayerDistance=%.1f Hit=%s Damage=%.2f"),
			*GetNameSafe(this),
			AttackShape == ETankSlamAttackShape::Circle ? TEXT("Circle") : TEXT("Box"),
			AttackAoERadius,
			AttackBoxLength,
			AttackBoxWidth,
			AttackBoxForwardOffset,
			*GetNameSafe(ActivePlayerCharacter),
			PlayerDistance,
			bHitPlayer ? TEXT("true") : TEXT("false"),
			SlamDamage);

		if (GetWorld())
		{
			if (AttackShape == ETankSlamAttackShape::Circle)
			{
				DrawDebugSphere(GetWorld(), GetActorLocation(), AttackAoERadius, 48, bHitPlayer ? FColor::Red : FColor::Yellow, false, 1.5f, 0, 3.0f);
			}
			else
			{
				DrawDebugBox(
					GetWorld(),
					GetBoxSlamCenter(),
					FVector(FMath::Max(0.0f, AttackBoxLength) * 0.5f, FMath::Max(0.0f, AttackBoxWidth) * 0.5f, 80.0f),
					GetActorQuat(),
					bHitPlayer ? FColor::Red : FColor::Yellow,
					false,
					1.5f,
					0,
					3.0f);
			}
		}
	}

	if (bHitPlayer && SlamDamage > 0.0f)
	{
		TargetHealth->ApplyDamage(SlamDamage);
		UE_LOG(LogTemp, Log, TEXT("Tank slam damaged player: Target=%s Damage=%.2f RemainingHealth=%.2f"),
			*GetNameSafe(ActivePlayerCharacter),
			SlamDamage,
			TargetHealth->GetCurrentHealth());
	}

	HideAttackTelegraph();
}

void ATankMeleeEnemyBase::ShowAttackTelegraph()
{
	InitializeTelegraphMaterialInstance();
	UpdateAttackTelegraphSizeAndPlacement();
	SetTelegraphFillAmount(0.0f);

	if (AttackTelegraphDecal)
	{
		AttackTelegraphDecal->SetHiddenInGame(false);
		AttackTelegraphDecal->SetVisibility(true);
	}

	if (CVarHDDebugTankSlam.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[TankSlam] AttackStarted Tank=%s Shape=%s Radius=%.1f BoxLength=%.1f BoxWidth=%.1f BoxForwardOffset=%.1f"),
			*GetNameSafe(this),
			AttackShape == ETankSlamAttackShape::Circle ? TEXT("Circle") : TEXT("Box"),
			AttackAoERadius,
			AttackBoxLength,
			AttackBoxWidth,
			AttackBoxForwardOffset);

		if (GetWorld())
		{
			if (AttackShape == ETankSlamAttackShape::Circle)
			{
				DrawDebugSphere(GetWorld(), GetActorLocation(), AttackAoERadius, 48, FColor::Orange, false, 1.5f, 0, 2.0f);
			}
			else
			{
				DrawDebugBox(
					GetWorld(),
					GetBoxSlamCenter(),
					FVector(FMath::Max(0.0f, AttackBoxLength) * 0.5f, FMath::Max(0.0f, AttackBoxWidth) * 0.5f, 80.0f),
					GetActorQuat(),
					FColor::Orange,
					false,
					1.5f,
					0,
					2.0f);
			}
		}
	}
}

void ATankMeleeEnemyBase::HideAttackTelegraph()
{
	ResetTelegraphFill();
	if (AttackTelegraphDecal)
	{
		AttackTelegraphDecal->SetHiddenInGame(true);
		AttackTelegraphDecal->SetVisibility(false);
	}
}

void ATankMeleeEnemyBase::InitializeTelegraphMaterialInstance()
{
	if (!AttackTelegraphDecal || TelegraphMaterialInstance)
	{
		return;
	}

	if (AttackTelegraphMaterial)
	{
		TelegraphMaterialInstance = UMaterialInstanceDynamic::Create(AttackTelegraphMaterial, this);
		if (TelegraphMaterialInstance)
		{
			AttackTelegraphDecal->SetDecalMaterial(TelegraphMaterialInstance);
			SetTelegraphFillAmount(0.0f);
		}
	}
}

void ATankMeleeEnemyBase::StartTelegraphFill()
{
	InitializeTelegraphMaterialInstance();
	SetTelegraphFillAmount(0.0f);

	UWorld* World = GetWorld();
	if (!World || TelegraphWindupDuration <= KINDA_SMALL_NUMBER)
	{
		SetTelegraphFillAmount(1.0f);
		return;
	}

	TelegraphFillStartTime = World->GetTimeSeconds();
	float ImpactNotifyTime = -1.0f;
	float MontagePosition = 0.0f;
	float MontagePlayRate = 1.0f;
	ActiveTelegraphFillDuration = CalculateTelegraphFillDuration(ImpactNotifyTime, MontagePosition, MontagePlayRate);

	if (CVarHDDebugTankSlam.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[TankSlam] Telegraph timing Montage=%s ImpactNotifyTime=%.3f MontagePosition=%.3f PlayRate=%.3f EffectiveWindupDuration=%.3f FallbackDuration=%.3f"),
			*GetNameSafe(AttackMontage),
			ImpactNotifyTime,
			MontagePosition,
			MontagePlayRate,
			ActiveTelegraphFillDuration,
			TelegraphWindupDuration);
	}

	World->GetTimerManager().ClearTimer(TelegraphFillTimerHandle);
	World->GetTimerManager().SetTimer(
		TelegraphFillTimerHandle,
		this,
		&ATankMeleeEnemyBase::HandleTelegraphFillTimerElapsed,
		FMath::Max(0.01f, TelegraphFillUpdateInterval),
		true);
}

void ATankMeleeEnemyBase::StopTelegraphFill()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TelegraphFillTimerHandle);
	}
}

void ATankMeleeEnemyBase::ResetTelegraphFill()
{
	SetTelegraphFillAmount(0.0f);
	TelegraphFillStartTime = 0.0;
	ActiveTelegraphFillDuration = FMath::Max(0.01f, TelegraphWindupDuration);
}

void ATankMeleeEnemyBase::SetTelegraphFillAmount(float FillAmount)
{
	if (TelegraphMaterialInstance)
	{
		TelegraphMaterialInstance->SetScalarParameterValue(TEXT("FillAmount"), FMath::Clamp(FillAmount, 0.0f, 1.0f));
	}
}

void ATankMeleeEnemyBase::HandleTelegraphFillTimerElapsed()
{
	const UWorld* World = GetWorld();
	if (!World || !bIsAttacking || bIsDead)
	{
		StopTelegraphFill();
		return;
	}

	const float Elapsed = static_cast<float>(World->GetTimeSeconds() - TelegraphFillStartTime);
	const float FillAmount = ActiveTelegraphFillDuration > KINDA_SMALL_NUMBER
		? FMath::Clamp(Elapsed / ActiveTelegraphFillDuration, 0.0f, 0.99f)
		: 1.0f;
	SetTelegraphFillAmount(FillAmount);

	if (FillAmount >= 0.99f)
	{
		StopTelegraphFill();
	}
}

float ATankMeleeEnemyBase::CalculateTelegraphFillDuration(float& OutImpactNotifyTime, float& OutMontagePosition, float& OutMontagePlayRate) const
{
	OutImpactNotifyTime = -1.0f;
	OutMontagePosition = 0.0f;
	OutMontagePlayRate = 1.0f;

	const USkeletalMeshComponent* MeshComponent = GetMesh();
	const UAnimInstance* AnimInstance = MeshComponent ? MeshComponent->GetAnimInstance() : nullptr;
	if (AnimInstance && AttackMontage)
	{
		OutMontagePosition = AnimInstance->Montage_GetPosition(AttackMontage);
		OutMontagePlayRate = FMath::Abs(AnimInstance->Montage_GetPlayRate(AttackMontage));
		if (FindImpactNotifyTime(OutMontagePosition, OutImpactNotifyTime) && OutMontagePlayRate > KINDA_SMALL_NUMBER)
		{
			const float SourceSecondsUntilImpact = FMath::Max(0.0f, OutImpactNotifyTime - OutMontagePosition);
			if (SourceSecondsUntilImpact > KINDA_SMALL_NUMBER)
			{
				return SourceSecondsUntilImpact / OutMontagePlayRate;
			}
		}
	}

	return FMath::Max(0.01f, TelegraphWindupDuration);
}

bool ATankMeleeEnemyBase::FindImpactNotifyTime(float MontagePosition, float& OutImpactNotifyTime) const
{
	OutImpactNotifyTime = -1.0f;
	if (!AttackMontage)
	{
		return false;
	}

	bool bFoundNotify = false;
	float BestNotifyTime = TNumericLimits<float>::Max();
	for (const FAnimNotifyEvent& NotifyEvent : AttackMontage->Notifies)
	{
		if (!NotifyEvent.Notify || !NotifyEvent.Notify->IsA<UAnimNotify_EnemyAttackHit>())
		{
			continue;
		}

		const float NotifyTime = NotifyEvent.GetTriggerTime();
		if (NotifyTime + KINDA_SMALL_NUMBER < MontagePosition)
		{
			continue;
		}

		if (NotifyTime < BestNotifyTime)
		{
			BestNotifyTime = NotifyTime;
			bFoundNotify = true;
		}
	}

	if (bFoundNotify)
	{
		OutImpactNotifyTime = BestNotifyTime;
	}

	return bFoundNotify;
}

void ATankMeleeEnemyBase::StartWindupFacingTracking()
{
	bTrackPlayerDuringAttackWindup = true;
	bAttackFacingLocked = false;

	if (CVarHDDebugTankSlam.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[TankSlam] Windup started Tank=%s Tracking=true RotationSpeed=%.1f"),
			*GetNameSafe(this),
			WindupTrackingRotationSpeed);
	}
}

void ATankMeleeEnemyBase::LockAttackFacing()
{
	if (!bTrackPlayerDuringAttackWindup && bAttackFacingLocked)
	{
		return;
	}

	bTrackPlayerDuringAttackWindup = false;
	bAttackFacingLocked = true;

	if (CVarHDDebugTankSlam.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[TankSlam] Impact commit Tank=%s Tracking=false FacingLocked=true LockedYaw=%.1f"),
			*GetNameSafe(this),
			GetActorRotation().Yaw);

		if (GetWorld())
		{
			const FVector DebugStart = GetActorLocation() + FVector(0.0f, 0.0f, 60.0f);
			DrawDebugLine(GetWorld(), DebugStart, DebugStart + GetActorForwardVector() * 180.0f, FColor::Red, false, 1.5f, 0, 3.0f);
		}
	}
}

void ATankMeleeEnemyBase::ClearAttackFacingState()
{
	const bool bWasLocked = bAttackFacingLocked;
	bTrackPlayerDuringAttackWindup = false;
	bAttackFacingLocked = false;

	if (bWasLocked && CVarHDDebugTankSlam.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[TankSlam] Attack finished Tank=%s FacingLocked=false Normal rotation resumed"),
			*GetNameSafe(this));
	}
}

void ATankMeleeEnemyBase::UpdateWindupFacing(float DeltaSeconds)
{
	if (DeltaSeconds <= 0.0f || WindupTrackingRotationSpeed <= 0.0f || !ObservedCharacterManager)
	{
		return;
	}

	const ACharacterBase* ActivePlayerCharacter = ObservedCharacterManager->GetActiveCharacter();
	if (!ActivePlayerCharacter)
	{
		return;
	}

	FVector ToPlayer = ActivePlayerCharacter->GetActorLocation() - GetActorLocation();
	ToPlayer.Z = 0.0f;
	if (!ToPlayer.Normalize())
	{
		return;
	}

	const FRotator CurrentRotation = GetActorRotation();
	const FRotator TargetRotation(0.0f, ToPlayer.Rotation().Yaw, 0.0f);
	const FRotator NewRotation = FMath::RInterpConstantTo(CurrentRotation, TargetRotation, DeltaSeconds, WindupTrackingRotationSpeed);
	SetActorRotation(FRotator(0.0f, NewRotation.Yaw, 0.0f));

	if (CVarHDDebugTankSlam.GetValueOnGameThread() != 0 && GetWorld())
	{
		const FVector DebugStart = GetActorLocation() + FVector(0.0f, 0.0f, 60.0f);
		DrawDebugLine(GetWorld(), DebugStart, DebugStart + GetActorForwardVector() * 140.0f, FColor::Orange, false, 0.15f, 0, 2.0f);
	}
}

void ATankMeleeEnemyBase::UpdateAttackTelegraphSizeAndPlacement()
{
	if (!AttackTelegraphDecal)
	{
		return;
	}

	float GroundOffset = 4.0f;
	if (const UCapsuleComponent* EnemyCapsule = GetCapsuleComponent())
	{
		GroundOffset -= EnemyCapsule->GetScaledCapsuleHalfHeight();
	}

	if (AttackShape == ETankSlamAttackShape::Circle)
	{
		const float SafeRadius = FMath::Max(0.0f, AttackAoERadius);
		AttackTelegraphDecal->DecalSize = FVector(64.0f, SafeRadius, SafeRadius);
		AttackTelegraphDecal->SetRelativeLocation(FVector(0.0f, 0.0f, GroundOffset));
	}
	else
	{
		const float SafeLength = FMath::Max(0.0f, AttackBoxLength);
		const float SafeWidth = FMath::Max(0.0f, AttackBoxWidth);
		AttackTelegraphDecal->DecalSize = FVector(64.0f, SafeWidth * 0.5f, SafeLength * 0.5f);
		AttackTelegraphDecal->SetRelativeLocation(FVector(FMath::Max(0.0f, AttackBoxForwardOffset), 0.0f, GroundOffset));
	}
}

bool ATankMeleeEnemyBase::IsPlayerInsideSlam(const ACharacterBase* ActivePlayerCharacter, float& OutDistanceForLog) const
{
	return AttackShape == ETankSlamAttackShape::Circle
		? IsPlayerInsideCircleSlam(ActivePlayerCharacter, OutDistanceForLog)
		: IsPlayerInsideBoxSlam(ActivePlayerCharacter, OutDistanceForLog);
}

bool ATankMeleeEnemyBase::IsPlayerInsideCircleSlam(const ACharacterBase* ActivePlayerCharacter, float& OutDistanceForLog) const
{
	if (!ActivePlayerCharacter)
	{
		OutDistanceForLog = 0.0f;
		return false;
	}

	OutDistanceForLog = FVector::Dist2D(GetActorLocation(), ActivePlayerCharacter->GetActorLocation());
	return OutDistanceForLog <= AttackAoERadius;
}

bool ATankMeleeEnemyBase::IsPlayerInsideBoxSlam(const ACharacterBase* ActivePlayerCharacter, float& OutDistanceForLog) const
{
	if (!ActivePlayerCharacter)
	{
		OutDistanceForLog = 0.0f;
		return false;
	}

	const FVector ToPlayer = ActivePlayerCharacter->GetActorLocation() - GetBoxSlamCenter();
	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = GetActorRightVector().GetSafeNormal2D();
	const float ForwardDistance = FVector::DotProduct(ToPlayer, Forward);
	const float RightDistance = FVector::DotProduct(ToPlayer, Right);
	OutDistanceForLog = FVector::Dist2D(GetBoxSlamCenter(), ActivePlayerCharacter->GetActorLocation());

	return FMath::Abs(ForwardDistance) <= FMath::Max(0.0f, AttackBoxLength) * 0.5f
		&& FMath::Abs(RightDistance) <= FMath::Max(0.0f, AttackBoxWidth) * 0.5f;
}

FVector ATankMeleeEnemyBase::GetBoxSlamCenter() const
{
	FVector Forward = GetActorForwardVector();
	Forward.Z = 0.0f;
	if (!Forward.Normalize())
	{
		Forward = FVector::ForwardVector;
	}

	return GetActorLocation() + Forward * FMath::Max(0.0f, AttackBoxForwardOffset);
}
