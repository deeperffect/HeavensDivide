#include "GoblinBombEnemy.h"

#include "HealthComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AGoblinBombEnemy::AGoblinBombEnemy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AttackMontage = nullptr;
	AttackShape = ETankSlamAttackShape::Circle;
	AttackRange = 250.0f;
	AttackAoERadius = 375.0f;
	TelegraphWindupDuration = 1.0f;
	WindupTrackingRotationSpeed = 0.0f;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CircleTelegraph(TEXT("/Game/Assets/EnemyCharacters/M_AttackTelegraphCircle"));
	AttackTelegraphMaterial = CircleTelegraph.Object;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FlashMaterial(TEXT("/Game/HeavensDivide/Materials/M_BombChargeFlash"));
	BombFlashMaterial = FlashMaterial.Object;
}

void AGoblinBombEnemy::StartAttack()
{
	if (bIsDead || bDetonated || bIsAttacking || bGameplaySuspended || !GetWorld()
		|| IsStressTestCombatDisabled() || IsPlayerTargetDead()
		|| !IsValid(CurrentTarget) || !IsTargetInAttackRange()) return;
	if (!CanStartAttackNow())
	{
		StartAttackTimer();
		return;
	}
	StopAttackTimer();
	StopEnemyMovement();
	FaceTarget();
	bIsAttacking = true;
	MarkAttackStarted();
	UpdateAnimationBudgetSignificance();
	// Do not use a montage from an old Blueprint default to time this enemy's fuse.
	AttackMontage = nullptr;
	HandleAttackCommitted();
	StartChargePresentation();
	GetWorldTimerManager().SetTimer(DetonationTimer, this, &AGoblinBombEnemy::Detonate,
		FMath::Max(0.01f, TelegraphWindupDuration), false);
}

void AGoblinBombEnemy::StopEnemyBehavior()
{
	StopChargePresentation();
	GetWorldTimerManager().ClearTimer(DetonationTimer);
	Super::StopEnemyBehavior();
}

void AGoblinBombEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopChargePresentation();
	GetWorldTimerManager().ClearTimer(DetonationTimer);
	Super::EndPlay(EndPlayReason);
}

void AGoblinBombEnemy::StartChargePresentation()
{
	if (GetNetMode() == NM_DedicatedServer || bChargePresentationActive) return;
	bChargePresentationActive = true;
	ChargeStartTime = GetWorld()->GetTimeSeconds();
	if (GetMesh())
	{
		PreChargeMeshRotation = GetMesh()->GetRelativeRotation().Quaternion();
		bPreChargePauseAnims = GetMesh()->bPauseAnims;
		GetMesh()->bPauseAnims = true;
	}
	TInlineComponentArray<UStaticMeshComponent*> StaticMeshes(this);
	for (UStaticMeshComponent* StaticMesh : StaticMeshes)
	{
		if (StaticMesh->GetFName() == BombMeshName || StaticMesh->ComponentHasTag(BombMeshName))
		{
			ChargeBombMesh = StaticMesh;
			break;
		}
	}
	if (ChargeBombMesh && BombFlashMaterial)
	{
		PreChargeBombMaterials.Reset();
		for (int32 Slot = 0; Slot < ChargeBombMesh->GetNumMaterials(); ++Slot)
			PreChargeBombMaterials.Add(ChargeBombMesh->GetMaterial(Slot));
		BombFlashMID = UMaterialInstanceDynamic::Create(BombFlashMaterial, this);
		BombFlashMID->SetVectorParameterValue(TEXT("FlashColor"), BombFlashColor);
		bBombFlashOn = false;
	}
	UpdateChargePresentation();
	GetWorldTimerManager().SetTimer(ChargePresentationTimer, this,
		&AGoblinBombEnemy::UpdateChargePresentation, 1.0f / 30.0f, true);
}

void AGoblinBombEnemy::UpdateChargePresentation()
{
	if (!bChargePresentationActive) return;
	if (bIsDead || !bIsAttacking || bGameplaySuspended)
	{
		StopChargePresentation();
		return;
	}
	StopEnemyMovement();
	const float Duration = FMath::Max(0.01f, TelegraphWindupDuration);
	const float Elapsed = FMath::Clamp(static_cast<float>(GetWorld()->GetTimeSeconds() - ChargeStartTime), 0.0f, Duration);
	const float Phase = 2.0f * PI * ChargeWobbleFrequency * Elapsed;
	if (GetMesh())
	{
		const FRotator Wobble(FMath::Sin(Phase * 0.7f) * ChargeWobbleDegrees * 0.5f,
			0.0f, FMath::Sin(Phase) * ChargeWobbleDegrees);
		GetMesh()->SetRelativeRotation(Wobble.Quaternion() * PreChargeMeshRotation);
	}
	if (BombFlashMID)
	{
		// Integrate the changing frequency so the blink accelerates without phase jumps.
		const float Cycles = BombFlashStartFrequency * Elapsed
			+ 0.5f * (BombFlashEndFrequency - BombFlashStartFrequency) * Elapsed * Elapsed / Duration;
		const bool bFlashOn = FMath::Frac(Cycles) < 0.5f;
		if (ChargeBombMesh && bFlashOn != bBombFlashOn)
		{
			bBombFlashOn = bFlashOn;
			for (int32 Slot = 0; Slot < PreChargeBombMaterials.Num(); ++Slot)
				ChargeBombMesh->SetMaterial(Slot, bFlashOn ? BombFlashMID.Get() : PreChargeBombMaterials[Slot].Get());
		}
	}
}

void AGoblinBombEnemy::StopChargePresentation()
{
	GetWorldTimerManager().ClearTimer(ChargePresentationTimer);
	if (!bChargePresentationActive) return;
	bChargePresentationActive = false;
	if (GetMesh())
	{
		GetMesh()->SetRelativeRotation(PreChargeMeshRotation);
		GetMesh()->bPauseAnims = bPreChargePauseAnims;
	}
	if (ChargeBombMesh && BombFlashMID)
	{
		for (int32 Slot = 0; Slot < PreChargeBombMaterials.Num(); ++Slot)
			ChargeBombMesh->SetMaterial(Slot, PreChargeBombMaterials[Slot]);
	}
	ChargeBombMesh = nullptr;
	BombFlashMID = nullptr;
	PreChargeBombMaterials.Reset();
	bBombFlashOn = false;
}

void AGoblinBombEnemy::Detonate()
{
	if (bIsDead || bDetonated || !bIsAttacking || IsStressTestCombatDisabled() || IsPlayerTargetDead()) return;
	bDetonated = true;
	GetWorldTimerManager().ClearTimer(DetonationTimer);
	// Shared slam resolves damage against the active player at impact, so escaping
	// the telegraph avoids damage but does not cancel the committed explosion.
	Super::ExecuteAttackHit();
	if (ExplosionNiagaraSystem && GetNetMode() != NM_DedicatedServer)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ExplosionNiagaraSystem,
			GetActorLocation() + ExplosionSpawnOffset, GetActorRotation(), FVector::OneVector,
			true, true, ENCPoolMethod::AutoRelease);
	}
	// Self-destruction is intentional even if incoming damage was disabled. Keep HP,
	// rewards, death notifications, and enemy counts on the normal health/death path.
	if (HealthComponent)
	{
		HealthComponent->SetDamageEnabled(true);
		HealthComponent->ApplyDamage(HealthComponent->GetCurrentHealth());
	}
	if (!bIsDead) HandleDeath();
}

void AGoblinBombEnemy::BeginDeathPresentation_Implementation()
{
	if (bDetonated)
	{
		// Explosion is the presentation. Remove the body and its attached Bomb mesh
		// immediately; the pooled world-space Niagara continues independently.
		DestroyAfterDeath();
		return;
	}
	Super::BeginDeathPresentation_Implementation();
}

