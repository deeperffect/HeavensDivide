#include "SwapPresentationComponent.h"
// Copyright Epic Games, Inc. All Rights Reserved.

#include "AutoAttackComponent.h"
#include "NinjaBuildComponent.h"
#include "InactiveCharacterAssistComponent.h"
#include "AnimNotify_SpawnSamuraiSlashNiagara.h"
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
#include "CrescentBuild.h"
#include "SamuraiIaijutsu.h"
#include "IaijutsuBuild.h"
#include "SamuraiCharacter.h"
#include "SharedPlayerStatsComponent.h"
#include "SurvivorPlayerController.h"
#include "TimerManager.h"

namespace SamuraiAutoAttackIds
{
static const FName MarkedBlade(TEXT("MarkedBlade"));
static const FName BleedingEdge(TEXT("BattleStance"));
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

const UUpgradeDefinition* UAutoAttackComponent::GetIaijutsuUpgrade() const
{
    if (!OwnerCharacter || !OwnerCharacter->IsA<ASamuraiCharacter>()) return nullptr;
    auto* Upgrades = ResolveSamuraiUpgrades(this, OwnerCharacter);
    return Upgrades && Upgrades->HasUpgradeId(TEXT("Iaijutsu"))
        ? Upgrades->FindUpgradeDefinition(TEXT("Iaijutsu")) : nullptr;
}

bool UAutoAttackComponent::ResolveIaijutsuAttackDirection(float Distance, FVector& Direction) const
{
    if (!OwnerCharacter) return false;
    auto* Upgrades = ResolveSamuraiUpgrades(this, OwnerCharacter);
    if (IsCursorTargetingEnabledForNormalAttack())
    {
        // Use the shared cursor/controller aim, including attacks into empty space.
        if (!ResolveCursorAttackDirection(Direction)) return false;
    }
    else
    {
        const float TargetRange = Distance + IaijutsuBuild::VacuumReach(Upgrades);
        TArray<AEnemyBase*> Targets;
        FindEnemyTargetsSortedFromLocation(OwnerCharacter->GetActorLocation(), TargetRange, Targets);
        Targets.RemoveAll([this, TargetRange](AEnemyBase* Enemy)
        {
            return !Enemy->CanReceivePlayerDamage(EPlayerAttackSource::Samurai)
                || FVector::DistSquared2D(Enemy->GetActorLocation(), OwnerCharacter->GetActorLocation()) > FMath::Square(TargetRange);
        });
        if (Targets.IsEmpty()) return false;
        Direction = (Targets[0]->GetActorLocation() - OwnerCharacter->GetActorLocation()).GetSafeNormal2D();
        if (Direction.IsNearlyZero()) Direction = OwnerCharacter->GetVisualForwardVector().GetSafeNormal2D();
    }
    return !Direction.IsNearlyZero();
}

bool UAutoAttackComponent::StartIaijutsuAttack()
{
    const auto* Card = GetIaijutsuUpgrade();
    if (!Card || !CanExecuteAttackInCurrentMode() || !CanStartAttackNow()) return false;
    const float Distance = FMath::Max(1.f, Card->GetBalanceValue(TEXT("DashDistance"), 800.f));
    auto* Upgrades = ResolveSamuraiUpgrades(this, OwnerCharacter);
    FVector Direction;
    if (!ResolveIaijutsuAttackDirection(Distance, Direction)) return false;
    auto* Samurai = Cast<ASamuraiCharacter>(OwnerCharacter);
    const float ChargeDuration = FMath::FRand() < IaijutsuBuild::Chance(Upgrades, TEXT("IaijutsuInstant"), TEXT("IaijutsuInstantChance"), .15f, .1f)
        ? 0.f : GetIaijutsuChargeDuration();
    LastAttackStartTime = GetWorld()->GetTimeSeconds();
    IaijutsuChargeEndTime = LastAttackStartTime + ChargeDuration;
    AttackIntervalAtLastAttackStart = GetEffectiveAttackInterval() - GetIaijutsuChargeDuration() + ChargeDuration;
    NextAttackReadyTime = LastAttackStartTime + AttackIntervalAtLastAttackStart;
    ActiveAttackDirection = Direction;
    if (!SpawnIaijutsuSlashes(Samurai->GetActorLocation(), Direction, Distance, ChargeDuration, true)) return false;
    PlayIaijutsuMontage(ChargeDuration);
    return true;
}

void UAutoAttackComponent::PlayIaijutsuMontage(float ChargeDuration)
{
    auto* Anim = OwnerCharacter && OwnerCharacter->GetMesh() ? OwnerCharacter->GetMesh()->GetAnimInstance() : nullptr;
    if (!IaijutsuMontage || !Anim) return;
    // Animate the real Samurai once per normal attack, independently of the lane
    // actors. Dash/cascade lanes must not restart this montage or interrupt a dash.
    // Instant casts still resolve immediately; their presentation uses authored speed.
    const float PlayRate = ChargeDuration > KINDA_SMALL_NUMBER
        ? IaijutsuMontage->GetPlayLength() / (ChargeDuration * FMath::Max(.01f, IaijutsuMontage->RateScale))
        : 1.f;
    if (Anim->Montage_Play(IaijutsuMontage, PlayRate) <= 0.f) return;
    if (auto* Instance = Anim->GetActiveInstanceForMontage(IaijutsuMontage)) Instance->PushDisableRootMotion();
    ActiveIaijutsuMontage = IaijutsuMontage;
    OwnerCharacter->SetVisualFacingRotation(ActiveAttackDirection.Rotation());
    // Keep bIsAttacking/facing overrides unset: the charge owns timing, and the
    // player can move or dash without animation notifies producing melee damage.
}

float UAutoAttackComponent::GetIaijutsuChargeDuration() const
{
    return IaijutsuBuild::Charge(ResolveSamuraiUpgrades(this, OwnerCharacter));
}

void UAutoAttackComponent::RollIaijutsuAssist()
{
    if (!CanAutoAttack() || !GetIaijutsuUpgrade() || OwnerCharacter->IsDashing()) return;
    auto* U = ResolveSamuraiUpgrades(this, OwnerCharacter);
    if (FMath::FRand() < IaijutsuBuild::Chance(U, TEXT("IaijutsuAssist"), TEXT("IaijutsuAssistChance"), .05f, .05f))
        if (auto* Assist = U->GetOwner()->FindComponentByClass<UInactiveCharacterAssistComponent>()) Assist->TryBloodAssist();
}

void UAutoAttackComponent::SpawnIaijutsuDash(FVector Origin, FVector Destination)
{
    auto* U = ResolveSamuraiUpgrades(this, OwnerCharacter);
    if (!GetIaijutsuUpgrade() || !U || !U->HasUpgradeId(TEXT("IaijutsuDash")) || !CanAutoAttack()) return;
    const FVector Path = Destination - Origin;
    if (Path.SizeSquared2D() < 1.f) return;
    const float Charge = FMath::FRand() < IaijutsuBuild::Chance(U, TEXT("IaijutsuInstant"), TEXT("IaijutsuInstantChance"), .15f, .1f)
        ? 0.f : GetIaijutsuChargeDuration();
    SpawnIaijutsuSlashes(Origin, Path.GetSafeNormal2D(), Path.Size2D(), Charge, false, nullptr,
        1.f + IaijutsuBuild::Scaling(U, TEXT("IaijutsuDashPower"), .2f));
}

bool UAutoAttackComponent::SpawnIaijutsuSlashes(FVector Origin, FVector Direction, float Distance,
    float ChargeDuration, bool bNormalAttack, TSharedPtr<int32> ChainBudget, float DamageMultiplier)
{
    auto* U = ResolveSamuraiUpgrades(this, OwnerCharacter);
    auto* Samurai = Cast<ASamuraiCharacter>(OwnerCharacter);
    if (!Samurai || !U || !GetIaijutsuUpgrade() || !CanAutoAttack()) return false;
    // One kill follow-up per originating attack, shared by both Double Cut lanes.
    // The cascaded attack inherits the exhausted budget and cannot cascade again.
    if (!ChainBudget) ChainBudget = MakeShared<int32>(1);
    const auto ChainTriggered = MakeShared<bool>(false);
    float Damage = GetEffectiveAttackDamage() * (1.f + IaijutsuBuild::Scaling(U, TEXT("IaijutsuDamage"), .2f))
        * FMath::Max(0.f, DamageMultiplier);
    float Area = AttackRadius > KINDA_SMALL_NUMBER ? GetEffectiveAttackRadius() / AttackRadius : 1.f;
    Area *= 1.f + IaijutsuBuild::Scaling(U, TEXT("IaijutsuWidth"), .25f);
    if (U->HasUpgradeId(TEXT("IaijutsuPowerPact"))) Damage *= 1.5f;
    if (U->HasUpgradeId(TEXT("IaijutsuMarkPact"))) Area *= .8f;
    const float Radius = IaijutsuBuild::Value(U, TEXT("Iaijutsu"), TEXT("SlashRadius"), 100.f) * Area;
    bool bCross = false;
    if (U->HasUpgradeId(TEXT("IaijutsuDoubleCut")))
    {
        const int32 Threshold = FMath::Max(1, 4 - U->GetUpgradeLevelById(TEXT("IaijutsuDoubleCutFrequency")));
        bCross = ++IaijutsuAttackCounter >= Threshold;
        if (bCross) IaijutsuAttackCounter = 0;
    }
    const bool bEndpoint = FMath::FRand() < IaijutsuBuild::Chance(U, TEXT("IaijutsuAOE"), TEXT("IaijutsuAOEChance"), .15f, .1f);
    struct FChargeAim
    {
        FVector Origin, Direction;
        uint64 Frame = MAX_uint64;
    };
    const auto ChargeAim = MakeShared<FChargeAim>();
    ChargeAim->Origin = Origin;
    ChargeAim->Direction = Direction;
    TArray<ASamuraiIaijutsu*> Slashes;
    for (int32 Index = 0; Index < (bCross ? 2 : 1); ++Index)
    {
        // Rotate around the lane midpoint so both lanes intersect in a true X.
        const FVector SlashDirection = bCross ? Direction.RotateAngleAxis(Index ? 30.f : -30.f, FVector::UpVector) : Direction;
        const FVector SlashOrigin = bCross ? Origin + (Direction - SlashDirection) * Distance * .5f : Origin;
        FActorSpawnParameters Params;
        Params.Owner = Samurai; Params.Instigator = Samurai;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Slash = GetWorld()->SpawnActor<ASamuraiIaijutsu>(SlashOrigin, SlashDirection.Rotation(), Params);
        if (!Slash) continue;
        Slash->ChainBudget = ChainBudget;
        Slash->ChainTriggered = ChainTriggered;
        Slash->bEndpointBurst = bEndpoint && Index == 0;
        Slash->EndpointMontage = IaijutsuEndpointMontage;
        Slash->Initialize(Samurai, U, SlashDirection, Damage, Distance, Radius, ChargeDuration,
            AttackMontage.Get(),
            ImpactFeedback, IaijutsuHitVFX, IaijutsuIndicatorMaterial, IaijutsuIndicatorColor);
        Slash->ConfigurePathSlashes(IaijutsuPathSlashVFX, IaijutsuPathSlashCount,
            IaijutsuPathSlashScale, IaijutsuPathSlashRotation,
            IaijutsuPathSlashDelay, IaijutsuPathSlashRotationRandomness);
        if (bNormalAttack)
        {
            // Sample after character movement. Both X lanes share one aim query per
            // frame, before either lane's vacuum or damage can change the targets.
            Slash->SetTickGroup(TG_PostPhysics);
            const float Angle = bCross ? (Index ? 30.f : -30.f) : 0.f;
            Slash->UpdateAim = FIaijutsuAimUpdate::CreateWeakLambda(this,
                [this, ChargeAim, Distance, Angle](FVector& LaneOrigin, FVector& LaneEnd)
            {
                if (!CanAutoAttack() || !GetIaijutsuUpgrade()) return;
                if (ChargeAim->Frame != GFrameCounter)
                {
                    ChargeAim->Frame = GFrameCounter;
                    ChargeAim->Origin = OwnerCharacter->GetActorLocation();
                    // With no valid target/aim, keep the last direction and finish
                    // the existing charge, still following the character.
                    FVector Aim;
                    if (ResolveIaijutsuAttackDirection(Distance, Aim)) ChargeAim->Direction = Aim;
                    if (!OwnerCharacter->IsDashing()) OwnerCharacter->SetVisualFacingRotation(ChargeAim->Direction.Rotation());
                }
                const FVector LaneDirection = ChargeAim->Direction.RotateAngleAxis(Angle, FVector::UpVector);
                LaneOrigin = ChargeAim->Origin + (ChargeAim->Direction - LaneDirection) * Distance * .5f;
                LaneEnd = LaneOrigin + LaneDirection * Distance;
            });
        }
        if (Index == 0 && bNormalAttack) Slash->OnResolved = FSimpleDelegate::CreateWeakLambda(this, [this, ChargeAim]
        {
            if (!CanExecuteAttackInCurrentMode() || OwnerCharacter->GetCharacterMode() != ECharacterMode::Active) return;
            ActiveAttackDirection = ChargeAim->Direction;
            if (GetReadyGrandEntranceUpgrade()) ExecuteMeleeAttackTrace();
            OnAutoAttack.Broadcast(this, EAutoAttackSource::NormalAutoAttack);
        });
        Slashes.Add(Slash);
    }
    // Resolve instant casts now, after all lanes have been configured.
    if (ChargeDuration <= 0.f) for (auto* Slash : Slashes) Slash->Tick(0.f);
    return !Slashes.IsEmpty();
}

bool UAutoAttackComponent::ExecuteMeleeAttackTrace()
{
    if (!OwnerCharacter || !GetWorld())
    {
        UE_LOG(LogTemp, Warning, TEXT("AutoAttack trace skipped: owner/world invalid."));
        return false;
    }

    const auto* StanceUpgrades = ResolveSamuraiUpgrades(this, OwnerCharacter);
    if (!bActiveAttackIsAssist && StanceUpgrades
        && (StanceUpgrades->HasUpgradeId(TEXT("BladeWave")) || StanceUpgrades->HasUpgradeId(TEXT("Iaijutsu")))
        && !GetReadyGrandEntranceUpgrade()) return false;

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
    // Double Cut keeps the normal attack radius but centers its follow-up on
    // the Samurai, covering the full circle rather than the forward hitbox.
    const FVector HitboxCenter = (GrandEntrance || bBloodCircularAttack)
        ? AttackOrigin : AttackOrigin + AttackForward * AttackForwardOffset;
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
    const UPlayerUpgradeComponent *PlayerUpgrades = ResolveSamuraiUpgrades(this, OwnerCharacter);
    const float BaseDamage = GetEffectiveAttackDamage();
    const bool bBlood = !bActiveAttackIsAssist && PlayerUpgrades && PlayerUpgrades->HasUpgradeId(TEXT("BattleStance"));
    const float CritChance = bBlood && PlayerUpgrades->HasUpgradeId(TEXT("BloodCritical"))
        ? .15f + .1f * PlayerUpgrades->GetUpgradeLevelById(TEXT("BloodCriticalChance")) : 0.f;
    const float EffectiveAttackDamage = BaseDamage * (FMath::FRand() < CritChance ? 2.f : 1.f);
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
        (GrandEntrance || bBloodCircularAttack) ? EffectiveAttackDamage
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

    if (bBlood)
    {
        RollBloodAssist();
        const float EchoChance = PlayerUpgrades->HasUpgradeId(TEXT("BloodEcho"))
            ? .15f + .1f * PlayerUpgrades->GetUpgradeLevelById(TEXT("BloodEchoChance")) : 0.f;
        if (FMath::FRand() < EchoChance)
        {
            const bool Circular = bBloodCircularAttack || GrandEntrance;
            FTimerHandle Timer;
            GetWorld()->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this,
                [this, AttackOrigin, AttackForward, BaseDamage, EffectiveAttackRadius, Circular]
                { ExecuteBloodEcho(AttackOrigin, AttackForward, BaseDamage * .5f, EffectiveAttackRadius, Circular); }), FMath::Max(.01f,BloodEchoDelay), false);
        }
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
    if (!bActiveAttackIsAssist && !ActiveAttackDirection.IsNearlyZero()) Forward = ActiveAttackDirection;
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
    const float FieldDamage = FMath::Max(0.0f, ResolvedPrimaryDamage) *
                             Tune(TEXT("WaveDamageMultiplier"), BladeWaveDamageMultiplier) * (1.0f + WideArc) *
                             (1.0f + Upgrades->GetAccumulatedUpgradeMagnitude(TEXT("BladeWavePower"))) *
                             (1.0f + CrescentBuild::Scaling(Upgrades, TEXT("CrescentDamage"), .2f));
    float WaveDamage = FieldDamage;
    if (Upgrades->HasUpgradeId(TEXT("CrescentFieldPact"))) WaveDamage *= .7f;
    if (Upgrades->HasUpgradeId(TEXT("CrescentPowerPact"))) WaveDamage *= 1.5f;
    const bool bReturns = Upgrades->HasUpgradeId(TEXT("ReturningBlade"));
    const bool bCrossing = Upgrades->HasUpgradeId(TEXT("CrossingBlades"));
    if (!bActiveAttackIsAssist && (bCrossing || Upgrades->HasUpgradeId(TEXT("CrescentDoubleCut")))) ++CrossingBladesAttackCounter;
    bool bPlus = false;
    if (!bActiveAttackIsAssist && Upgrades->HasUpgradeId(TEXT("CrescentDoubleCut")))
    {
        const int32 Frequency = FMath::Max(1, 4 - Upgrades->GetUpgradeLevelById(TEXT("CrescentDoubleCutFrequency")));
        bPlus = CrossingBladesAttackCounter >= Frequency;
        if (bPlus) CrossingBladesAttackCounter = 0;
    }
    const bool bArc = FMath::FRand() < CrescentBuild::Chance(Upgrades, TEXT("CrescentArc"), TEXT("CrescentArcChance"), .15f, .1f);
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
    const float WaveDelay = bTriple && FMath::IsFinite(CrossingBladeWaveDelay) ? FMath::Max(0.f, CrossingBladeWaveDelay) : 0.f;
    const float TravelDistance = Tune(TEXT("WaveTravelDistance"), BladeWaveTravelDistance)
        * (1.f + CrescentBuild::Scaling(Upgrades, TEXT("CrescentRange"), .2f));
    const float Speed = Tune(TEXT("WaveSpeed"), BladeWaveSpeed) * (1.f + Upgrades->GetAccumulatedUpgradeMagnitude(TEXT("BladeWaveHaste")))
        * (1.f + CrescentBuild::Scaling(Upgrades, TEXT("CrescentSpeed"), .2f))
        * (Upgrades->HasUpgradeId(TEXT("CrescentPowerPact")) ? .5f : 1.f);
    const float VisualArea = FMath::Max(0.f, AreaMultiplier) * (1.f + WideArc);
    PendingBladeWaveTimers.RemoveAll([this](const FTimerHandle& Timer) { return !GetWorld()->GetTimerManager().TimerExists(Timer); });
    TArray<float> Angles;
    if (bPlus || bArc)
    {
        const float ArcAngle = CrescentBuild::Value(Upgrades, TEXT("CrescentArc"), TEXT("SideAngle"), 20.f);
        for (int32 DirectionIndex = 0; DirectionIndex < (bPlus ? 4 : 1); ++DirectionIndex)
            for (int32 FanIndex = 0; FanIndex < (bArc ? 3 : 1); ++FanIndex)
                Angles.Add(DirectionIndex * 90.f + (bArc ? (FanIndex - 1) * ArcAngle : 0.f));
    }
    else for (int32 Index = 0; Index < WaveCount; ++Index)
        Angles.Add(WaveCount > 1 ? FMath::Lerp(-FanHalfAngle, FanHalfAngle, static_cast<float>(Index)/(WaveCount-1)) : 0.f);
    for (int32 Index = 0; Index < Angles.Num(); ++Index)
    {
        const float Angle = Angles[Index];
        const FVector Direction = Forward.RotateAngleAxis(Angle, FVector::UpVector);
        const FVector SpawnLocation = Samurai->GetActorLocation() + Direction * Tune(TEXT("SpawnForwardOffset"), 80) +
                                      FVector(0.0f, 0.0f, Tune(TEXT("SpawnHeightOffset"), 60));
        // Capture the committed swing's origin, aim and stats; later movement or
        // upgrade changes must not redirect or rescale the rest of this salvo.
        auto SpawnWave = [this, Source = TWeakObjectPtr<ASamuraiCharacter>(Samurai),
            SourceUpgrades = TWeakObjectPtr<UPlayerUpgradeComponent>(Upgrades), WaveClass = BladeWaveClass,
            SpawnLocation, Direction, WaveDamage, WaveWidth, TravelDistance, Speed, bReturns, VisualArea, FieldDamage]()
        {
            if (!Source.IsValid() || !SourceUpgrades.IsValid() || !GetWorld()) return;
            FActorSpawnParameters Params;
            Params.Owner = Source.Get(); Params.Instigator = Source.Get();
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            if (auto* Wave = GetWorld()->SpawnActor<ASamuraiBladeWave>(WaveClass, SpawnLocation, Direction.Rotation(), Params))
                Wave->InitializeBladeWave(Source.Get(), SourceUpgrades.Get(), Direction, WaveDamage, WaveWidth,
                    TravelDistance, Speed, bReturns, VisualArea, FieldDamage);
        };
        if (Index == 0 || WaveDelay <= 0.f) SpawnWave();
        else
        {
            FTimerHandle Timer;
            GetWorld()->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this, SpawnWave), Index * WaveDelay, false);
            PendingBladeWaveTimers.Add(Timer);
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
    return PlayerUpgrades && PlayerUpgrades->HasUpgradeId(TEXT("BattleStance")) && PlayerUpgrades->HasUpgradeId(TEXT("DoubleCut"));
}

bool UAutoAttackComponent::WillNextSamuraiAttackTriggerDoubleCut() const
{
    return HasDoubleCutUpgrade() &&
           (bDoubleCutReady ||
            (DoubleCutPrimaryAttackCounter + 1 >= GetDoubleCutThreshold()));
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
    return true; // Double Cut now replaces a swing, so there is no follow-up to keep targets in range for.
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

int32 UAutoAttackComponent::GetDoubleCutThreshold() const
{
 const auto* U = ResolveSamuraiUpgrades(this,OwnerCharacter);
 return FMath::Max(1,4-(U ? U->GetUpgradeLevelById(TEXT("DoubleCutFrequency")) : 0));
}

void UAutoAttackComponent::RollBloodAssist()
{
 auto* U = ResolveSamuraiUpgrades(this,OwnerCharacter);
 if (!U || !U->HasUpgradeId(TEXT("BattleStance")) || !U->HasUpgradeId(TEXT("BloodAssist"))) return;
 if (FMath::FRand() < .05f + .05f * U->GetUpgradeLevelById(TEXT("BloodAssistChance")))
  if(auto* Assist = U->GetOwner()->FindComponentByClass<UInactiveCharacterAssistComponent>()) Assist->TryBloodAssist();
}

void UAutoAttackComponent::ExecuteBloodEcho(FVector Origin,FVector Direction,float Damage,float Radius,bool bCircular)
{
 auto* U = ResolveSamuraiUpgrades(this,OwnerCharacter);
 if (!U || !U->HasUpgradeId(TEXT("BattleStance")) || !CanExecuteAttackInCurrentMode() || OwnerCharacter->GetCharacterMode()!=ECharacterMode::Active) return;
 // Echoes can crit independently, but cannot recursively generate more echoes.
 if(U->HasUpgradeId(TEXT("BloodCritical")) && FMath::FRand() < .15f+.1f*U->GetUpgradeLevelById(TEXT("BloodCriticalChance"))) Damage*=2.f;
 if(HasDoubleCutUpgrade())
 {
  bCircular |= WillNextSamuraiAttackTriggerDoubleCut();
  DoubleCutPrimaryAttackCounter = WillNextSamuraiAttackTriggerDoubleCut() ? 0 : DoubleCutPrimaryAttackCounter+1;
 }
 const FVector Center = bCircular ? Origin : Origin + Direction*AttackForwardOffset;
 if(BloodEchoVFX)
 {
  FVector VFXOrigin=Origin, VFXScale(Radius/FMath::Max(1.f,AttackRadius));
  FQuat VFXRotation=Direction.ToOrientationQuat();
  // Reuse the normal slash's authored offsets and scale, at the committed lane.
  // Only Niagara is spawned; no montage, root motion, or gameplay notify is replayed.
  if (AttackMontage && OwnerCharacter->GetMesh())
   for (const auto& Event : AttackMontage->Notifies)
    if (const auto* Slash=Cast<UAnimNotify_SpawnSamuraiSlashNiagara>(Event.Notify))
    {
     const auto* Mesh=OwnerCharacter->GetMesh();
     const FQuat FacingDelta=(Direction.Rotation()-OwnerCharacter->GetVisualForwardVector().Rotation()).Quaternion();
     const FVector LocalOffset=bCircular ? FVector(0,0,Slash->LocationOffset.Z) : Slash->LocationOffset;
     VFXOrigin=Origin+FacingDelta.RotateVector(Mesh->GetComponentTransform().TransformPosition(LocalOffset)-OwnerCharacter->GetActorLocation());
     VFXRotation=FacingDelta*Mesh->GetComponentQuat()*Slash->RotationOffset.Quaternion();
     const float Area=OwnerCharacter->GetCharacterStats()->GetFinalAttackAreaMultiplier();
     const float VisualArea=Area>1.f ? 1.f+(Area-1.f)*FMath::Max(0.f,Slash->AreaBonusScaleMultiplier) : Area;
     VFXScale=Slash->Scale*Mesh->GetComponentScale()*VisualArea;
     break;
    }
  for(int32 i=0;i<(bCircular?4:1);++i)
   UNiagaraFunctionLibrary::SpawnSystemAtLocation(this,BloodEchoVFX,VFXOrigin,(FRotator(0,i*90.f,0).Quaternion()*VFXRotation).Rotator(),VFXScale);
 }
 TArray<FOverlapResult> Hits;
 FCollisionObjectQueryParams Objects;Objects.AddObjectTypesToQuery(ECC_Pawn);Objects.AddObjectTypesToQuery(ECC_GameTraceChannel1);
 GetWorld()->OverlapMultiByObjectType(Hits,Center,FQuat::Identity,Objects,FCollisionShape::MakeSphere(Radius));
 AEnemyBase* Primary = nullptr;
 float BestAlignment = -FLT_MAX;
 for (const auto& Hit : Hits)
 {
  auto* Enemy = Cast<AEnemyBase>(Hit.GetActor());
  if (!Enemy || Enemy->IsDead() || !Enemy->CanReceivePlayerDamage(EPlayerAttackSource::Samurai)) continue;
  const float Alignment = FVector::DotProduct(Direction,(Enemy->GetActorLocation()-Origin).GetSafeNormal2D());
  if (!Primary || Alignment > BestAlignment) { Primary=Enemy; BestAlignment=Alignment; }
 }
 TSet<AEnemyBase*> Seen;
 for(const auto& Hit:Hits)
 {
  auto* Enemy=Cast<AEnemyBase>(Hit.GetActor());
  if(!Enemy||Seen.Contains(Enemy)||Enemy->IsDead()||!Enemy->CanReceivePlayerDamage(EPlayerAttackSource::Samurai))continue;
  Seen.Add(Enemy);
  const float HitDamage = Damage * (bCircular || Enemy==Primary ? 1.f : FMath::Clamp(SecondaryTargetDamageMultiplier,0.f,1.f));
  const float Before=Enemy->GetHealthComponent()->GetCurrentHealth();
  if(Enemy->ApplyPlayerDamage(HitDamage,EPlayerAttackSource::Samurai))
  {
   U->HandleSamuraiDirectHit(Enemy,HitDamage,Before);
   if(!Enemy->IsDead()) Enemy->GetStatusEffectComponent()->ApplyStatus(EEnemyStatusEffect::Bleed,U,EPlayerAttackSource::Samurai,false,HitDamage);
   if(U->HasUpgradeId(TEXT("MarkedBlade"))) Enemy->ApplyMark();
  }
 }
 RollBloodAssist();
 OnAutoAttack.Broadcast(this,EAutoAttackSource::NormalAutoAttack);
}
