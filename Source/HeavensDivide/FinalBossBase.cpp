#include "FinalBossBase.h"

#include "AnimNotify_BossAttackExecute.h"
#include "BossGroundTelegraph.h"
#include "CharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "HealthComponent.h"
#include "EnemyLightweightMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "SurvivorPlayerController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

AFinalBossBase::AFinalBossBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	MoveSpeed = BossMoveSpeed;
	StopDistance = 350.0f;
	bUseCrowdSpread = false;
	bUseEnemySeparation = false;
	bDropsXP = false;
	XPReward = 0;
	GroundTelegraphClass = ABossGroundTelegraph::StaticClass();

	// Same component/projection setup as ATankMeleeEnemyBase (the Ogre).
	RectangleTelegraph = CreateDefaultSubobject<UDecalComponent>(TEXT("RectangleTelegraph"));
	RectangleTelegraph->SetupAttachment(RootComponent);
	RectangleTelegraph->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	RectangleTelegraph->SetComponentTickEnabled(false);
	RectangleTelegraph->SetHiddenInGame(true);
	RectangleTelegraph->SetVisibility(false);

	CircleTelegraph = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CircleTelegraph"));
	CircleTelegraph->SetupAttachment(RootComponent);
	CircleTelegraph->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CircleTelegraph->SetCastShadow(false);
	CircleTelegraph->SetVisibility(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> OgreMaterial(TEXT("/Game/HeavensDivide/Materials/M_AttackTelegraphBox.M_AttackTelegraphBox"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CircleMaterial(TEXT("/Game/HeavensDivide/Materials/M_SamuraiLaneIndicator.M_SamuraiLaneIndicator"));
	if (Cylinder.Succeeded()) CircleTelegraph->SetStaticMesh(Cylinder.Object);
	if (OgreMaterial.Succeeded()) RectangleTelegraphMaterial = OgreMaterial.Object;
	if (CircleMaterial.Succeeded()) CircleTelegraphMaterial = CircleMaterial.Object;
}

void AFinalBossBase::BeginPlay()
{
	Super::BeginPlay();
	if (HealthComponent) HealthComponent->SetMaxHealthPreservePercent(BossMaxHealth);
	if (HealthComponent) HealthComponent->OnHealthChanged.AddUniqueDynamic(this, &AFinalBossBase::HandleBossHealthChangedForPhase);
	MoveSpeed = BossMoveSpeed;
	ApplySpawnInstanceModifiers(1.0f, 1.0f, 1.0f);
	PlayerController = ResolvePlayerController();
	if (RectangleTelegraphMaterial)
	{
		RectangleMaterialInstance = UMaterialInstanceDynamic::Create(RectangleTelegraphMaterial, this);
		RectangleTelegraph->SetDecalMaterial(RectangleMaterialInstance);
	}
	if (CircleTelegraphMaterial) CircleTelegraph->SetMaterial(0, CircleTelegraphMaterial);
	CircleMaterialInstance = CircleTelegraph->CreateAndSetMaterialInstanceDynamic(0);
	if (CircleMaterialInstance)
	{
		CircleMaterialInstance->SetVectorParameterValue(TEXT("FillColor"), FLinearColor::Red);
		CircleMaterialInstance->SetScalarParameterValue(TEXT("FillAmount"), 1.0f);
	}
	HideAttackTelegraphs();
	if (bAutoStartCombat) StartBossCombat();
}

void AFinalBossBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CleanupCurrentAttack(true);
	Super::EndPlay(EndPlayReason);
}

void AFinalBossBase::StartBossCombat()
{
	if (IsDead()) return;
	bCombatEnabled = true;
	BossState = EFinalBossState::Cooldown;
	StateElapsed = 0.0f;
	PlayerController = ResolvePlayerController();
	if (PlayerController) PlayerController->ShowBossHealthBar(this);
}

void AFinalBossBase::StopBossCombat()
{
	bCombatEnabled = false;
	if (PlayerController) PlayerController->HideBossHealthBar(this);
	CleanupCurrentAttack(true);
	if (!IsDead()) BossState = EFinalBossState::Idle;
}

void AFinalBossBase::DebugForceAttack(EFinalBossAttack Attack)
{
	if (IsDead() || Attack == EFinalBossAttack::None) return;
	if (!bCombatEnabled) StartBossCombat();
	CleanupCurrentAttack(false);
	BeginAttack(Attack);
}

void AFinalBossBase::DebugForceForwardCleave() { DebugForceAttack(EFinalBossAttack::ForwardCleave); }

void AFinalBossBase::ForcePhase2()
{
	if (!IsDead() && !bPhase2Active && !bPhase2TransitionQueued) QueuePhase2Transition();
}

bool AFinalBossBase::ApplyPlayerDamage(float DamageAmount, EPlayerAttackSource AttackSource)
{
	if (BossState == EFinalBossState::PhaseTransition && bPhase2InvulnerableDuringTransition) return false;
	return Super::ApplyPlayerDamage(DamageAmount, AttackSource);
}

void AFinalBossBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bCombatEnabled || IsDead()) return;
	if (!PlayerController) PlayerController = ResolvePlayerController();
	if (!PlayerController || PlayerController->IsPlayerDead()) { StopBossCombat(); return; }

	if (BossState == EFinalBossState::AttackActive && CurrentAttack == EFinalBossAttack::LongDash)
	{
		UpdateDash(DeltaSeconds);
	}
	else if (BossState == EFinalBossState::Recovery || BossState == EFinalBossState::Cooldown || BossState == EFinalBossState::PhaseTransition)
	{
		StateElapsed += DeltaSeconds;
		if (BossState == EFinalBossState::PhaseTransition && !bPhase2TransitionMontagePlaying && StateElapsed >= Phase2FallbackTransitionDuration)
		{
			CompletePhase2Transition();
		}
		else if (BossState == EFinalBossState::Recovery && StateElapsed >= GetRecoveryDurationForAttack(CurrentAttack))
		{
			BossState = EFinalBossState::Cooldown;
			StateElapsed = 0.0f;
		}
		else if (BossState == EFinalBossState::Cooldown && StateElapsed >= GetCurrentAttackCooldown())
		{
			ChooseAttack();
		}
	}
}

void AFinalBossBase::UpdateEnemyBehavior(float DeltaSeconds)
{
	if (!bCombatEnabled) StopEnemyMovement();
	else if (BossState == EFinalBossState::Idle || BossState == EFinalBossState::Cooldown) Super::UpdateEnemyBehavior(DeltaSeconds);
	else StopEnemyMovement();
}

bool AFinalBossBase::ShouldSkipMovement() const
{
	return Super::ShouldSkipMovement() || (bCombatEnabled && BossState != EFinalBossState::Idle && BossState != EFinalBossState::Cooldown);
}

void AFinalBossBase::ChooseAttack()
{
	const ACharacterBase* Player = PlayerController ? Cast<ACharacterBase>(PlayerController->GetPawn()) : nullptr;
	if (!Player) return;

	const float PlayerDistance = FVector::Dist2D(GetActorLocation(), Player->GetActorLocation());
	TArray<EFinalBossAttack> Choices;
	for (const EFinalBossAttack Attack : { EFinalBossAttack::ForwardCleave, EFinalBossAttack::PointBlankAoE, EFinalBossAttack::LongDash, EFinalBossAttack::GroundPursuit })
	{
		if (IsAttackWithinStartDistance(Attack, PlayerDistance)) Choices.Add(Attack);
	}

	// If nothing is eligible, remain in Cooldown so inherited movement can close
	// distance. Avoid repeatedly attempting selection every frame.
	if (Choices.IsEmpty())
	{
		StateElapsed = 0.0f;
		return;
	}

	// Preserve no-immediate-repeat when there is another eligible option. A sole
	// eligible attack may repeat rather than leaving the boss unable to attack.
	if (Choices.Num() > 1 && PreviousAttack != EFinalBossAttack::None) Choices.Remove(PreviousAttack);
	BeginAttack(Choices[FMath::RandRange(0, Choices.Num() - 1)]);
}

bool AFinalBossBase::IsAttackWithinStartDistance(EFinalBossAttack Attack, float PlayerDistance) const
{
	float MinDistance = 0.0f;
	float MaxDistance = TNumericLimits<float>::Max();
	switch (Attack)
	{
	case EFinalBossAttack::ForwardCleave:
		MinDistance = ForwardCleaveMinStartDistance;
		MaxDistance = ForwardCleaveMaxStartDistance;
		break;
	case EFinalBossAttack::PointBlankAoE:
		MinDistance = PointBlankAOEMinStartDistance;
		MaxDistance = PointBlankAOEMaxStartDistance;
		break;
	case EFinalBossAttack::LongDash:
		MinDistance = LongDashMinStartDistance;
		MaxDistance = LongDashMaxStartDistance;
		break;
	case EFinalBossAttack::GroundPursuit:
		MinDistance = GroundPursuitMinStartDistance;
		MaxDistance = GroundPursuitMaxStartDistance;
		break;
	default:
		return false;
	}

	const float SafeMinDistance = FMath::Max(0.0f, MinDistance);
	const float SafeMaxDistance = FMath::Max(SafeMinDistance, MaxDistance);
	return PlayerDistance >= SafeMinDistance && PlayerDistance <= SafeMaxDistance;
}

float AFinalBossBase::GetRecoveryDurationForAttack(EFinalBossAttack Attack) const
{
	if (bPhase2Active)
	{
		switch (Attack)
		{
		case EFinalBossAttack::ForwardCleave: return FMath::Max(0.0f, Phase2ForwardCleaveRecoveryDuration);
		case EFinalBossAttack::PointBlankAoE: return FMath::Max(0.0f, Phase2PointBlankAOERecoveryDuration);
		case EFinalBossAttack::LongDash: return FMath::Max(0.0f, Phase2LongDashRecoveryDuration);
		case EFinalBossAttack::GroundPursuit: return FMath::Max(0.0f, Phase2GroundPursuitRecoveryDuration);
		default: return 0.0f;
		}
	}
	switch (Attack)
	{
	case EFinalBossAttack::ForwardCleave: return FMath::Max(0.0f, ForwardCleaveRecoveryDuration);
	case EFinalBossAttack::PointBlankAoE: return FMath::Max(0.0f, PointBlankAOERecoveryDuration);
	case EFinalBossAttack::LongDash: return FMath::Max(0.0f, LongDashRecoveryDuration);
	case EFinalBossAttack::GroundPursuit: return FMath::Max(0.0f, GroundPursuitRecoveryDuration);
	default: return 0.0f;
	}
}

float AFinalBossBase::GetCurrentAttackCooldown() const
{
	return FMath::Max(0.0f, bPhase2Active ? Phase2AttackCooldown : AttackCooldown);
}

void AFinalBossBase::HandleBossHealthChangedForPhase(float CurrentHealth, float MaxHealth, float HealthPercent)
{
	if (!bPhase2Active && !bPhase2TransitionQueued
		&& HealthPercent <= FMath::Clamp(Phase2HealthThreshold, 0.0f, 1.0f)
		&& CurrentHealth > 0.0f && MaxHealth > 0.0f)
	{
		QueuePhase2Transition();
	}
}

void AFinalBossBase::QueuePhase2Transition()
{
	if (bPhase2Active || bPhase2TransitionQueued || IsDead()) return;
	bPhase2TransitionQueued = true;
	if (BossState != EFinalBossState::Windup && BossState != EFinalBossState::AttackActive)
	{
		BeginPhase2Transition();
	}
}

void AFinalBossBase::BeginPhase2Transition()
{
	if (bPhase2Active || IsDead()) return;
	bPhase2TransitionQueued = true;
	bGroundPursuitWindowActive = false;
	HideAttackTelegraphs();
	StopDash();
	StopEnemyMovement();
	BossState = EFinalBossState::PhaseTransition;
	StateElapsed = 0.0f;
	PreviousAttack = EFinalBossAttack::None;
	CurrentAttack = EFinalBossAttack::None;
	BP_OnBossPhase2TransitionStarted();

	bPhase2TransitionMontagePlaying = false;
	if (Phase2TransitionMontage)
	{
		if (UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
		{
			if (AnimInstance->Montage_Play(Phase2TransitionMontage) > 0.0f)
			{
				bPhase2TransitionMontagePlaying = true;
				FOnMontageEnded EndDelegate;
				EndDelegate.BindUObject(this, &AFinalBossBase::HandlePhase2TransitionMontageEnded);
				AnimInstance->Montage_SetEndDelegate(EndDelegate, Phase2TransitionMontage);
			}
		}
	}
}

void AFinalBossBase::HandlePhase2TransitionMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage == Phase2TransitionMontage && BossState == EFinalBossState::PhaseTransition && bCombatEnabled) CompletePhase2Transition();
}

void AFinalBossBase::CompletePhase2Transition()
{
	if (bPhase2Active || IsDead()) return;
	bPhase2TransitionMontagePlaying = false;
	bPhase2TransitionQueued = false;
	bPhase2Active = true;
	PreviousAttack = EFinalBossAttack::None;
	CurrentAttack = EFinalBossAttack::None;
	BossState = EFinalBossState::Cooldown;
	StateElapsed = 0.0f;
	OnBossPhase2Started.Broadcast();
	BP_OnBossPhase2Started();
}

void AFinalBossBase::ConfigurePhase2CleaveDirection()
{
	float AngleDegrees = 0.0f;
	if (Phase2CleaveTelegraphIndex == 1) AngleDegrees = -Phase2ForwardCleaveSideAngle;
	else if (Phase2CleaveTelegraphIndex >= 2) AngleDegrees = Phase2ForwardCleaveSideAngle;
	ActivePhase2CleaveDirection = FQuat(FVector::UpVector, FMath::DegreesToRadians(AngleDegrees))
		.RotateVector(Phase2CleaveBaseDirection).GetSafeNormal2D();
	if (ActivePhase2CleaveDirection.IsNearlyZero()) ActivePhase2CleaveDirection = Phase2CleaveBaseDirection;
#if !UE_BUILD_SHIPPING
	if (bDebugAttackTelegraphs)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BossTripleCleave] Index=%d Angle=%.1f BaseDirection=%s ActiveDirection=%s"),
			Phase2CleaveTelegraphIndex, AngleDegrees,
			*Phase2CleaveBaseDirection.ToCompactString(), *ActivePhase2CleaveDirection.ToCompactString());
	}
#endif
	++Phase2CleaveTelegraphIndex;
}

void AFinalBossBase::ReacquireDashDirection()
{
	const ACharacterBase* Player = PlayerController ? Cast<ACharacterBase>(PlayerController->GetPawn()) : nullptr;
	if (!Player) return;
	LockedAttackOrigin = GetActorLocation();
	LockedAttackDirection = (Player->GetActorLocation() - LockedAttackOrigin).GetSafeNormal2D();
	if (LockedAttackDirection.IsNearlyZero()) LockedAttackDirection = GetActorForwardVector().GetSafeNormal2D();
	SetActorRotation(LockedAttackDirection.Rotation());
}

void AFinalBossBase::BeginAttack(EFinalBossAttack Attack)
{
	ACharacterBase* Player = PlayerController ? Cast<ACharacterBase>(PlayerController->GetPawn()) : nullptr;
	if (!Player) { StopBossCombat(); return; }
	CurrentAttack = Attack;
	if (Attack == EFinalBossAttack::LongDash) BeginDashRootMotionOverride();
	LockedAttackOrigin = GetActorLocation();
	LockedAttackDirection = (Player->GetActorLocation() - LockedAttackOrigin).GetSafeNormal2D();
	if (LockedAttackDirection.IsNearlyZero()) LockedAttackDirection = GetActorForwardVector().GetSafeNormal2D();
	SetActorRotation(LockedAttackDirection.Rotation());
	Phase2BaseAttackDirection = LockedAttackDirection;
	Phase2CleaveBaseDirection = LockedAttackDirection;
	ActivePhase2CleaveDirection = LockedAttackDirection;
	Phase2CleaveTelegraphIndex = 0;
	Phase2DashTelegraphIndex = 0;
	bAttackExecuted = false;
	bGroundPursuitWindowActive = Attack == EFinalBossAttack::GroundPursuit;
	BossState = EFinalBossState::Windup;
	StateElapsed = 0.0f;
	StopEnemyMovement();
	if (!PlayCurrentAttackMontage())
	{
		UE_LOG(LogTemp, Warning, TEXT("[BossAttack] Missing or unplayable montage for attack %d on %s; attack skipped."), static_cast<int32>(Attack), *GetNameSafe(this));
		CleanupCurrentAttack(false);
		BeginRecovery();
		return;
	}
	PreviousAttack = Attack;
	OnBossAttackStarted.Broadcast(Attack);
}

UAnimMontage* AFinalBossBase::GetMontageForAttack(EFinalBossAttack Attack) const
{
	switch (Attack)
	{
	case EFinalBossAttack::ForwardCleave: return bPhase2Active ? Phase2ForwardCleaveMontage.Get() : ForwardCleaveMontage.Get();
	case EFinalBossAttack::PointBlankAoE: return bPhase2Active ? Phase2AOEMontage.Get() : PointBlankAOEMontage.Get();
	case EFinalBossAttack::LongDash: return bPhase2Active ? Phase2LongDashMontage.Get() : LongDashMontage.Get();
	case EFinalBossAttack::GroundPursuit: return GroundPursuitMontage;
	default: return nullptr;
	}
}

bool AFinalBossBase::PlayCurrentAttackMontage()
{
	UAnimMontage* Montage = GetMontageForAttack(CurrentAttack);
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	const float PlayRate = bPhase2Active && CurrentAttack == EFinalBossAttack::PointBlankAoE ? FMath::Max(0.01f, Phase2AOEMontagePlayRate) : 1.0f;
	if (!Montage || !AnimInstance || AnimInstance->Montage_Play(Montage, PlayRate) <= 0.0f) return false;
	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &AFinalBossBase::HandleAttackMontageEnded);
	AnimInstance->Montage_SetEndDelegate(EndDelegate, Montage);
	return true;
}

void AFinalBossBase::HandleBossTelegraphStart()
{
	if (!bCombatEnabled || IsDead() || BossState != EFinalBossState::Windup) return;
	if (bPhase2Active && CurrentAttack == EFinalBossAttack::ForwardCleave)
	{
		ConfigurePhase2CleaveDirection();
		bAttackExecuted = false;
	}
	else if (bPhase2Active && CurrentAttack == EFinalBossAttack::LongDash)
	{
		if (Phase2DashTelegraphIndex > 0) ReacquireDashDirection();
		++Phase2DashTelegraphIndex;
		bAttackExecuted = false;
	}
	if (CurrentAttack == EFinalBossAttack::ForwardCleave)
	{
		ShowRectangleTelegraph(ForwardAttackLength, ForwardAttackWidth,
			bPhase2Active ? ActivePhase2CleaveDirection : LockedAttackDirection);
		StartRectangleFill();
	}
	else if (CurrentAttack == EFinalBossAttack::LongDash) { ShowRectangleTelegraph(DashDistance, DashWidth, LockedAttackDirection); StartRectangleFill(); }
	else if (CurrentAttack == EFinalBossAttack::PointBlankAoE) { ShowCircleTelegraph(bPhase2Active ? Phase2AoERadius : AoERadius); StartRectangleFill(); }
}

void AFinalBossBase::HandleBossAttackExecute()
{
	if (!bCombatEnabled || IsDead() || bAttackExecuted || BossState == EFinalBossState::Recovery || BossState == EFinalBossState::Cooldown) return;
	bAttackExecuted = true;
	StopRectangleFill();
	SetRectangleFill(1.0f);
	OnBossAttackImpact.Broadcast(CurrentAttack);
	if (CurrentAttack == EFinalBossAttack::ForwardCleave)
	{
		DamagePlayerInRectangle(
			ForwardAttackLength,
			ForwardAttackWidth,
			ForwardAttackDamage * (bPhase2Active ? Phase2ForwardCleaveDamageMultiplier : 1.0f),
			bPhase2Active ? ActivePhase2CleaveDirection : LockedAttackDirection);
	}
	else if (CurrentAttack == EFinalBossAttack::PointBlankAoE) DamagePlayerInCircle(bPhase2Active ? Phase2AoERadius : AoERadius, AoEDamage * (bPhase2Active ? Phase2AOEDamageMultiplier : 1.0f));
	else if (CurrentAttack == EFinalBossAttack::LongDash) StartDash();
}

void AFinalBossBase::HandleBossTelegraphEnd()
{
	if (!bCombatEnabled || IsDead()) return;
	HideAttackTelegraphs();
	if (CurrentAttack == EFinalBossAttack::LongDash) StopDash();
}

void AFinalBossBase::HandleBossSpawnGroundCircle()
{
	if (bCombatEnabled && !IsDead() && CurrentAttack == EFinalBossAttack::GroundPursuit && bGroundPursuitWindowActive)
	{
		const int32 CircleCount = bPhase2Active ? FMath::Max(1, Phase2GroundPursuitCirclesPerNotify) : 1;
		for (int32 Index = 0; Index < CircleCount; ++Index) SpawnGroundCircle();
	}
}

void AFinalBossBase::HandleAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != GetMontageForAttack(CurrentAttack) || BossState == EFinalBossState::Dead) return;
	bGroundPursuitWindowActive = false;
	EndDashRootMotionOverride();
	StopDash();
	HideAttackTelegraphs();
	if (bDebugAttackTelegraphs) UE_LOG(LogTemp, Log, TEXT("[BossAttack] Montage ended Attack=%d Interrupted=%d"), static_cast<int32>(CurrentAttack), bInterrupted ? 1 : 0);
	if (bCombatEnabled)
	{
		if (bPhase2TransitionQueued && !bPhase2Active) BeginPhase2Transition();
		else BeginRecovery();
	}
}

void AFinalBossBase::BeginRecovery()
{
	HideAttackTelegraphs();
	StopDash();
	BossState = EFinalBossState::Recovery;
	StateElapsed = 0.0f;
}

void AFinalBossBase::ShowRectangleTelegraph(float Length, float Width, const FVector& AttackDirection)
{
	if (!GetWorld() || !GroundTelegraphClass) return;
	if (IsValid(ActiveAttackTelegraph)) ActiveAttackTelegraph->CancelTelegraph();
	const float GroundZ = GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 4.0f;
	const FVector SafeDirection = AttackDirection.GetSafeNormal2D();
	FVector Center = LockedAttackOrigin + SafeDirection * Length * 0.5f;
	Center.Z = GroundZ;
	FActorSpawnParameters Params;
	Params.Owner = this;
	ActiveAttackTelegraph = GetWorld()->SpawnActor<ABossGroundTelegraph>(GroundTelegraphClass, Center, SafeDirection.Rotation(), Params);
	if (ActiveAttackTelegraph)
	{
		ActiveAttackTelegraph->InitializePersistentRectangle(Length, Width, RectangleTelegraphMaterial);
	}
}

void AFinalBossBase::ShowCircleTelegraph(float Radius)
{
	if (!GetWorld() || !GroundTelegraphClass) return;
	if (IsValid(ActiveAttackTelegraph)) ActiveAttackTelegraph->CancelTelegraph();
	FVector Center = GetActorLocation();
	Center.Z -= GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 4.0f;
	FActorSpawnParameters Params;
	Params.Owner = this;
	ActiveAttackTelegraph = GetWorld()->SpawnActor<ABossGroundTelegraph>(GroundTelegraphClass, Center, FRotator::ZeroRotator, Params);
	if (ActiveAttackTelegraph) ActiveAttackTelegraph->InitializePersistentCircle(Radius, CircleTelegraphMaterial);
}

void AFinalBossBase::HideAttackTelegraphs()
{
	StopRectangleFill();
	SetRectangleFill(0.0f);
	if (RectangleTelegraph) { RectangleTelegraph->SetHiddenInGame(true); RectangleTelegraph->SetVisibility(false); }
	if (CircleTelegraph) { CircleTelegraph->SetHiddenInGame(true); CircleTelegraph->SetVisibility(false); }
	if (IsValid(ActiveAttackTelegraph)) ActiveAttackTelegraph->CancelTelegraph();
	ActiveAttackTelegraph = nullptr;
}

void AFinalBossBase::StartRectangleFill()
{
	StopRectangleFill();
	SetRectangleFill(0.0f);
	if (!GetWorld()) return;
	RectangleFillDuration = GetSecondsUntilExecuteNotify();
	if (RectangleFillDuration <= KINDA_SMALL_NUMBER) { SetRectangleFill(1.0f); return; }
	RectangleFillStartTime = GetWorld()->GetTimeSeconds();
	GetWorldTimerManager().SetTimer(RectangleFillTimerHandle, this, &AFinalBossBase::UpdateRectangleFill, FMath::Max(0.01f, TelegraphFillUpdateInterval), true);
}

void AFinalBossBase::StopRectangleFill() { if (GetWorld()) GetWorldTimerManager().ClearTimer(RectangleFillTimerHandle); }

void AFinalBossBase::UpdateRectangleFill()
{
	if (!GetWorld() || BossState != EFinalBossState::Windup) { StopRectangleFill(); return; }
	const float Alpha = FMath::Clamp(static_cast<float>(GetWorld()->GetTimeSeconds() - RectangleFillStartTime) / RectangleFillDuration, 0.0f, 0.99f);
	SetRectangleFill(Alpha);
	if (Alpha >= 0.99f) StopRectangleFill();
}

void AFinalBossBase::SetRectangleFill(float Alpha)
{
	const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	if (RectangleMaterialInstance) RectangleMaterialInstance->SetScalarParameterValue(TEXT("FillAmount"), ClampedAlpha);
	if (IsValid(ActiveAttackTelegraph)) ActiveAttackTelegraph->SetTelegraphFillAmount(ClampedAlpha);
}

float AFinalBossBase::GetSecondsUntilExecuteNotify() const
{
	UAnimMontage* Montage = GetMontageForAttack(CurrentAttack);
	const UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (Montage && AnimInstance)
	{
		const float Position = AnimInstance->Montage_GetPosition(Montage);
		float BestTime = TNumericLimits<float>::Max();
		for (const FAnimNotifyEvent& Event : Montage->Notifies)
		{
			if (Event.Notify && Event.Notify->IsA<UAnimNotify_BossAttackExecute>() && Event.GetTriggerTime() >= Position) BestTime = FMath::Min(BestTime, Event.GetTriggerTime());
		}
		const float Rate = FMath::Abs(AnimInstance->Montage_GetPlayRate(Montage));
		if (BestTime < TNumericLimits<float>::Max() && Rate > KINDA_SMALL_NUMBER) return FMath::Max(0.01f, (BestTime - Position) / Rate);
	}
	return FMath::Max(0.01f, ForwardAttackTelegraphDuration);
}

void AFinalBossBase::DamagePlayerInRectangle(float Length, float Width, float Damage, const FVector& AttackDirection)
{
	const ACharacterBase* Player = PlayerController ? Cast<ACharacterBase>(PlayerController->GetPawn()) : nullptr;
	if (!Player) return;
	const FVector Delta = Player->GetActorLocation() - LockedAttackOrigin;
	const FVector SafeDirection = AttackDirection.GetSafeNormal2D();
	const float Forward = FVector::DotProduct(Delta, SafeDirection);
	const FVector Right = FVector::CrossProduct(FVector::UpVector, SafeDirection);
	if (Forward >= 0.0f && Forward <= Length && FMath::Abs(FVector::DotProduct(Delta, Right)) <= Width * 0.5f) PlayerController->ApplyDamageToPlayer(Damage);
}

void AFinalBossBase::DamagePlayerInCircle(float Radius, float Damage)
{
	const ACharacterBase* Player = PlayerController ? Cast<ACharacterBase>(PlayerController->GetPawn()) : nullptr;
	if (Player && FVector::DistSquared2D(Player->GetActorLocation(), GetActorLocation()) <= FMath::Square(Radius)) PlayerController->ApplyDamageToPlayer(Damage);
}

void AFinalBossBase::StartDash()
{
	StopEnemyMovement();
	BeginDashCollisionOverride();
	BossState = EFinalBossState::AttackActive;
	DashElapsed = 0.0f;
	DashStart = GetActorLocation();
	DashEnd = DashStart + LockedAttackDirection * DashDistance;
	bDashHitPlayer = false;
	HideAttackTelegraphs();

#if !UE_BUILD_SHIPPING
	if (bDebugAttackTelegraphs)
	{
		const UCapsuleComponent* Capsule = GetCapsuleComponent();
		UE_LOG(LogTemp, Warning,
			TEXT("[BossDash] Begin Location=%s Destination=%s Direction=%s CollisionEnabled=%s Profile=%s ObjectType=%d Pawn=%d WorldStatic=%d WorldDynamic=%d Montage=%s MontageHasRootMotion=%d AnimRootMotionModeBefore=%d"),
			*GetActorLocation().ToCompactString(), *DashEnd.ToCompactString(), *LockedAttackDirection.ToCompactString(),
			Capsule ? *UEnum::GetValueAsString(Capsule->GetCollisionEnabled()) : TEXT("None"),
			Capsule ? *Capsule->GetCollisionProfileName().ToString() : TEXT("None"),
			Capsule ? static_cast<int32>(Capsule->GetCollisionObjectType()) : -1,
			Capsule ? static_cast<int32>(Capsule->GetCollisionResponseToChannel(ECC_Pawn)) : -1,
			Capsule ? static_cast<int32>(Capsule->GetCollisionResponseToChannel(ECC_WorldStatic)) : -1,
			Capsule ? static_cast<int32>(Capsule->GetCollisionResponseToChannel(ECC_WorldDynamic)) : -1,
			*GetNameSafe(GetMontageForAttack(CurrentAttack)),
			GetMontageForAttack(CurrentAttack) && GetMontageForAttack(CurrentAttack)->HasRootMotion() ? 1 : 0,
			static_cast<int32>(PreDashRootMotionMode.GetValue()));
	}
#endif
}

void AFinalBossBase::StopDash()
{
	EndDashCollisionOverride();
	if (CurrentAttack == EFinalBossAttack::LongDash && BossState == EFinalBossState::AttackActive) BossState = EFinalBossState::Windup;
}

void AFinalBossBase::BeginDashCollisionOverride()
{
	if (bDashCollisionOverrideActive) return;
	if (UCapsuleComponent* BossCapsule = GetCapsuleComponent())
	{
		PreDashPawnCollisionResponse = BossCapsule->GetCollisionResponseToChannel(ECC_Pawn);
		BossCapsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		bDashCollisionOverrideActive = true;
	}
}

void AFinalBossBase::EndDashCollisionOverride()
{
	if (!bDashCollisionOverrideActive) return;
	if (UCapsuleComponent* BossCapsule = GetCapsuleComponent())
	{
		BossCapsule->SetCollisionResponseToChannel(ECC_Pawn, PreDashPawnCollisionResponse);
	}
	bDashCollisionOverrideActive = false;
}

void AFinalBossBase::BeginDashRootMotionOverride()
{
	if (bDashRootMotionOverrideActive) return;
	if (UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
	{
		PreDashRootMotionMode = AnimInstance->RootMotionMode;
		AnimInstance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
		bDashRootMotionOverrideActive = true;
	}
}

void AFinalBossBase::EndDashRootMotionOverride()
{
	if (!bDashRootMotionOverrideActive) return;
	if (UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
	{
		AnimInstance->SetRootMotionMode(PreDashRootMotionMode);
	}
	bDashRootMotionOverrideActive = false;
}

void AFinalBossBase::UpdateDash(float DeltaSeconds)
{
	DashElapsed += DeltaSeconds;
	const FVector Previous = GetActorLocation();
	const float Alpha = FMath::Clamp(DashElapsed / FMath::Max(0.01f, DashTravelDuration), 0.0f, 1.0f);
	const FVector DesiredLocation = FMath::Lerp(DashStart, DashEnd, Alpha);
	FHitResult MovementHit;
	bool bDashBlockedByWorld = false;
	if (LightweightMovementComponent)
	{
		bDashBlockedByWorld = LightweightMovementComponent->MoveOwnerToNoSlide(DesiredLocation, MovementHit);
	}
	else
	{
		SetActorLocation(DesiredLocation, true, &MovementHit);
		bDashBlockedByWorld = MovementHit.bBlockingHit;
	}
	if (!bDashHitPlayer && PlayerController)
	{
		if (const ACharacterBase* Player = Cast<ACharacterBase>(PlayerController->GetPawn()))
		{
			const FVector Closest = FMath::ClosestPointOnSegment(Player->GetActorLocation(), Previous, GetActorLocation());
			if (FVector::DistSquared2D(Closest, Player->GetActorLocation()) <= FMath::Square(DashWidth * 0.5f)) { bDashHitPlayer = true; PlayerController->ApplyDamageToPlayer(DashDamage); }
		}
	}
	if (bDashBlockedByWorld)
	{
#if !UE_BUILD_SHIPPING
		if (bDebugAttackTelegraphs)
		{
			const UPrimitiveComponent* HitComponent = MovementHit.GetComponent();
			UE_LOG(LogTemp, Warning,
				TEXT("[BossDash] BlockingHit Actor=%s Component=%s Profile=%s ObjectType=%d Blocking=%d Time=%.3f DistanceTraveled=%.1f ImpactPoint=%s ImpactNormal=%s Before=%s Result=%s"),
				*GetNameSafe(MovementHit.GetActor()), *GetNameSafe(HitComponent),
				HitComponent ? *HitComponent->GetCollisionProfileName().ToString() : TEXT("None"),
				HitComponent ? static_cast<int32>(HitComponent->GetCollisionObjectType()) : -1,
				MovementHit.bBlockingHit ? 1 : 0, MovementHit.Time,
				FVector::Dist2D(Previous, GetActorLocation()), *MovementHit.ImpactPoint.ToCompactString(),
				*MovementHit.ImpactNormal.ToCompactString(), *Previous.ToCompactString(), *GetActorLocation().ToCompactString());
		}
#endif
		StopDash();
		return;
	}
	if (Alpha >= 1.0f) StopDash();
}

void AFinalBossBase::SpawnGroundCircle()
{
	ACharacterBase* Player = PlayerController ? Cast<ACharacterBase>(PlayerController->GetPawn()) : nullptr;
	if (!Player || !GroundTelegraphClass || !GetWorld()) return;

	const FVector PlayerLocation = Player->GetActorLocation();
	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(BossGroundPursuitFloor), false);
	TraceParams.AddIgnoredActor(Player);
	TraceParams.AddIgnoredActor(this);

	const float MinRadius = FMath::Max(0.0f, GroundPursuitMinSpawnRadius);
	const float MaxRadius = FMath::Max(MinRadius, GroundPursuitMaxSpawnRadius);
	const float MinSeparationSquared = FMath::Square(FMath::Max(0.0f, GroundPursuitMinCircleSeparation));
	const int32 MaxAttempts = FMath::Max(1, GroundPursuitMaxSpawnAttempts);
	FVector SpawnLocation = PlayerLocation;
	bool bFoundValidCandidate = false;
	int32 AttemptsUsed = 0;
	float AcceptedDistance = 0.0f;

	for (int32 Attempt = 1; Attempt <= MaxAttempts; ++Attempt)
	{
		AttemptsUsed = Attempt;
		const float AngleRadians = FMath::FRandRange(0.0f, 2.0f * UE_PI);
		const float Distance = FMath::FRandRange(MinRadius, MaxRadius);
		const FVector Candidate(
			PlayerLocation.X + FMath::Cos(AngleRadians) * Distance,
			PlayerLocation.Y + FMath::Sin(AngleRadians) * Distance,
			PlayerLocation.Z);

		FHitResult FloorHit;
		const FVector TraceStart = Candidate + FVector(0.0f, 0.0f, 200.0f);
		const FVector TraceEnd = Candidate - FVector(0.0f, 0.0f, 3000.0f);
		if (!GetWorld()->LineTraceSingleByChannel(FloorHit, TraceStart, TraceEnd, ECC_Visibility, TraceParams))
		{
			continue;
		}

		const FVector CandidateGroundLocation = FloorHit.ImpactPoint + FloorHit.ImpactNormal * 4.0f;
		bool bMeetsSeparation = true;
		for (ABossGroundTelegraph* ExistingTelegraph : ActiveGroundTelegraphs)
		{
			if (IsValid(ExistingTelegraph)
				&& FVector::DistSquared2D(CandidateGroundLocation, ExistingTelegraph->GetActorLocation()) < MinSeparationSquared)
			{
				bMeetsSeparation = false;
				break;
			}
		}
		if (!bMeetsSeparation)
		{
			continue;
		}

		SpawnLocation = CandidateGroundLocation;
		AcceptedDistance = Distance;
		bFoundValidCandidate = true;
		break;
	}

	if (!bFoundValidCandidate)
	{
		// Preserve the previous known-safe placement as the final fallback.
		FHitResult FloorHit;
		const FVector TraceStart = PlayerLocation + FVector(0.0f, 0.0f, 200.0f);
		const FVector TraceEnd = PlayerLocation - FVector(0.0f, 0.0f, 3000.0f);
		if (GetWorld()->LineTraceSingleByChannel(FloorHit, TraceStart, TraceEnd, ECC_Visibility, TraceParams))
		{
			SpawnLocation = FloorHit.ImpactPoint + FloorHit.ImpactNormal * 4.0f;
		}
		else if (const UCapsuleComponent* PlayerCapsule = Player->GetCapsuleComponent())
		{
			SpawnLocation.Z -= PlayerCapsule->GetScaledCapsuleHalfHeight() - 4.0f;
		}
	}

	if (bDebugAttackTelegraphs)
	{
		UE_LOG(LogTemp, Log,
			TEXT("[BossGroundPursuit] Player=(%.1f,%.1f) Candidate=(%.1f,%.1f) Distance=%.1f Attempts=%d RandomCandidate=%d"),
			PlayerLocation.X, PlayerLocation.Y, SpawnLocation.X, SpawnLocation.Y,
			AcceptedDistance, AttemptsUsed, bFoundValidCandidate ? 1 : 0);
	}

	FActorSpawnParameters Params; Params.Owner = this;
	ABossGroundTelegraph* Telegraph = GetWorld()->SpawnActor<ABossGroundTelegraph>(GroundTelegraphClass, SpawnLocation, FRotator::ZeroRotator, Params);
	if (Telegraph)
	{
		Telegraph->InitializeTelegraph(PlayerController, GroundCircleRadius, GroundCircleTelegraphDuration, GroundCircleDamage, CircleTelegraphMaterial);
		ActiveGroundTelegraphs.Add(Telegraph);
	}
}

void AFinalBossBase::CleanupCurrentAttack(bool bCancelPendingGroundTelegraphs)
{
	bGroundPursuitWindowActive = false;
	Phase2CleaveTelegraphIndex = 0;
	Phase2CleaveBaseDirection = FVector::ForwardVector;
	ActivePhase2CleaveDirection = FVector::ForwardVector;
	EndDashCollisionOverride();
	HideAttackTelegraphs();
	if (UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
	{
		if (UAnimMontage* Montage = GetMontageForAttack(CurrentAttack)) AnimInstance->Montage_Stop(0.1f, Montage);
		if (Phase2TransitionMontage && AnimInstance->Montage_IsPlaying(Phase2TransitionMontage)) AnimInstance->Montage_Stop(0.1f, Phase2TransitionMontage);
	}
	EndDashRootMotionOverride();
	bPhase2TransitionMontagePlaying = false;
	if (bCancelPendingGroundTelegraphs)
	{
		for (ABossGroundTelegraph* Telegraph : ActiveGroundTelegraphs) if (IsValid(Telegraph)) Telegraph->CancelTelegraph();
		ActiveGroundTelegraphs.Reset();
	}
	StopEnemyMovement();
	CurrentAttack = EFinalBossAttack::None;
}

ASurvivorPlayerController* AFinalBossBase::ResolvePlayerController()
{
	return GetWorld() ? Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0)) : nullptr;
}

void AFinalBossBase::HandleDeath()
{
	if (IsDead()) return;
	bCombatEnabled = false;
	BossState = EFinalBossState::Dead;
	CleanupCurrentAttack(true);
	OnBossDefeated.Broadcast(this);
	Super::HandleDeath();
}
