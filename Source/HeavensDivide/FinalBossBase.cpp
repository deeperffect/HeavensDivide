#include "FinalBossBase.h"

#include "BossGroundTelegraph.h"
#include "CharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "Components/StaticMeshComponent.h"
#include "HealthComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "SurvivorPlayerController.h"
#include "Animation/AnimInstance.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "UObject/ConstructorHelpers.h"

AFinalBossBase::AFinalBossBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	MoveSpeed = BossMoveSpeed;
	StopDistance = 350.0f;
	bUseCrowdSpread = false;
	bUseEnemySeparation = false;
	bDropsXP = false;
	XPReward = 0;
	GroundTelegraphClass = ABossGroundTelegraph::StaticClass();

	BossAttackTelegraphDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("BossAttackTelegraphDecal"));
	BossAttackTelegraphDecal->SetupAttachment(RootComponent);
	BossAttackTelegraphDecal->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	BossAttackTelegraphDecal->SetComponentTickEnabled(false);
	BossAttackTelegraphDecal->SetHiddenInGame(true);
	BossAttackTelegraphDecal->SetVisibility(false);
	CircleTelegraph = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CircleTelegraph"));
	CircleTelegraph->SetupAttachment(RootComponent);
	CircleTelegraph->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CircleTelegraph->SetCastShadow(false);
	CircleTelegraph->SetVisibility(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> RectangleMaterialAsset(TEXT("/Game/Assets/EnemyCharacters/M_AttackTelegraphBox.M_AttackTelegraphBox"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Game/HeavensDivide/Materials/M_SamuraiLaneIndicator.M_SamuraiLaneIndicator"));
	if (Cylinder.Succeeded()) CircleTelegraph->SetStaticMesh(Cylinder.Object);
	if (RectangleMaterialAsset.Succeeded())
	{
		RectangleTelegraphMaterial = RectangleMaterialAsset.Object;
		BossAttackTelegraphDecal->SetDecalMaterial(RectangleTelegraphMaterial);
	}
	if (Material.Succeeded()) TelegraphMaterial = Material.Object;
}

void AFinalBossBase::BeginPlay()
{
	Super::BeginPlay();
	if (HealthComponent) HealthComponent->SetMaxHealthPreservePercent(BossMaxHealth);
	MoveSpeed = BossMoveSpeed;
	ApplySpawnInstanceModifiers(1.0f, 1.0f, 1.0f);
	PlayerController = ResolvePlayerController();
	if (TelegraphMaterial)
	{
		CircleTelegraph->SetMaterial(0, TelegraphMaterial);
	}
	if (RectangleTelegraphMaterial)
	{
		RectangleMaterial = UMaterialInstanceDynamic::Create(RectangleTelegraphMaterial, this);
		if (RectangleMaterial) BossAttackTelegraphDecal->SetDecalMaterial(RectangleMaterial);
	}
	CircleMaterial = CircleTelegraph->CreateAndSetMaterialInstanceDynamic(0);
	for (UMaterialInstanceDynamic* Material : { RectangleMaterial.Get(), CircleMaterial.Get() })
	{
		if (Material)
		{
			Material->SetVectorParameterValue(TEXT("FillColor"), FLinearColor::Red);
			Material->SetScalarParameterValue(TEXT("FillAmount"), 0.0f);
		}
	}
	if (bAutoStartCombat) StartBossCombat();
}

void AFinalBossBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CleanupAttacks();
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
	CleanupAttacks();
	if (!IsDead()) BossState = EFinalBossState::Idle;
}

void AFinalBossBase::DebugForceAttack(EFinalBossAttack Attack)
{
	if (IsDead() || Attack == EFinalBossAttack::None) return;
	if (!bCombatEnabled) StartBossCombat();
	CleanupAttacks();
	BeginAttack(Attack);
}

void AFinalBossBase::DebugForceForwardCleave()
{
	DebugForceAttack(EFinalBossAttack::ForwardCleave);
}

void AFinalBossBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bCombatEnabled || IsDead()) return;
	if (!PlayerController) PlayerController = ResolvePlayerController();
	if (!PlayerController || PlayerController->IsPlayerDead())
	{
		StopBossCombat();
		return;
	}

	StateElapsed += DeltaSeconds;
	switch (BossState)
	{
	case EFinalBossState::Windup:
		UpdateTelegraph(FMath::Clamp(StateElapsed / CurrentWindupDuration, 0.0f, 1.0f));
		if (StateElapsed >= CurrentWindupDuration) ResolveWindup();
		break;
	case EFinalBossState::AttackActive:
		if (CurrentAttack == EFinalBossAttack::LongDash) UpdateDash(DeltaSeconds);
		else if (CurrentAttack == EFinalBossAttack::GroundPursuit) UpdateGroundPursuit(DeltaSeconds);
		break;
	case EFinalBossState::Recovery:
		if (StateElapsed >= RecoveryDuration)
		{
			BossState = EFinalBossState::Cooldown;
			StateElapsed = 0.0f;
		}
		break;
	case EFinalBossState::Cooldown:
		if (StateElapsed >= AttackCooldown) ChooseAttack();
		break;
	default:
		break;
	}
}

void AFinalBossBase::UpdateEnemyBehavior(float DeltaSeconds)
{
	if (!bCombatEnabled)
	{
		StopEnemyMovement();
	}
	else if (BossState == EFinalBossState::Idle || BossState == EFinalBossState::Cooldown)
	{
		Super::UpdateEnemyBehavior(DeltaSeconds);
	}
	else
	{
		StopEnemyMovement();
	}
}

bool AFinalBossBase::ShouldSkipMovement() const
{
	return Super::ShouldSkipMovement() || (bCombatEnabled && BossState != EFinalBossState::Idle && BossState != EFinalBossState::Cooldown);
}

void AFinalBossBase::ChooseAttack()
{
	TArray<EFinalBossAttack> Choices = { EFinalBossAttack::ForwardCleave, EFinalBossAttack::PointBlankAoE, EFinalBossAttack::LongDash, EFinalBossAttack::GroundPursuit };
	if (PreviousAttack != EFinalBossAttack::None && Choices.Num() > 1) Choices.Remove(PreviousAttack);
	BeginAttack(Choices[FMath::RandRange(0, Choices.Num() - 1)]);
}

void AFinalBossBase::BeginAttack(EFinalBossAttack Attack)
{
	ACharacterBase* Player = PlayerController ? Cast<ACharacterBase>(PlayerController->GetPawn()) : nullptr;
	if (!Player)
	{
		StopBossCombat();
		return;
	}
	CurrentAttack = Attack;
	if (bDebugAttackTelegraphs)
	{
		const TCHAR* AttackName = Attack == EFinalBossAttack::ForwardCleave ? TEXT("ForwardCleave")
			: Attack == EFinalBossAttack::PointBlankAoE ? TEXT("PointBlankAoE")
			: Attack == EFinalBossAttack::LongDash ? TEXT("LongDash")
			: Attack == EFinalBossAttack::GroundPursuit ? TEXT("GroundPursuit") : TEXT("None");
		UE_LOG(LogTemp, Warning, TEXT("[BossTelegraphDebug] BeginAttack Name=%s Value=%d"), AttackName, static_cast<int32>(Attack));
	}
	PreviousAttack = Attack;
	BossState = EFinalBossState::Windup;
	StateElapsed = 0.0f;
	LockedAttackOrigin = GetActorLocation();
	LockedAttackDirection = Player->GetActorLocation() - LockedAttackOrigin;
	LockedAttackDirection.Z = 0.0f;
	if (!LockedAttackDirection.Normalize()) LockedAttackDirection = GetActorForwardVector();
	SetActorRotation(LockedAttackDirection.Rotation());
	BossAttackTelegraphDecal->SetVisibility(false);
	CircleTelegraph->SetVisibility(false);

	switch (Attack)
	{
	case EFinalBossAttack::ForwardCleave:
		CurrentWindupDuration = ForwardAttackTelegraphDuration;
		ConfigureRectangle(ForwardAttackLength, ForwardAttackWidth);
		PlayAttackMontage(Attack1Montage);
		break;
	case EFinalBossAttack::PointBlankAoE:
		CurrentWindupDuration = AoETelegraphDuration;
		ConfigureCircle(AoERadius);
		PlayAttackMontage(Attack2Montage);
		break;
	case EFinalBossAttack::LongDash:
		CurrentWindupDuration = DashTelegraphDuration;
		ConfigureRectangle(DashDistance, DashWidth);
		PlayAttackMontage(Attack3Montage);
		break;
	case EFinalBossAttack::GroundPursuit:
		CurrentWindupDuration = 0.01f;
		PlayAttackMontage(Attack4Montage);
		break;
	default: break;
	}
	OnBossAttackStarted.Broadcast(Attack);
}

void AFinalBossBase::ResolveWindup()
{
	if (bDebugAttackTelegraphs && (CurrentAttack == EFinalBossAttack::ForwardCleave || CurrentAttack == EFinalBossAttack::LongDash))
	{
		UE_LOG(LogTemp, Warning, TEXT("[BossTelegraphDebug] Resolve Attack=%d Visible=%d HiddenInGame=%d Registered=%d Material=%s MID=%s Fill=1.0"),
			static_cast<int32>(CurrentAttack), BossAttackTelegraphDecal->IsVisible() ? 1 : 0,
			BossAttackTelegraphDecal->bHiddenInGame ? 1 : 0, BossAttackTelegraphDecal->IsRegistered() ? 1 : 0,
			*GetNameSafe(BossAttackTelegraphDecal->GetDecalMaterial()), *GetNameSafe(RectangleMaterial));
	}
	UpdateTelegraph(1.0f);
	OnBossAttackImpact.Broadcast(CurrentAttack);
	if (CurrentAttack == EFinalBossAttack::ForwardCleave)
	{
		DamagePlayerInRectangle(ForwardAttackLength, ForwardAttackWidth, ForwardAttackDamage);
		BeginRecovery();
	}
	else if (CurrentAttack == EFinalBossAttack::PointBlankAoE)
	{
		DamagePlayerInCircle(AoERadius, AoEDamage);
		BeginRecovery();
	}
	else if (CurrentAttack == EFinalBossAttack::LongDash)
	{
		BossAttackTelegraphDecal->SetHiddenInGame(true);
		BossAttackTelegraphDecal->SetVisibility(false);
		BossState = EFinalBossState::AttackActive;
		StateElapsed = 0.0f;
		DashStart = GetActorLocation();
		DashEnd = DashStart + LockedAttackDirection * DashDistance;
		bDashHitPlayer = false;
	}
	else if (CurrentAttack == EFinalBossAttack::GroundPursuit)
	{
		BossState = EFinalBossState::AttackActive;
		StateElapsed = 0.0f;
		GroundSpawnElapsed = GroundCircleSpawnInterval;
		GroundCirclesSpawned = 0;
	}
}

void AFinalBossBase::BeginRecovery()
{
	BossAttackTelegraphDecal->SetHiddenInGame(true);
	BossAttackTelegraphDecal->SetVisibility(false);
	CircleTelegraph->SetVisibility(false);
	BossState = EFinalBossState::Recovery;
	StateElapsed = 0.0f;
}

void AFinalBossBase::UpdateTelegraph(float Alpha)
{
	if (RectangleMaterial) RectangleMaterial->SetScalarParameterValue(TEXT("FillAmount"), Alpha);
	if (CircleMaterial) CircleMaterial->SetScalarParameterValue(TEXT("FillAmount"), Alpha);
	if (CurrentAttack == EFinalBossAttack::PointBlankAoE && CircleTelegraph->IsVisible())
	{
		const float FilledRadius = AoERadius * FMath::Max(0.001f, Alpha);
		CircleTelegraph->SetRelativeScale3D(FVector(FilledRadius / 50.0f, FilledRadius / 50.0f, 0.025f));
	}
}

void AFinalBossBase::ConfigureRectangle(float Length, float Width)
{
	if (!RectangleMaterial)
	{
		if (RectangleTelegraphMaterial)
		{
			RectangleMaterial = UMaterialInstanceDynamic::Create(RectangleTelegraphMaterial, this);
			if (RectangleMaterial) BossAttackTelegraphDecal->SetDecalMaterial(RectangleMaterial);
		}
	}
	BossAttackTelegraphDecal->SetHiddenInGame(false);
	BossAttackTelegraphDecal->SetVisibility(true);
	BossAttackTelegraphDecal->SetRelativeLocation(FVector(Length * 0.5f, 0.0f, -GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 4.0f));
	BossAttackTelegraphDecal->DecalSize = FVector(64.0f, Width * 0.5f, Length * 0.5f);
	UpdateTelegraph(0.0f);
	if (bDebugAttackTelegraphs)
	{
		const FVector DebugCenter = LockedAttackOrigin + LockedAttackDirection * Length * 0.5f;
		DrawDebugBox(GetWorld(), DebugCenter, FVector(Length * 0.5f, Width * 0.5f, 8.0f), LockedAttackDirection.Rotation().Quaternion(), FColor::Cyan, false, FMath::Max(3.0f, CurrentWindupDuration), 0, 5.0f);
		UE_LOG(LogTemp, Warning, TEXT("[BossTelegraphDebug] Show Attack=%d Visible=%d HiddenInGame=%d Registered=%d Material=%s MID=%s Location=%s Rotation=%s DecalSize=%s"),
			static_cast<int32>(CurrentAttack), BossAttackTelegraphDecal->IsVisible() ? 1 : 0,
			BossAttackTelegraphDecal->bHiddenInGame ? 1 : 0, BossAttackTelegraphDecal->IsRegistered() ? 1 : 0,
			*GetNameSafe(BossAttackTelegraphDecal->GetDecalMaterial()), *GetNameSafe(RectangleMaterial),
			*BossAttackTelegraphDecal->GetComponentLocation().ToCompactString(), *BossAttackTelegraphDecal->GetComponentRotation().ToCompactString(),
			*BossAttackTelegraphDecal->DecalSize.ToCompactString());
	}
}

void AFinalBossBase::ConfigureCircle(float Radius)
{
	CircleTelegraph->SetVisibility(true);
	CircleTelegraph->SetRelativeLocation(FVector(0.0f, 0.0f, -GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 3.0f));
	CircleTelegraph->SetRelativeScale3D(FVector(Radius / 50.0f, Radius / 50.0f, 0.025f));
	UpdateTelegraph(0.0f);
}

void AFinalBossBase::DamagePlayerInRectangle(float Length, float Width, float Damage)
{
	const ACharacterBase* Player = PlayerController ? Cast<ACharacterBase>(PlayerController->GetPawn()) : nullptr;
	if (!Player) return;
	const FVector Delta = Player->GetActorLocation() - LockedAttackOrigin;
	const float Forward = FVector::DotProduct(Delta, LockedAttackDirection);
	const FVector Right = FVector::CrossProduct(FVector::UpVector, LockedAttackDirection);
	const float Side = FMath::Abs(FVector::DotProduct(Delta, Right));
	if (Forward >= 0.0f && Forward <= Length && Side <= Width * 0.5f) PlayerController->ApplyDamageToPlayer(Damage);
}

void AFinalBossBase::DamagePlayerInCircle(float Radius, float Damage)
{
	const ACharacterBase* Player = PlayerController ? Cast<ACharacterBase>(PlayerController->GetPawn()) : nullptr;
	if (Player && FVector::DistSquared2D(Player->GetActorLocation(), GetActorLocation()) <= FMath::Square(Radius)) PlayerController->ApplyDamageToPlayer(Damage);
}

void AFinalBossBase::UpdateDash(float DeltaSeconds)
{
	const FVector PreviousLocation = GetActorLocation();
	const float Alpha = FMath::Clamp(StateElapsed / FMath::Max(0.01f, DashTravelDuration), 0.0f, 1.0f);
	const FVector DesiredLocation = FMath::Lerp(DashStart, DashEnd, Alpha);
	SetActorLocation(DesiredLocation, true);
	if (!bDashHitPlayer && PlayerController)
	{
		if (const ACharacterBase* Player = Cast<ACharacterBase>(PlayerController->GetPawn()))
		{
			const FVector Closest = FMath::ClosestPointOnSegment(Player->GetActorLocation(), PreviousLocation, GetActorLocation());
			if (FVector::DistSquared2D(Closest, Player->GetActorLocation()) <= FMath::Square(DashWidth * 0.5f))
			{
				bDashHitPlayer = true;
				PlayerController->ApplyDamageToPlayer(DashDamage);
			}
		}
	}
	if (Alpha >= 1.0f) BeginRecovery();
}

void AFinalBossBase::UpdateGroundPursuit(float DeltaSeconds)
{
	GroundSpawnElapsed += DeltaSeconds;
	if (GroundCirclesSpawned < GroundCircleCount && GroundSpawnElapsed >= GroundCircleSpawnInterval)
	{
		GroundSpawnElapsed -= GroundCircleSpawnInterval;
		SpawnGroundCircle();
	}
	if (GroundCirclesSpawned >= GroundCircleCount && StateElapsed >= (GroundCircleCount - 1) * GroundCircleSpawnInterval + GroundCircleTelegraphDuration)
	{
		BeginRecovery();
	}
}

void AFinalBossBase::SpawnGroundCircle()
{
	ACharacterBase* Player = PlayerController ? Cast<ACharacterBase>(PlayerController->GetPawn()) : nullptr;
	if (!Player || !GroundTelegraphClass || !GetWorld()) return;
	FActorSpawnParameters Params;
	Params.Owner = this;
	ABossGroundTelegraph* Telegraph = GetWorld()->SpawnActor<ABossGroundTelegraph>(GroundTelegraphClass, Player->GetActorLocation(), FRotator::ZeroRotator, Params);
	if (Telegraph)
	{
		Telegraph->InitializeTelegraph(PlayerController, GroundCircleRadius, GroundCircleTelegraphDuration, GroundCircleDamage, TelegraphMaterial);
		ActiveGroundTelegraphs.Add(Telegraph);
	}
	++GroundCirclesSpawned;
}

void AFinalBossBase::PlayAttackMontage(UAnimMontage* Montage)
{
	if (Montage && GetMesh() && GetMesh()->GetAnimInstance()) GetMesh()->GetAnimInstance()->Montage_Play(Montage);
}

void AFinalBossBase::CleanupAttacks()
{
	BossAttackTelegraphDecal->SetHiddenInGame(true);
	BossAttackTelegraphDecal->SetVisibility(false);
	CircleTelegraph->SetVisibility(false);
	for (ABossGroundTelegraph* Telegraph : ActiveGroundTelegraphs)
	{
		if (IsValid(Telegraph)) Telegraph->CancelTelegraph();
	}
	ActiveGroundTelegraphs.Reset();
	StopEnemyMovement();
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
	CleanupAttacks();
	OnBossDefeated.Broadcast(this);
	Super::HandleDeath();
}
