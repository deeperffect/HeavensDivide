#include "FinalBossBase.h"

#include "AnimNotify_BossAttackExecute.h"
#include "BossGroundTelegraph.h"
#include "CharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "Components/StaticMeshComponent.h"
#include "HealthComponent.h"
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
	else if (BossState == EFinalBossState::Recovery || BossState == EFinalBossState::Cooldown)
	{
		StateElapsed += DeltaSeconds;
		if (BossState == EFinalBossState::Recovery && StateElapsed >= GetRecoveryDurationForAttack(CurrentAttack))
		{
			BossState = EFinalBossState::Cooldown;
			StateElapsed = 0.0f;
		}
		else if (BossState == EFinalBossState::Cooldown && StateElapsed >= AttackCooldown)
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
	switch (Attack)
	{
	case EFinalBossAttack::ForwardCleave: return FMath::Max(0.0f, ForwardCleaveRecoveryDuration);
	case EFinalBossAttack::PointBlankAoE: return FMath::Max(0.0f, PointBlankAOERecoveryDuration);
	case EFinalBossAttack::LongDash: return FMath::Max(0.0f, LongDashRecoveryDuration);
	case EFinalBossAttack::GroundPursuit: return FMath::Max(0.0f, GroundPursuitRecoveryDuration);
	default: return 0.0f;
	}
}

void AFinalBossBase::BeginAttack(EFinalBossAttack Attack)
{
	ACharacterBase* Player = PlayerController ? Cast<ACharacterBase>(PlayerController->GetPawn()) : nullptr;
	if (!Player) { StopBossCombat(); return; }
	CurrentAttack = Attack;
	LockedAttackOrigin = GetActorLocation();
	LockedAttackDirection = (Player->GetActorLocation() - LockedAttackOrigin).GetSafeNormal2D();
	if (LockedAttackDirection.IsNearlyZero()) LockedAttackDirection = GetActorForwardVector().GetSafeNormal2D();
	SetActorRotation(LockedAttackDirection.Rotation());
	bAttackExecuted = false;
	bGroundPursuitWindowActive = Attack == EFinalBossAttack::GroundPursuit;
	BossState = EFinalBossState::Windup;
	StateElapsed = 0.0f;
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
	case EFinalBossAttack::ForwardCleave: return ForwardCleaveMontage;
	case EFinalBossAttack::PointBlankAoE: return PointBlankAOEMontage;
	case EFinalBossAttack::LongDash: return LongDashMontage;
	case EFinalBossAttack::GroundPursuit: return GroundPursuitMontage;
	default: return nullptr;
	}
}

bool AFinalBossBase::PlayCurrentAttackMontage()
{
	UAnimMontage* Montage = GetMontageForAttack(CurrentAttack);
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (!Montage || !AnimInstance || AnimInstance->Montage_Play(Montage) <= 0.0f) return false;
	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &AFinalBossBase::HandleAttackMontageEnded);
	AnimInstance->Montage_SetEndDelegate(EndDelegate, Montage);
	return true;
}

void AFinalBossBase::HandleBossTelegraphStart()
{
	if (!bCombatEnabled || IsDead() || BossState != EFinalBossState::Windup) return;
	if (CurrentAttack == EFinalBossAttack::ForwardCleave) { ShowRectangleTelegraph(ForwardAttackLength, ForwardAttackWidth); StartRectangleFill(); }
	else if (CurrentAttack == EFinalBossAttack::LongDash) { ShowRectangleTelegraph(DashDistance, DashWidth); StartRectangleFill(); }
	else if (CurrentAttack == EFinalBossAttack::PointBlankAoE) { ShowCircleTelegraph(AoERadius); StartRectangleFill(); }
}

void AFinalBossBase::HandleBossAttackExecute()
{
	if (!bCombatEnabled || IsDead() || bAttackExecuted || BossState == EFinalBossState::Recovery || BossState == EFinalBossState::Cooldown) return;
	bAttackExecuted = true;
	StopRectangleFill();
	SetRectangleFill(1.0f);
	OnBossAttackImpact.Broadcast(CurrentAttack);
	if (CurrentAttack == EFinalBossAttack::ForwardCleave) DamagePlayerInRectangle(ForwardAttackLength, ForwardAttackWidth, ForwardAttackDamage);
	else if (CurrentAttack == EFinalBossAttack::PointBlankAoE) DamagePlayerInCircle(AoERadius, AoEDamage);
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
	if (bCombatEnabled && !IsDead() && CurrentAttack == EFinalBossAttack::GroundPursuit && bGroundPursuitWindowActive) SpawnGroundCircle();
}

void AFinalBossBase::HandleAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != GetMontageForAttack(CurrentAttack) || BossState == EFinalBossState::Dead) return;
	bGroundPursuitWindowActive = false;
	StopDash();
	HideAttackTelegraphs();
	if (bDebugAttackTelegraphs) UE_LOG(LogTemp, Log, TEXT("[BossAttack] Montage ended Attack=%d Interrupted=%d"), static_cast<int32>(CurrentAttack), bInterrupted ? 1 : 0);
	if (bCombatEnabled) BeginRecovery();
}

void AFinalBossBase::BeginRecovery()
{
	HideAttackTelegraphs();
	StopDash();
	BossState = EFinalBossState::Recovery;
	StateElapsed = 0.0f;
}

void AFinalBossBase::ShowRectangleTelegraph(float Length, float Width)
{
	if (!GetWorld() || !GroundTelegraphClass) return;
	if (IsValid(ActiveAttackTelegraph)) ActiveAttackTelegraph->CancelTelegraph();
	const float GroundZ = GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 4.0f;
	FVector Center = LockedAttackOrigin + LockedAttackDirection * Length * 0.5f;
	Center.Z = GroundZ;
	FActorSpawnParameters Params;
	Params.Owner = this;
	ActiveAttackTelegraph = GetWorld()->SpawnActor<ABossGroundTelegraph>(GroundTelegraphClass, Center, LockedAttackDirection.Rotation(), Params);
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

void AFinalBossBase::DamagePlayerInRectangle(float Length, float Width, float Damage)
{
	const ACharacterBase* Player = PlayerController ? Cast<ACharacterBase>(PlayerController->GetPawn()) : nullptr;
	if (!Player) return;
	const FVector Delta = Player->GetActorLocation() - LockedAttackOrigin;
	const float Forward = FVector::DotProduct(Delta, LockedAttackDirection);
	const FVector Right = FVector::CrossProduct(FVector::UpVector, LockedAttackDirection);
	if (Forward >= 0.0f && Forward <= Length && FMath::Abs(FVector::DotProduct(Delta, Right)) <= Width * 0.5f) PlayerController->ApplyDamageToPlayer(Damage);
}

void AFinalBossBase::DamagePlayerInCircle(float Radius, float Damage)
{
	const ACharacterBase* Player = PlayerController ? Cast<ACharacterBase>(PlayerController->GetPawn()) : nullptr;
	if (Player && FVector::DistSquared2D(Player->GetActorLocation(), GetActorLocation()) <= FMath::Square(Radius)) PlayerController->ApplyDamageToPlayer(Damage);
}

void AFinalBossBase::StartDash()
{
	BossState = EFinalBossState::AttackActive;
	DashElapsed = 0.0f;
	DashStart = GetActorLocation();
	DashEnd = DashStart + LockedAttackDirection * DashDistance;
	bDashHitPlayer = false;
	HideAttackTelegraphs();
}

void AFinalBossBase::StopDash()
{
	if (CurrentAttack == EFinalBossAttack::LongDash && BossState == EFinalBossState::AttackActive) BossState = EFinalBossState::Windup;
}

void AFinalBossBase::UpdateDash(float DeltaSeconds)
{
	DashElapsed += DeltaSeconds;
	const FVector Previous = GetActorLocation();
	const float Alpha = FMath::Clamp(DashElapsed / FMath::Max(0.01f, DashTravelDuration), 0.0f, 1.0f);
	SetActorLocation(FMath::Lerp(DashStart, DashEnd, Alpha), true);
	if (!bDashHitPlayer && PlayerController)
	{
		if (const ACharacterBase* Player = Cast<ACharacterBase>(PlayerController->GetPawn()))
		{
			const FVector Closest = FMath::ClosestPointOnSegment(Player->GetActorLocation(), Previous, GetActorLocation());
			if (FVector::DistSquared2D(Closest, Player->GetActorLocation()) <= FMath::Square(DashWidth * 0.5f)) { bDashHitPlayer = true; PlayerController->ApplyDamageToPlayer(DashDamage); }
		}
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
	HideAttackTelegraphs();
	if (UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
	{
		if (UAnimMontage* Montage = GetMontageForAttack(CurrentAttack)) AnimInstance->Montage_Stop(0.1f, Montage);
	}
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
