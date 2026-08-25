// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShadowClone.h"

#include "AutoAttackComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AnimNotify_SpawnAutoAttackProjectile.h"
#include "EnemyBase.h"
#include "NinjaCharacter.h"
#include "PlayerUpgradeComponent.h"
#include "SurvivorPlayerController.h"
#include "UpgradeDefinition.h"

AShadowClone::AShadowClone()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	CloneMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CloneMesh"));
	CloneMesh->SetupAttachment(Root);
	CloneMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CloneMesh->SetGenerateOverlapEvents(false);
	CloneMesh->SetRenderCustomDepth(true);
	CloneMesh->SetCustomDepthStencilValue(2);
	SetActorEnableCollision(false);
}

void AShadowClone::InitializeShadowClone(ANinjaCharacter* InSourceNinja, ASurvivorPlayerController* InController, int32 InAttackQuota)
{
	SourceNinja = InSourceNinja;
	SourceController = InController;
	SourceAttack = InSourceNinja ? InSourceNinja->FindComponentByClass<UAutoAttackComponent>() : nullptr;
	RemainingAttacks = FMath::Max(1, InAttackQuota);

	if (InSourceNinja && InSourceNinja->GetMesh())
	{
		USkeletalMeshComponent* SourceMesh = InSourceNinja->GetMesh();
		CloneMesh->SetSkeletalMeshAsset(SourceMesh->GetSkeletalMeshAsset());
		CloneMesh->SetRelativeTransform(SourceMesh->GetRelativeTransform());
		for (int32 Index = 0; Index < SourceMesh->GetNumMaterials(); ++Index) CloneMesh->SetMaterial(Index, SourceMesh->GetMaterial(Index));
		// The clone needs its own AnimInstance so it can explicitly play the Ninja
		// attack montage instead of mirroring the live Ninja's locomotion pose.
		CloneMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		CloneMesh->SetAnimInstanceClass(SourceMesh->GetAnimClass());
	}

	OnShadowCloneSpawned.Broadcast(this);
	GetWorldTimerManager().SetTimer(AttackTimer, this, &AShadowClone::BeginAttack, FMath::Max(0.01f, InitialAttackDelay), false);
	GetWorldTimerManager().SetTimer(SafetyTimer, this, &AShadowClone::FinishClone, FMath::Max(0.1f, SafetyLifetime), false);
}

void AShadowClone::BeginAttack()
{
	if (bFinished || bAttackQuotaFinished || bAttackInProgress || !SourceController.IsValid() || SourceController->IsPlayerDead() || !SourceNinja.IsValid() || !SourceAttack.IsValid())
	{
		FinishClone();
		return;
	}

	const FVector Origin = GetActorTransform().TransformPosition(ProjectileOriginOffset);
	AEnemyBase* Target = SourceAttack->FindAssistTargetNearLocation(Origin, AttackRange);
	if (!Target || Target->IsDead())
	{
		GetWorldTimerManager().SetTimer(AttackTimer, this, &AShadowClone::BeginAttack, FMath::Max(0.01f, TargetRetryInterval), false);
		return;
	}

	FVector Facing = Target->GetActorLocation() - GetActorLocation();
	Facing.Z = 0.0f;
	if (Facing.Normalize()) SetActorRotation(FRotator(0.0f, Facing.Rotation().Yaw, 0.0f));

	UAnimMontage* Montage = SourceAttack->GetAttackMontageForShadowClone();
	UAnimInstance* AnimInstance = CloneMesh ? CloneMesh->GetAnimInstance() : nullptr;
	if (!Montage || !AnimInstance)
	{
		// The configured montage is authoritative. Without it, retry rather than
		// firing a visually disconnected projectile.
		GetWorldTimerManager().SetTimer(AttackTimer, this, &AShadowClone::BeginAttack, FMath::Max(0.01f, TargetRetryInterval), false);
		return;
	}

	bAttackInProgress = true;
	const float DesiredCadence = GetAttackInterval();
	const float PlayRate = FMath::Max(1.0f, Montage->GetPlayLength() / FMath::Max(0.01f, DesiredCadence));
	ActiveMontagePlayRate = PlayRate;
	AnimInstance->Montage_Stop(0.0f, Montage);
	if (AnimInstance->Montage_Play(Montage, PlayRate) <= 0.0f)
	{
		bAttackInProgress = false;
		GetWorldTimerManager().SetTimer(AttackTimer, this, &AShadowClone::BeginAttack, FMath::Max(0.01f, TargetRetryInterval), false);
	}
}

void AShadowClone::HandleAttackProjectileNotify()
{
	if (bFinished || bAttackQuotaFinished || !bAttackInProgress || !SourceAttack.IsValid()) return;
	bAttackInProgress = false;
	const FVector Origin = GetActorTransform().TransformPosition(ProjectileOriginOffset);
	if (!SourceAttack->SpawnShadowCloneVolley(Origin, AttackRange))
	{
		GetWorldTimerManager().SetTimer(AttackTimer, this, &AShadowClone::BeginAttack, FMath::Max(0.01f, TargetRetryInterval), false);
		return;
	}

	--RemainingAttacks;
	OnShadowCloneAttack.Broadcast(this, RemainingAttacks);
	if (RemainingAttacks <= 0)
	{
		BeginFinalLinger();
		return;
	}
	UAnimMontage* Montage = SourceAttack->GetAttackMontageForShadowClone();
	const float NextNotifyDelay = GetProjectileNotifyDelay(Montage, ActiveMontagePlayRate);
	const float NextAttackStartDelay = FMath::Max(0.01f, GetAttackInterval() - NextNotifyDelay);
	GetWorldTimerManager().SetTimer(AttackTimer, this, &AShadowClone::BeginAttack, NextAttackStartDelay, false);
}

void AShadowClone::BeginFinalLinger()
{
	if (bFinished || bAttackQuotaFinished) return;
	bAttackQuotaFinished = true;
	bAttackInProgress = false;
	GetWorldTimerManager().ClearTimer(AttackTimer);
	GetWorldTimerManager().ClearTimer(SafetyTimer);
	OnShadowCloneBeginDisappear.Broadcast(this);
	if (PostAttackLingerDuration <= KINDA_SMALL_NUMBER) FinishClone();
	else GetWorldTimerManager().SetTimer(LingerTimer, this, &AShadowClone::FinishClone, PostAttackLingerDuration, false);
}

float AShadowClone::GetAttackInterval() const
{
	float SpeedBonus = 0.0f;
	const UPlayerUpgradeComponent* Upgrades = SourceController.IsValid() ? SourceController->GetPlayerUpgrades() : nullptr;
	const UUpgradeDefinition* Frenzy = Upgrades ? Upgrades->GetAcquiredUpgradeWithSpecialEffect(EUpgradeSpecialEffect::AfterimageFrenzy) : nullptr;
	if (Frenzy) SpeedBonus = FMath::Max(0.0f, Frenzy->AfterimageFrenzyAttackSpeedBonus);
	return FMath::Max(0.01f, BaseAttackInterval / (1.0f + SpeedBonus));
}

float AShadowClone::GetProjectileNotifyDelay(const UAnimMontage* Montage, float PlayRate) const
{
	if (!Montage) return 0.0f;
	for (const FAnimNotifyEvent& Event : Montage->Notifies)
	{
		if (Event.Notify && Event.Notify->IsA<UAnimNotify_SpawnAutoAttackProjectile>())
		{
			return Event.GetTriggerTime() / FMath::Max(0.01f, PlayRate);
		}
	}
	return 0.0f;
}

void AShadowClone::FinishClone()
{
	if (bFinished) return;
	bFinished = true;
	GetWorldTimerManager().ClearTimer(AttackTimer);
	GetWorldTimerManager().ClearTimer(SafetyTimer);
	GetWorldTimerManager().ClearTimer(LingerTimer);
	OnShadowCloneFinished.Broadcast(this);
	Destroy();
}

void AShadowClone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(AttackTimer);
	GetWorldTimerManager().ClearTimer(SafetyTimer);
	GetWorldTimerManager().ClearTimer(LingerTimer);
	Super::EndPlay(EndPlayReason);
}
