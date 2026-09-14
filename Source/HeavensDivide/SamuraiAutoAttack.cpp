#include "SwapPresentationComponent.h"
// Copyright Epic Games, Inc. All Rights Reserved.

#include "AutoAttackComponent.h"
#include "NinjaBuildComponent.h"
#include "SurvivorAbilityComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AttackProjectileBase.h"
#include "CharacterStatsComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "EnemyBase.h"
#include "EnemyStatusEffectComponent.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Character.h"
#include "HAL/IConsoleManager.h"
#include "HealthComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NinjaCharacter.h"
#include "PlayerUpgradeComponent.h"
#include "SamuraiBladeWave.h"
#include "SamuraiCharacter.h"
#include "SharedPlayerStatsComponent.h"
#include "SurvivorPlayerController.h"
#include "TimerManager.h"

namespace SamuraiAutoAttackIds
{
static const FName MarkedBlade(TEXT("MarkedBlade"));
static const FName BleedingEdge(TEXT("BleedingEdge"));
} // namespace SamuraiAutoAttackIds
static UPlayerUpgradeComponent *ResolveSamuraiUpgrades(const UObject *WorldContextObject,
                                                                             const AActor *PlayerCharacter)
{
    const ASurvivorPlayerController *SurvivorController =
        Cast<ASurvivorPlayerController>(PlayerCharacter ? PlayerCharacter->GetOwner() : nullptr);
    if (!SurvivorController)
    {
        SurvivorController =
            Cast<ASurvivorPlayerController>(UGameplayStatics::GetPlayerController(WorldContextObject, 0));
    }

    return SurvivorController ? SurvivorController->GetPlayerUpgrades() : nullptr;
}

bool UAutoAttackComponent::ExecuteMeleeAttackTrace()
{
    if (!OwnerCharacter || !GetWorld())
    {
        UE_LOG(LogTemp, Warning, TEXT("AutoAttack trace skipped: owner/world invalid."));
        return false;
    }

    if (bActiveAttackIsAssist && OwnerCharacter->GetOwner())
        if (auto *Abilities = OwnerCharacter->GetOwner()->FindComponentByClass<USurvivorAbilityComponent>())
            return Abilities->ExecuteSetupAssist(OwnerCharacter);

    if (bActiveAttackIsAssist && !ProjectileClass)
    {
        AEnemyBase *AssistTarget = CurrentAttackTarget.Get();
        if (!AssistTarget || AssistTarget->IsDead() || !IsTargetInCurrentMeleeReach(AssistTarget))
        {
            AssistTarget =
                FindAssistTargetNearLocation(OwnerCharacter->GetActorLocation(), GetEffectiveTargetingRange());
            CurrentAttackTarget = AssistTarget;
        }

        if (!AssistTarget || AssistTarget->IsDead() || !IsTargetInCurrentMeleeReach(AssistTarget))
        {
            return false;
        }
    }

    FVector AttackForward = OwnerCharacter->GetVisualForwardVector();
    AttackForward.Z = 0.0f;
    if (!AttackForward.Normalize())
    {
        UE_LOG(LogTemp, Warning, TEXT("AutoAttack trace skipped: visual forward invalid."));
        return false;
    }

    const FVector AttackOrigin = OwnerCharacter->GetActorLocation();
    const UUpgradeDefinition *GrandEntrance = GetReadyGrandEntranceUpgrade();
    const FVector HitboxCenter = GrandEntrance ? AttackOrigin : AttackOrigin + AttackForward * AttackForwardOffset;
    const float EffectiveAttackRadius =
        GrandEntrance ? GetGrandEntranceRadius(GrandEntrance) : GetEffectiveAttackRadius();
    if (GrandEntrance)
    {
        // Spend only at the committed swing, including a legitimate swing that misses.
        bGrandEntranceReady = false;
        FActorSpawnParameters VisualParams;
        VisualParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        if (auto *Visual = GetWorld()->SpawnActor<AAbilityAccent>(AttackOrigin - FVector(0, 0, 70),
                                                                  FRotator::ZeroRotator, VisualParams))
            Visual->Initialize(AttackOrigin, EffectiveAttackRadius, FLinearColor(3, 0.15f, 0.5f), 0.4f, false,
                               &GrandEntrance->Presentation);
    }
    const float EffectiveAttackDamage = GetEffectiveAttackDamage();
    const UPlayerUpgradeComponent *PlayerUpgrades = ResolveSamuraiUpgrades(this, OwnerCharacter);
    const bool bCanApplyMarkedBlade =
        PlayerUpgrades && PlayerUpgrades->HasUpgradeId(SamuraiAutoAttackIds::MarkedBlade);

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AutoAttackTrace), false, OwnerCharacter);
    QueryParams.AddIgnoredActor(OwnerCharacter);

    TArray<FHitResult> HitResults;
    const FCollisionShape TraceShape = FCollisionShape::MakeSphere(EffectiveAttackRadius);
    FCollisionObjectQueryParams ObjectQueryParams;
    ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
    ObjectQueryParams.AddObjectTypesToQuery(ECC_GameTraceChannel1);
    GetWorld()->SweepMultiByObjectType(HitResults, HitboxCenter, HitboxCenter, FQuat::Identity, ObjectQueryParams,
                                       TraceShape, QueryParams);

    TSet<AEnemyBase *> UniqueEnemies;
    TArray<AEnemyBase *> HitEnemies;
    const AActor *OwnerActor = OwnerCharacter->GetOwner();
    const EPlayerAttackSource AttackSource = AEnemyBase::ResolvePlayerAttackSource(OwnerCharacter);
    const bool bCanApplyBleed = AttackSource == EPlayerAttackSource::Samurai && PlayerUpgrades &&
                                PlayerUpgrades->HasUpgradeId(SamuraiAutoAttackIds::BleedingEdge);
    for (const FHitResult &HitResult : HitResults)
    {
        AActor *HitActor = HitResult.GetActor();
        if (!HitActor || HitActor == OwnerCharacter || HitActor->GetOwner() == OwnerActor)
        {
            continue;
        }

        AEnemyBase *HitEnemy = Cast<AEnemyBase>(HitActor);
        if (!HitEnemy || HitEnemy->IsDead() || UniqueEnemies.Contains(HitEnemy))
        {
            continue;
        }

        UHealthComponent *EnemyHealth = HitEnemy->GetHealthComponent();
        if (!EnemyHealth || EnemyHealth->IsDead())
        {
            continue;
        }
        if (!HitEnemy->CanReceivePlayerDamage(AttackSource))
        {
            continue;
        }

        UniqueEnemies.Add(HitEnemy);
        HitEnemies.Add(HitEnemy);
    }

    AEnemyBase *PrimaryTarget = nullptr;
    float BestAlignment = -FLT_MAX;
    float BestDistanceSquared = FLT_MAX;
    constexpr float AlignmentTieTolerance = 0.0001f;
    constexpr float DistanceTieToleranceSquared = 1.0f;
    for (AEnemyBase *Candidate : HitEnemies)
    {
        FVector ToCandidate = Candidate->GetActorLocation() - AttackOrigin;
        ToCandidate.Z = 0.0f;
        const float DistanceSquared = ToCandidate.SizeSquared();
        const float Alignment = ToCandidate.Normalize() ? FVector::DotProduct(AttackForward, ToCandidate) : 1.0f;
        const bool bBetterAlignment = Alignment > BestAlignment + AlignmentTieTolerance;
        const bool bAlignmentTied = FMath::Abs(Alignment - BestAlignment) <= AlignmentTieTolerance;
        const bool bNearer = DistanceSquared < BestDistanceSquared - DistanceTieToleranceSquared;
        const bool bDistanceTied = FMath::Abs(DistanceSquared - BestDistanceSquared) <= DistanceTieToleranceSquared;
        const bool bStableNameWins =
            bDistanceTied && PrimaryTarget &&
            Candidate->GetPathName().Compare(PrimaryTarget->GetPathName(), ESearchCase::CaseSensitive) < 0;
        if (!PrimaryTarget || bBetterAlignment || (bAlignmentTied && (bNearer || bStableNameWins)))
        {
            PrimaryTarget = Candidate;
            BestAlignment = Alignment;
            BestDistanceSquared = DistanceSquared;
        }
    }

    const float ResolvedPrimaryDamage = EffectiveAttackDamage;
    LastResolvedPrimaryAttackDamage = ResolvedPrimaryDamage;
    const float SecondaryDamage =
        GrandEntrance ? EffectiveAttackDamage
                      : EffectiveAttackDamage * FMath::Clamp(SecondaryTargetDamageMultiplier, 0.0f, 1.0f);
    bool bPrimaryKilled = false;
    bool bHitSomething = false;
    const auto ApplyMeleeImpact = [&](AEnemyBase *Enemy, float Damage) {
        FVector Location, Normal;
        Enemy->GetImpactContact(AttackOrigin, Location, Normal);
        const float HealthBeforeHit =
            Enemy->GetHealthComponent() ? Enemy->GetHealthComponent()->GetCurrentHealth() : 0.f;
        const bool bApplied = Enemy->ApplyPlayerDamage(Damage, AttackSource);
        if (bApplied)
        {
            if (!bActiveAttackIsAssist && PlayerUpgrades)
                const_cast<UPlayerUpgradeComponent *>(PlayerUpgrades)
                    ->HandleSamuraiDirectHit(Enemy, Damage, HealthBeforeHit);
            if (ShouldApplySamuraiPushback())
                Enemy->ApplyAttackPushback(AttackOrigin, AttackSource, SamuraiPushbackDistance,
                                           SamuraiPushbackDuration);
            bHitSomething = true;
            if (OwnerCharacter->GetOwner())
                if (auto *Abilities = OwnerCharacter->GetOwner()->FindComponentByClass<USurvivorAbilityComponent>())
                    Abilities->NotifyPartnerHit(AttackSource, Enemy);
            UImpactFeedbackLibrary::PlayImpactFeedback(this, ImpactFeedback, Location, Normal, false);
        }
        return bApplied;
    };

    if (PrimaryTarget)
    {
        if (UHealthComponent *PrimaryHealth = PrimaryTarget->GetHealthComponent();
            PrimaryHealth && !PrimaryHealth->IsDead())
        {
            const bool bDamageApplied = ApplyMeleeImpact(PrimaryTarget, ResolvedPrimaryDamage);
            bPrimaryKilled = PrimaryHealth->IsDead();
            if (bDamageApplied && !bPrimaryKilled && bCanApplyBleed)
                PrimaryTarget->GetStatusEffectComponent()->ApplyStatus(
                    EEnemyStatusEffect::Bleed, const_cast<UPlayerUpgradeComponent *>(PlayerUpgrades), AttackSource,
                    false, ResolvedPrimaryDamage);
            if (bCanApplyMarkedBlade)
                PrimaryTarget->ApplyMark();
        }
    }

    for (AEnemyBase *HitEnemy : HitEnemies)
    {
        if (!HitEnemy || HitEnemy == PrimaryTarget)
            continue;
        UHealthComponent *EnemyHealth = HitEnemy->GetHealthComponent();
        if (!EnemyHealth || EnemyHealth->IsDead())
            continue;
        const bool bDamageApplied = ApplyMeleeImpact(HitEnemy, SecondaryDamage);
        if (bDamageApplied && !EnemyHealth->IsDead() && bCanApplyBleed)
            HitEnemy->GetStatusEffectComponent()->ApplyStatus(EEnemyStatusEffect::Bleed,
                                                              const_cast<UPlayerUpgradeComponent *>(PlayerUpgrades),
                                                              AttackSource, false, SecondaryDamage);
        if (bCanApplyMarkedBlade)
            HitEnemy->ApplyMark();
    }

    if (bHitSomething && ImpactSound && !ImpactFeedback.HitSound)
    {
        UGameplayStatics::PlaySound2D(GetWorld(), ImpactSound);
    }
    if (bHitSomething && AttackSource == EPlayerAttackSource::Samurai && ImpactFeedback.bEnableCameraShake)
        UImpactFeedbackLibrary::PlayGameplayCameraShake(this, ImpactFeedback.CameraShakeClass,
                                                        ImpactFeedback.CameraShakeScale);

    if (bDebugAttackTrace)
    {
        const FColor DebugColor = HitEnemies.Num() > 0 ? FColor::Red : FColor::Cyan;
        constexpr float DebugDuration = 1.5f;
        DrawDebugLine(GetWorld(), AttackOrigin, HitboxCenter, DebugColor, false, DebugDuration, 0, 4.0f);
        DrawDebugSphere(GetWorld(), HitboxCenter, EffectiveAttackRadius, 24, DebugColor, false, DebugDuration, 0, 4.0f);
    }

    return true;
}

void UAutoAttackComponent::SpawnBladeWavesForAttack(float ResolvedPrimaryDamage)
{
    ASamuraiCharacter *Samurai = Cast<ASamuraiCharacter>(OwnerCharacter);
    UPlayerUpgradeComponent *Upgrades = ResolveSamuraiUpgrades(this, Samurai);
    if (!Samurai || !Upgrades || !Upgrades->HasUpgradeId(TEXT("BladeWave")) || !BladeWaveClass || !GetWorld())
        return;

    auto *FamilyComponent =
        Samurai->GetOwner() ? Samurai->GetOwner()->FindComponentByClass<USurvivorAbilityComponent>() : nullptr;
    const auto Tune = [FamilyComponent](FName Key, float Default, int32 Slot = -1) {
        return FamilyComponent ? FamilyComponent->Tuning(4, Key, Default, Slot) : Default;
    };
    FVector Forward = Samurai->GetVisualForwardVector();
    if (bActiveAttackIsAssist)
    {
        if (AEnemyBase *AssistTarget = CurrentAttackTarget.Get(); AssistTarget && !AssistTarget->IsDead())
        {
            Forward = AssistTarget->GetActorLocation() - Samurai->GetActorLocation();
        }
    }
    Forward.Z = 0.0f;
    if (!Forward.Normalize())
        return;
    const float WideArc = FMath::Max(0.0f, Upgrades->GetAccumulatedUpgradeMagnitude(TEXT("WideArc")));
    const float AreaMultiplier = AttackRadius > KINDA_SMALL_NUMBER ? GetEffectiveAttackRadius() / AttackRadius : 1.0f;
    const float WaveWidth =
        Tune(TEXT("WaveWidth"), BladeWaveBaseWidth) * FMath::Max(0.0f, AreaMultiplier) * (1.0f + WideArc);
    const float WaveDamage = FMath::Max(0.0f, ResolvedPrimaryDamage) *
                             Tune(TEXT("WaveDamageMultiplier"), BladeWaveDamageMultiplier) * (1.0f + WideArc) *
                             (1.0f + Upgrades->GetAccumulatedUpgradeMagnitude(TEXT("BladeWavePower")));
    const bool bReturns = Upgrades->HasUpgradeId(TEXT("ReturningBlade"));
    const bool bCrossing = Upgrades->HasUpgradeId(TEXT("CrossingBlades"));
    ++CrossingBladesAttackCounter;
    const bool bTriple =
        bCrossing &&
        CrossingBladesAttackCounter % FMath::Max(1, FMath::RoundToInt(Tune(TEXT("AttackFrequency"), 3, 1))) == 0;
    const float SideAngle = Tune(TEXT("SideAngle"), CrossingBladeSideAngle, 1);
    const int32 BonusWaves = FMath::Clamp(Upgrades->GetUpgradeLevelById(TEXT("WaveMultishot")), 0, 3);
    const int32 WaveCount =
        FMath::Clamp((bTriple ? FMath::RoundToInt(Tune(TEXT("WaveCount"), 3, 1)) : 1) + BonusWaves, 1, 16);
    const auto *Multishot = Upgrades->FindUpgradeDefinition(TEXT("WaveMultishot"));
    const float ExtraAngle = FMath::Max(0.f, Multishot ? Multishot->GetBalanceValue(TEXT("AnglePerWave"), 8.f) : 8.f);
    const float FanHalfAngle = bTriple ? SideAngle : ExtraAngle * BonusWaves * 0.5f;
    for (int32 Index = 0; Index < WaveCount; ++Index)
    {
        const float Angle = WaveCount > 1
                                ? FMath::Lerp(-FanHalfAngle, FanHalfAngle, static_cast<float>(Index) / (WaveCount - 1))
                                : 0.0f;
        const FVector Direction = Forward.RotateAngleAxis(Angle, FVector::UpVector);
        const FVector SpawnLocation = Samurai->GetActorLocation() + Direction * Tune(TEXT("SpawnForwardOffset"), 80) +
                                      FVector(0.0f, 0.0f, Tune(TEXT("SpawnHeightOffset"), 60));
        FActorSpawnParameters Params;
        Params.Owner = Samurai;
        Params.Instigator = Samurai;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        if (ASamuraiBladeWave *Wave =
                GetWorld()->SpawnActor<ASamuraiBladeWave>(BladeWaveClass, SpawnLocation, Direction.Rotation(), Params))
        {
            Wave->InitializeBladeWave(Samurai, Upgrades, Direction, WaveDamage, WaveWidth,
                                      Tune(TEXT("WaveTravelDistance"), BladeWaveTravelDistance),
                                      Tune(TEXT("WaveSpeed"), BladeWaveSpeed) *
                                          (1.0f + Upgrades->GetAccumulatedUpgradeMagnitude(TEXT("BladeWaveHaste"))),
                                      bReturns, FMath::Max(0.0f, AreaMultiplier) * (1.0f + WideArc));
        }
    }
}

void UAutoAttackComponent::RegisterDoubleCutPrimaryAttack()
{
    if (!GetWorld() || DoubleCutPrimaryAttackCount <= 0)
    {
        return;
    }

    // Progress and ownership are independent. A stored proc remains owned while
    // later legitimate primaries begin progress toward the following proc.
    ++DoubleCutPrimaryAttackCounter;
    if (DoubleCutPrimaryAttackCounter < DoubleCutPrimaryAttackCount)
    {
        return;
    }

    if (bDoubleCutReady)
    {
        DoubleCutPrimaryAttackCounter = DoubleCutPrimaryAttackCount - 1;
        return;
    }
    DoubleCutPrimaryAttackCounter = 0;
    bDoubleCutReady = true;
}

bool UAutoAttackComponent::HasDoubleCutUpgrade() const
{
    if (!OwnerCharacter || !OwnerCharacter->IsA<ASamuraiCharacter>())
    {
        return false;
    }

    const UPlayerUpgradeComponent *PlayerUpgrades = ResolveSamuraiUpgrades(this, OwnerCharacter);
    return PlayerUpgrades && PlayerUpgrades->GetSpecialEffectLevel(EUpgradeSpecialEffect::DoubleCut) > 0;
}

bool UAutoAttackComponent::WillNextSamuraiAttackTriggerDoubleCut() const
{
    return HasDoubleCutUpgrade() &&
           (bDoubleCutReady ||
            (DoubleCutPrimaryAttackCount > 0 && DoubleCutPrimaryAttackCounter + 1 >= DoubleCutPrimaryAttackCount));
}

bool UAutoAttackComponent::AcquireDoubleCutFollowUpTarget()
{
    if (!OwnerCharacter || !OwnerCharacter->IsA<ASamuraiCharacter>())
    {
        CurrentAttackTarget.Reset();
        return false;
    }

    if (IsCursorTargetingEnabledForNormalAttack())
    {
        FVector CursorDirection;
        if (!ResolveCursorAttackDirection(CursorDirection))
        {
            return false;
        }

        CurrentAttackTarget.Reset();
        ActiveAttackDirection = CursorDirection;
        const FVector FacingTarget =
            OwnerCharacter->GetActorLocation() + CursorDirection * FMath::Max(100.0f, GetEffectiveTargetingRange());
        OwnerCharacter->SetFacingOverrideTarget(FacingTarget);
        OwnerCharacter->SetVisualFacingRotation(FRotator(0.0f, CursorDirection.Rotation().Yaw, 0.0f));
        return true;
    }

    AEnemyBase *TargetEnemy = FindNearestEnemyTarget();
    if (!TargetEnemy || TargetEnemy->IsDead())
    {
        CurrentAttackTarget.Reset();
        if (OwnerCharacter)
        {
            OwnerCharacter->ClearFacingOverride();
        }
        return false;
    }

    CurrentAttackTarget = TargetEnemy;
    const FVector AimLocation = GetEnemyAimLocation(TargetEnemy);
    FVector ToTarget = AimLocation - OwnerCharacter->GetActorLocation();
    ToTarget.Z = 0.0f;
    if (!ToTarget.Normalize())
    {
        CurrentAttackTarget.Reset();
        return false;
    }

    OwnerCharacter->SetFacingOverrideTarget(AimLocation);
    OwnerCharacter->SetVisualFacingRotation(FRotator(0.0f, ToTarget.Rotation().Yaw, 0.0f));
    return true;
}

bool UAutoAttackComponent::StartDoubleCutFollowUp()
{
    if (OwnerCharacter && OwnerCharacter->SwapPresentation && OwnerCharacter->SwapPresentation->IsBlockingAttacks()) return false;
    if (!bDoubleCutReady || !CanExecuteAttackInCurrentMode() || !OwnerCharacter || OwnerCharacter->IsDashing() ||
        !OwnerCharacter->IsA<ASamuraiCharacter>() || ProjectileClass)
    {
        return false;
    }

    if (!AcquireDoubleCutFollowUpTarget())
    {
        return false;
    }

    UAnimMontage *FollowUpMontage = DoubleCutMontage ? DoubleCutMontage.Get() : AttackMontage.Get();
    if (!FollowUpMontage)
        return false;

    ACharacter *OwnerAsCharacter = Cast<ACharacter>(GetOwner());
    USkeletalMeshComponent *MeshComponent = OwnerAsCharacter ? OwnerAsCharacter->GetMesh() : nullptr;
    UAnimInstance *AnimInstance = MeshComponent ? MeshComponent->GetAnimInstance() : nullptr;
    if (!AnimInstance)
    {
        UE_LOG(LogTemp, Warning, TEXT("Double Cut skipped: AnimInstance invalid on %s."), *GetNameSafe(OwnerCharacter));
        return false;
    }

    const float PlayResult =
        AnimInstance->Montage_Play(FollowUpMontage, CalculateAttackMontagePlayRate(FollowUpMontage));
    if (PlayResult <= 0.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("Double Cut montage failed to play on %s."), *GetNameSafe(OwnerCharacter));
        return false;
    }

    FOnMontageEnded MontageEndedDelegate;
    MontageEndedDelegate.BindUObject(this, &UAutoAttackComponent::HandleDoubleCutMontageEnded);
    AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, FollowUpMontage);

    bIsAttacking = true;
    bAttackNotifyConsumed = false;
    bActiveAttackIsAssist = false;
    bDoubleCutFollowUpActive = true;
    ActiveAttackMontage = FollowUpMontage;
    ApplyAttackWeaponVisualScale();
    return true;
}

bool UAutoAttackComponent::ConsumePendingDoubleCutFollowUp()
{
    if (!bDoubleCutFollowUpPending)
    {
        return false;
    }

    bDoubleCutFollowUpPending = false;
    bAttackNotifyConsumed = false;
    bActiveAttackIsAssist = false;
    ActiveAttackSequence = 0;
    return StartDoubleCutFollowUp();
}

void UAutoAttackComponent::HandleDoubleCutMontageEnded(UAnimMontage *Montage, bool bInterrupted)
{
    if (!bDoubleCutFollowUpActive || Montage != ActiveAttackMontage)
    {
        return;
    }

    bIsAttacking = false;
    bAttackNotifyConsumed = false;
    bActiveAttackIsAssist = false;
    bDoubleCutFollowUpActive = false;
    ActiveAttackMontage = nullptr;
    CurrentAttackTarget.Reset();
    if (OwnerCharacter)
    {
        OwnerCharacter->ClearFacingOverride();
    }
    RestoreAttackWeaponVisualScale();
}

bool UAutoAttackComponent::ShouldApplySamuraiPushback() const
{
    // Checked before the primary strike increments its counter: includes the strike
    // that earns Double Cut as well as a previously stored ready proc.
    return bDoubleCutFollowUpActive || bActiveAttackIsAssist || !WillNextSamuraiAttackTriggerDoubleCut();
}

USceneComponent *UAutoAttackComponent::ResolveWeaponVisualComponent()
{
    if (IsValid(WeaponVisualComponent))
    {
        return WeaponVisualComponent;
    }

    ASamuraiCharacter *Samurai = Cast<ASamuraiCharacter>(OwnerCharacter);
    if (!Samurai || WeaponVisualComponentName.IsNone())
    {
        return nullptr;
    }

    TArray<USceneComponent *> SceneComponents;
    Samurai->GetComponents<USceneComponent>(SceneComponents);
    for (USceneComponent *SceneComponent : SceneComponents)
    {
        if (IsValid(SceneComponent) && SceneComponent->GetFName() == WeaponVisualComponentName)
        {
            WeaponVisualComponent = SceneComponent;
            return WeaponVisualComponent;
        }
    }

    return nullptr;
}

void UAutoAttackComponent::ApplyAttackWeaponVisualScale()
{
    if (!OwnerCharacter || !OwnerCharacter->IsA<ASamuraiCharacter>() || ProjectileClass)
    {
        return;
    }

    USceneComponent *ScaleRoot = ResolveWeaponVisualComponent();
    const UCharacterStatsComponent *CharacterStats = OwnerCharacter->GetCharacterStats();
    if (!ScaleRoot || !CharacterStats)
    {
        return;
    }

    if (!bAttackWeaponVisualScaleApplied)
    {
        OriginalWeaponVisualComponentRelativeScale = ScaleRoot->GetRelativeScale3D();
        bAttackWeaponVisualScaleApplied = true;
    }

    const float AreaMultiplier = CharacterStats->GetFinalAttackAreaMultiplier();
    ScaleRoot->SetRelativeScale3D(OriginalWeaponVisualComponentRelativeScale * AreaMultiplier);
}

void UAutoAttackComponent::RestoreAttackWeaponVisualScale()
{
    if (!bAttackWeaponVisualScaleApplied)
    {
        return;
    }

    if (USceneComponent *ScaleRoot = ResolveWeaponVisualComponent())
    {
        ScaleRoot->SetRelativeScale3D(OriginalWeaponVisualComponentRelativeScale);
    }
    bAttackWeaponVisualScaleApplied = false;
}
