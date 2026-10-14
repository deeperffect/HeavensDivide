#include "JointComboActor.h"
#include "CharacterBase.h"
#include "SamuraiCharacter.h"
#include "NinjaCharacter.h"
#include "CharacterManagerComponent.h"
#include "CharacterStatsComponent.h"
#include "SurvivorPlayerController.h"
#include "SharedPlayerStatsComponent.h"
#include "PlayerUpgradeComponent.h"
#include "AutoAttackComponent.h"
#include "NinjaBuildComponent.h"
#include "NinjaBuildProjectile.h"
#include "SamuraiIaijutsu.h"
#include "SamuraiBladeWave.h"
#include "EnemyBase.h"
#include "EnemyStatusEffectComponent.h"
#include "HealthComponent.h"
#include "InactiveCharacterAssistComponent.h"
#include "SwapAfterimage.h"
#include "SwapPresentationComponent.h"
#include "PlayerCameraRig.h"
#include "AttackProjectileBase.h"
#include "IaijutsuBuild.h"
#include "FangBuild.h"
#include "BarrageBuild.h"
#include "UpgradeBalance.h"
#include "RunReportSubsystem.h"
#include "CombatGround.h"
#include "IndicatorAttackVFX.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ScopeExit.h"

AJointComboActor::AJointComboActor()
{
    PrimaryActorTick.bCanEverTick=true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root"))); SetActorEnableCollision(false);
}
AJointComboActor* AJointComboActor::Spawn(ACharacterBase* Leader, UComboAbilityDefinition* Definition, float Power)
{
    auto* PC=Leader ? Cast<ASurvivorPlayerController>(Leader->GetOwner()) : nullptr;
    auto* Party=PC ? PC->GetCharacterManager() : nullptr;
    if (!Leader || !Definition || !Definition->IsJoint() || !Party || !Party->GetSamurai() || !Party->GetNinja()) return nullptr;
    FActorSpawnParameters P; P.Owner=Leader; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Actor=Leader->GetWorld()->SpawnActor<AJointComboActor>(Leader->GetActorLocation(),FRotator::ZeroRotator,P);
    if (Actor) Actor->Initialize(Leader,Definition,Power);
    return IsValid(Actor) && !Actor->IsActorBeingDestroyed() ? Actor : nullptr;
}
void AJointComboActor::Initialize(ACharacterBase* Leader, UComboAbilityDefinition* Definition, float Multiplier)
{
    Active=Leader; Data=Definition; Power=FMath::Max(0.f,Multiplier); Controller=CastChecked<ASurvivorPlayerController>(Leader->GetOwner());
    auto* Party=Controller->GetCharacterManager(); Samurai=Party->GetSamurai(); Ninja=Party->GetNinja();
    auto* U=Controller->GetPlayerUpgrades();
    Origin=Leader->GetActorLocation(); Direction=Leader->GetVisualForwardVector().GetSafeNormal2D();
    if (Direction.IsNearlyZero()) Direction=FVector::ForwardVector;
    Windup=FMath::Max(.1f,Data->Windup);
    if (Data->Technique==EComboTechnique::EclipseSeverance && U->HasUpgradeId(TEXT("Iaijutsu"))) {
        Windup*=IaijutsuBuild::Charge(U)/FMath::Max(.01f,IaijutsuBuild::Value(U,TEXT("Iaijutsu"),TEXT("ChargeDuration"),1.f));
        if (FMath::FRand()<IaijutsuBuild::Chance(U,TEXT("IaijutsuInstant"),TEXT("IaijutsuInstantChance"),.15f,.1f)) Windup=.2f;
    }
    Windup=FMath::Clamp(Windup,.2f,3.f);
    HelperArrivalTime=Windup*FMath::Clamp(Data->HelperArrivalFraction,.1f,.7f);
    NextFang=HelperArrivalTime+.04f;
    ControlDuration=Windup+FMath::Clamp(Data->ReleaseRecovery,.2f,.65f);
    if (auto* Assist=Controller->FindComponentByClass<UInactiveCharacterAssistComponent>()) Assist->DeactivateAssistEffect(true);
    Leader->GetCharacterMovement()->StopMovementImmediately(); Leader->ConsumeMovementInputVector();
    if (auto* HP=Controller->GetPlayerHealthComponent()) { bSavedDamageEnabled=HP->IsDamageEnabled(); HP->SetDamageEnabled(false); bOwnsDamageGuard=true; }
    const FVector Right=FVector::CrossProduct(FVector::UpVector,Direction);
    SamuraiPoint=Origin-Right*95; NinjaPoint=Origin+Right*95-Direction*45;
    // The controlled hero's copy starts exactly over its real capsule.
    if (Leader==Samurai.Get()) SamuraiPoint=Origin; else NinjaPoint=Origin;
    const auto MakePerformer=[&](ACharacterBase* Hero,FVector Point,UAnimMontage* Montage) -> ASwapAfterimage* {
        // Translate each copy to the active hero's floor; the hidden source can be far away.
        FVector Surface; if (CombatGround::FindSurface(GetWorld(),Point,Leader,Surface)) Point.Z=Surface.Z+Hero->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        FActorSpawnParameters P; P.Owner=this;
        auto* Visual=GetWorld()->SpawnActor<ASwapAfterimage>(Point,Hero->GetActorRotation(),P);
        if (Visual && Visual->InitializeJointCast(Hero,Montage,Direction,ControlDuration)) return Visual;
        if (Visual) Visual->Destroy(); return nullptr;
    };
    SamuraiVisual=MakePerformer(Samurai.Get(),SamuraiPoint,Data->SamuraiMontage);
    NinjaVisual=MakePerformer(Ninja.Get(),NinjaPoint,Data->NinjaMontage);
    if (auto* Helper=GetHelper()) Helper->SetActorHiddenInGame(true);
    const bool bLeaderCopied=Leader==Samurai.Get() ? SamuraiVisual.IsValid() : NinjaVisual.IsValid();
    bSavedVisualHidden=Leader->GetVisualRoot()->bHiddenInGame;
    if (bLeaderCopied) Leader->GetVisualRoot()->SetHiddenInGame(true,true);
    for (TActorIterator<APlayerCameraRig> It(GetWorld());It;++It) if (It->GetFollowTarget()==Leader) { CameraRig=*It; break; }
    const FVector CameraFocus=Origin+(Data->Technique==EComboTechnique::EclipseSeverance?Direction*Data->Range*.2f:FVector::ZeroVector);
    if (CameraRig.IsValid()) CameraRig->StartJointCamera(this,CameraFocus,Direction,HelperArrivalTime,ControlDuration);
    Burst(Data->ChargeVFX,Origin,Data->ChargeVFXScale,Data->ChargeVFXSeconds>0?FMath::Min(Data->ChargeVFXSeconds,Windup):Windup,Data->ChargeVFXColor,Data->ChargeVFXRotation,Data->ChargeVFXOffset);
    if (Data->Technique==EComboTechnique::EclipseSeverance) StartLane();
    BeginSlowMotion();
    SetLifeSpan(ControlDuration+120.f);
}
bool AJointComboActor::CanContinue() const
{ return Data && Active.IsValid() && Samurai.IsValid() && Ninja.IsValid() && Controller.IsValid() && Controller->IsRunInProgress() && !Controller->IsPlayerDead(); }
float AJointComboActor::HeroDamage(ACharacterBase* Hero) const
{
    const auto* A=Hero ? Hero->FindComponentByClass<UAutoAttackComponent>() : nullptr;
    const auto* S=Hero ? Hero->GetCharacterStats() : nullptr;
    const auto* Shared=Controller->GetSharedPlayerStats();
    return A ? A->GetBaseAttackDamage()*(S?S->GetFinalDamageMultiplier():1)*(Shared?Shared->GetFinalDamageMultiplier():1)*Power : 0;
}
FVector AJointComboActor::Ground(FVector Point) const
{
    FVector Surface; if (CombatGround::FindSurface(GetWorld(),Point,Active.Get(),Surface)) return Surface+FVector(0,0,14);
    Point.Z=Origin.Z-Active->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+14; return Point;
}
void AJointComboActor::Burst(UNiagaraSystem* System,FVector Point,float Scale,float Seconds,FLinearColor Color,FRotator Rotation,FVector Offset,bool bGroundAnchor,bool bTint)
{
    if (!System || GetNetMode()==NM_DedicatedServer) return;
    auto* FX=NewObject<UNiagaraComponent>(this); FX->SetAutoActivate(false); FX->SetAutoDestroy(false); FX->SetAsset(System);
    FX->SetAllowScalability(false); FX->SetTranslucentSortPriority(40); AddInstanceComponent(FX); FX->RegisterComponent();
    FX->SetWorldLocationAndRotation((bGroundAnchor?Ground(Point):Point)+Direction.Rotation().RotateVector(Offset),(Direction.Rotation().Quaternion()*Rotation.Quaternion()).Rotator()); FX->SetWorldScale3D(FVector(FMath::Max(.01f,Scale)));
    FX->SetRandomSeedOffset(static_cast<int32>(FPlatformTime::Cycles()));
    TArray<FNiagaraVariable> Params;System->GetExposedParameters().GetParameters(Params);
    for (const auto& P:Params) if (bTint && Data->bTintVFX && P.GetType()==FNiagaraTypeDefinition::GetColorDef()) { Color.A=System->GetExposedParameters().GetParameterValue<FLinearColor>(P).A; const auto Original=System->GetExposedParameters().GetParameterValue<FLinearColor>(P);
        const float Brightness=FMath::Clamp(FMath::Max3(Original.R,Original.G,Original.B),0.f,1.f);
        if (Brightness>.05f) FX->SetVariableLinearColor(P.GetName(),FLinearColor(Color.R*Brightness,Color.G*Brightness,Color.B*Brightness,Original.A)); }
    RegisterEffect(FX); FX->Activate(true); Effects.Add({FX,Age+FMath::Max(.05f,Seconds)});
}
TArray<AEnemyBase*> AJointComboActor::Enemies(FVector Point,float Radius) const
{
    TArray<AEnemyBase*> Result; TArray<FOverlapResult> Hits;
    FCollisionObjectQueryParams Objects; Objects.AddObjectTypesToQuery(ECC_Pawn); Objects.AddObjectTypesToQuery(ECC_GameTraceChannel1);
    GetWorld()->OverlapMultiByObjectType(Hits,Point,FQuat::Identity,Objects,FCollisionShape::MakeSphere(FMath::Max(1.f,Radius)));
    for (const auto& Hit:Hits) if (auto* E=Cast<AEnemyBase>(Hit.GetActor()); E && !E->IsDead()) Result.AddUnique(E);
    return Result;
}
ASwapAfterimage* AJointComboActor::GetHelper() const
{ return Active.Get()==Samurai.Get()?NinjaVisual.Get():SamuraiVisual.Get(); }
UNiagaraSystem* AJointComboActor::HelperSmoke(bool bDeparture) const
{
    if (auto* Override=bDeparture?Data->HelperDepartureVFX.Get():Data->HelperArrivalVFX.Get()) return Override;
    const auto* Hero=Active.Get()==Samurai.Get()?static_cast<ACharacterBase*>(Ninja.Get()):static_cast<ACharacterBase*>(Samurai.Get());
    return Hero && Hero->SwapPresentation ? Hero->SwapPresentation->CharacterSmokeVFX.Get() : nullptr;
}
void AJointComboActor::RevealHelper()
{
    if (bHelperArrived || bPerformersReleased || Age<HelperArrivalTime) return;
    bHelperArrived=true;
    if (auto* Helper=GetHelper()) {
        Burst(HelperSmoke(false),Helper->GetActorLocation(),Data->HelperSmokeScale,Data->HelperSmokeSeconds,FLinearColor::White,Data->HelperSmokeRotation,Data->HelperSmokeOffset,true,false);
        Helper->SetActorHiddenInGame(false);
    }
}
void AJointComboActor::PlayHitImpact(AEnemyBase* Enemy,TOptional<FVector> HitPosition)
{
    if (!Enemy || !Data || !Data->HitVFX || (!HitPosition.IsSet() && !IsValid(Enemy))) return;
    // Keep bursts alive independently of the victim, including killing blows.
    // Repeated wheel/wave contacts share only a cosmetic per-enemy throttle.
    if (const float* Last=LastHitEffects.Find(Enemy);Last && Age-*Last<FMath::Max(0.f,Data->HitVFXInterval)) return;
    LastHitEffects.Add(Enemy,Age);
    Burst(Data->HitVFX,HitPosition.IsSet()?HitPosition.GetValue():Enemy->GetActorLocation(),Data->HitVFXScale,Data->HitVFXSeconds,Data->Color,Data->HitVFXRotation,Data->HitVFXOffset,false);
}
FVector AJointComboActor::GetBloodSlashCenter() const
{
    return Ground(SamuraiPoint + Direction.Rotation().RotateVector(Data->BloodSlashAnchor)) + Direction.Rotation().RotateVector(Data->VFXOffset);
}
float AJointComboActor::GetBloodSlashRadius() const
{
    return Data->BloodSlashVFXRadius * FMath::Max(.01f,Data->VFXScale) * Samurai->GetCharacterStats()->GetFinalAttackAreaMultiplier() * Data->AreaMultiplier;
}
TArray<AEnemyBase*> AJointComboActor::GetBloodSlashTargets() const
{
    return Enemies(GetBloodSlashCenter(), GetBloodSlashRadius());
}
void AJointComboActor::BloodSlash(bool bEcho)
{
    auto* A=Samurai->FindComponentByClass<UAutoAttackComponent>(); auto* U=Controller->GetPlayerUpgrades();
    const float Area=Samurai->GetCharacterStats()->GetFinalAttackAreaMultiplier();
    const float Radius=GetBloodSlashRadius();
    const float Damage=(U->HasUpgradeId(TEXT("BattleStance")) ? A->GetEffectiveAttackDamage()*Power : HeroDamage(Samurai.Get()))*Data->SamuraiPower*(bEcho?.5f:1.f);
    if (U->HasUpgradeId(TEXT("BattleStance"))) A->ExecuteBloodEcho(SamuraiPoint,Direction,Damage,Radius,false,Data->AbilityId,this);
    else for (auto* E:GetBloodSlashTargets()) {
        const FVector HitPosition=E->GetActorLocation();
        FRunDamageScope Scope(Data->AbilityId);
        if (E->ApplyPlayerDamage(Damage,EPlayerAttackSource::Samurai)) {
            PlayHitImpact(E,HitPosition);
            if (!E->IsDead()) E->GetStatusEffectComponent()->ApplyStatus(EEnemyStatusEffect::Bleed,U,EPlayerAttackSource::Samurai,true,Damage,true);
        }
    }
    Burst(Data->VFX,SamuraiPoint+Direction.Rotation().RotateVector(Data->BloodSlashAnchor),Data->VFXScale*Area*Data->AreaMultiplier,Data->VFXSeconds,Data->Color,Data->VFXRotation,Data->VFXOffset);
    if (!bEcho && U->HasUpgradeId(TEXT("BattleStance")) && U->HasUpgradeId(TEXT("BloodEcho")) && FMath::FRand()<UpgradeBalance::Value(U,TEXT("BloodEcho"),TEXT("Chance"),.15f)+UpgradeBalance::Rank(U,TEXT("BloodEchoChance"),.1f)) EchoTime=Age+FMath::Max(.05f,A->BloodEchoDelay);
}
void AJointComboActor::Volley()
{
    auto* A=Ninja->FindComponentByClass<UAutoAttackComponent>(); auto* B=Ninja->FindComponentByClass<UNinjaBuildComponent>(); auto* U=Controller->GetPlayerUpgrades();
    if (!A || !A->ProjectileClass) return;
    const bool Match=U->HasUpgradeId(TEXT("BarrageStance"));
    int32 Count=Data->Count+(Match?FMath::Max(0,A->GetEffectiveProjectileCount()-1):0); float Spacing=9.f;
    if (Match) { int32 ComboVolley=B->VolleyCount; B->ModifyVolleyWithCounter(Count,Spacing,ComboVolley); B->VolleyCount=ComboVolley; }
    Count=FMath::Clamp(Count,1,64);
    // This cinematic remains an arc even when Procession changes ordinary volleys to serial fire.
    const float HalfAngle=FMath::Clamp(FMath::Max(45.f,Spacing*(Count-1)*.5f),45.f,85.f);
    const float Damage=(Match?A->GetEffectiveAttackDamage()*Power:HeroDamage(Ninja.Get()))*Data->NinjaPower;
    if (B->KunaiThrowSound) UGameplayStatics::PlaySoundAtLocation(this,B->KunaiThrowSound,NinjaPoint,.75f);
    for (int32 I=0;I<Count;++I) {
        const FVector Aim=Direction.RotateAngleAxis(Count==1?0:FMath::Lerp(-HalfAngle,HalfAngle,float(I)/(Count-1)),FVector::UpVector);
        FActorSpawnParameters P;P.Owner=Ninja.Get();P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Shot=GetWorld()->SpawnActor<AAttackProjectileBase>(A->ProjectileClass,NinjaPoint+Aim*100,Aim.Rotation(),P);
        if (!Shot) continue; Shot->bAssistProjectile=true; Shot->AssistDamageSource=Data->AbilityId; Shot->JointOwner=this;
        Shot->InitializeProjectile(Ninja.Get(),Aim,Damage,A->GetEffectiveProjectileSpeed()*1.4f,EProjectileTargetType::Enemies,Data->Range,nullptr,true,
            Match?A->GetEffectiveProjectilePierceBonus():0,Match?A->GetEffectiveProjectileBounceBonus():0,Match?A->GetEffectiveProjectileSplitBonus():0);
        RegisterChild(Shot);
    }
    Burst(Data->SecondaryVFX,NinjaPoint,Data->SecondaryVFXScale,Data->SecondaryVFXSeconds,Data->SecondaryVFXColor,Data->SecondaryVFXRotation,Data->SecondaryVFXOffset);
}
void AJointComboActor::StartLane()
{
    auto* A=Samurai->FindComponentByClass<UAutoAttackComponent>(); auto* U=Controller->GetPlayerUpgrades();
    const bool Match=U->HasUpgradeId(TEXT("Iaijutsu"));
    const float Range=Match ? Data->Range*IaijutsuBuild::Value(U,TEXT("Iaijutsu"),TEXT("DashDistance"),800.f)/800.f : Data->Range;
    if (Match) {
        TGuardValue<FName> Source(A->AssistDamageSource,Data->AbilityId);
        A->SpawnIaijutsuSlashes(SamuraiPoint,Direction,Range,Windup,false,nullptr,Data->SamuraiPower*Power,true,true,Data->AreaMultiplier*Data->Radius/100.f,this);

    } else {
        FActorSpawnParameters P;P.Owner=Samurai.Get();
        auto* Lane=GetWorld()->SpawnActor<ASamuraiIaijutsu>(SamuraiPoint,Direction.Rotation(),P);
        if (!Lane) return; Lane->bUseRouteUpgrades=false; Lane->JointOwner=this; Lane->JointDamageSource=Data->AbilityId;
        Lane->OnJointVictim=ASamuraiIaijutsu::FJointVictim::CreateWeakLambda(this,[this](AEnemyBase* E){RecordVictim(E);});
        Lane->Initialize(Samurai.Get(),U,Direction,HeroDamage(Samurai.Get())*Data->SamuraiPower,Range,Data->Radius*Data->AreaMultiplier*Samurai->GetCharacterStats()->GetFinalAttackAreaMultiplier(),Windup,nullptr,{});
        RegisterChild(Lane);
    }
}
void AJointComboActor::RecordVictim(AEnemyBase* E)
{
    if (!IsValid(E)) return;
    Victims.Add(E,E->GetActorLocation());
    if (!FiredVictims.Contains(E)) FangQueue.AddUnique(E);
}
void AJointComboActor::GatherChargeTargets()
{
    TArray<AEnemyBase*> Targets;
    for (const auto& Child:Children) if (auto* Lane=Cast<ASamuraiIaijutsu>(Child.Get()))
        for (auto* E:Lane->GetChargeTargets()) Targets.AddUnique(E);
    Targets.Sort([this](const AEnemyBase& A,const AEnemyBase& B) {
        const float DA=FVector::DistSquared(A.GetActorLocation(),NinjaPoint),DB=FVector::DistSquared(B.GetActorLocation(),NinjaPoint);
        return DA==DB?A.GetUniqueID()<B.GetUniqueID():DA<DB;
    });
    // Follow the actual upgraded/Double Cut lanes. Leave targets that move out
    // during charging; repeat the sequence if all current targets were visited.
    FangQueue.RemoveAll([&Targets](const auto& E){return !E.IsValid() || !Targets.Contains(E.Get());});
    for (auto* E:Targets) RecordVictim(E);
    if (FangQueue.IsEmpty()) for (auto* E:Targets) FangQueue.Add(E);
}
void AJointComboActor::UpdatePerformers()
{
    if (bPerformersReleased) return;
    const float Charge=FMath::Clamp(Age/Windup,0.f,1.f);
    const float Release=FMath::Clamp((Age-Windup)/FMath::Max(.01f,ControlDuration-Windup),0.f,1.f);
    if (SamuraiVisual.IsValid()) SamuraiVisual->SetJointPose(Age<Windup?.12f*Charge:.12f+.85f*Release);
    if (NinjaVisual.IsValid()) {
        float Pose=Age<Windup?.1f*Charge:.1f+.85f*Release;
        if (Data->Technique==EComboTechnique::EclipseSeverance && LastFang>=0) {
            const float Beat=FMath::Clamp(Data->FangThrowInterval,.08f,.5f);
            const float SinceThrow=FMath::Max(0.f,Age-LastFang);
            // Reserve the return-to-ready portion for gaps and final recovery.
            // A new throw resets the montage; an empty target queue never freezes
            // the last frame while Samurai is still charging or releasing the slash.
            Pose=SinceThrow<=Beat ? FMath::Lerp(.18f,.65f,SinceThrow/Beat)
                : FMath::Lerp(.65f,.97f,FMath::Clamp((SinceThrow-Beat)/FMath::Max(.01f,ControlDuration-LastFang-Beat),0.f,1.f));
        }
        NinjaVisual->SetJointPose(Pose);
    }

}
void AJointComboActor::LaunchFangs()
{
    auto* B=Ninja->FindComponentByClass<UNinjaBuildComponent>(); auto* A=B?B->Attack():nullptr; auto* U=Controller->GetPlayerUpgrades();
    if (!B || !A) return;
    const bool Match=U->HasUpgradeId(TEXT("ReturningFang"));
    // One throw per animation beat, including the post-slash follow-through.
    if (FangQueue.IsEmpty()) return;
    const auto Key=FangQueue[0]; FangQueue.RemoveAt(0);
    auto* E=Key.Get(); FiredVictims.Add(Key); LastFang=Age;
    // Change the actual evaluated animation asset, not just the throw timing.
    UAnimMontage* Montage = (FangThrowCount % 2 == 1)
        ? (Data->NinjaAlternateMontage ? Data->NinjaAlternateMontage.Get() : A->AlternateAttackMontage.Get())
        : (Data->NinjaMontage ? Data->NinjaMontage.Get() : A->AttackMontage.Get());
    if (!Montage) Montage = Data->NinjaMontage ? Data->NinjaMontage.Get() : A->AttackMontage.Get();
    if (NinjaVisual.IsValid()) NinjaVisual->SetJointMontage(Montage);
    ++FangThrowCount;
    const FVector VictimPoint=E?E->GetActorLocation():Victims.FindRef(Key);
    Burst(Data->SecondaryVFX,NinjaPoint,Data->SecondaryVFXScale,Data->SecondaryVFXSeconds,Data->SecondaryVFXColor,Data->SecondaryVFXRotation,Data->SecondaryVFXOffset);
    if (B->KunaiThrowSound) UGameplayStatics::PlaySoundAtLocation(this,B->KunaiThrowSound,NinjaPoint,.45f);
    {
        if (!E || E->IsDead() || !E->CanReceivePlayerDamage(EPlayerAttackSource::Ninja)) {
            // The slash can kill a victim before Ninja releases. Keep her visual
            // follow-through, with a non-damaging Fang travelling to that position.
            const FVector Start=NinjaPoint+Direction*100;
            if (auto* Ghost=B->SpawnBlade(ENinjaProjectileKind::ReturningFang,Start)) {
                Ghost->bAssistProjectile=Ghost->bJointProjectile=true;Ghost->JointOwner=this;
                if (Ghost->KunaiPresentation.IsValid()) Ghost->KunaiPresentation->SetActorScale3D(FVector(FMath::Max(.01f,Data->FangPresentationScale)));
                Ghost->AssistDamageSource=Data->AbilityId;Ghost->Damage=0;Ghost->SetActorTickEnabled(false);
                const FVector Travel=VictimPoint+FVector(0,0,45)-Start;
                auto* Move=NewObject<UProjectileMovementComponent>(Ghost);
                Move->ProjectileGravityScale=0;Move->bRotationFollowsVelocity=true;
                Move->Velocity=Travel.GetSafeNormal()*4500;Move->SetUpdatedComponent(Ghost->GetRootComponent());
                Ghost->AddInstanceComponent(Move);Move->RegisterComponent();
                ScheduleAttack(FMath::Max(.02f,Travel.Size()/4500),[Blade=TWeakObjectPtr<ANinjaBuildProjectile>(Ghost)]{if(Blade.IsValid())Blade->Destroy();});RegisterChild(Ghost);
            }
            return;
        }
        if (auto* Fang=B->SpawnBlade(ENinjaProjectileKind::ReturningFang,NinjaPoint+Direction*100)) {
            Fang->Target=E; Fang->bAssistProjectile=Fang->bJointProjectile=true; Fang->bJointRouteMatched=Match; Fang->JointOwner=this;
            if (Fang->KunaiPresentation.IsValid()) Fang->KunaiPresentation->SetActorScale3D(FVector(FMath::Max(.01f,Data->FangPresentationScale)));
            Fang->AssistDamageSource=Data->AbilityId; Fang->AssistReturnOrigin=NinjaPoint;
            Fang->Damage=(Match?A->GetEffectiveAttackDamage()*Power:HeroDamage(Ninja.Get()))*Data->NinjaPower;
            if (Match) Fang->Damage*=B->FangDamageMultiplier()*(1+FMath::Max(0,A->GetEffectiveProjectileCount()-1)*B->Tune(TEXT("ReturningFang"),TEXT("CountDamage"),.25f));
            Fang->Speed=FMath::Max(4500.f,A->GetEffectiveProjectileSpeed()*Data->FangSpeedMultiplier*(Match?B->FangSpeedMultiplier():1));
            Fang->Direction=(E->GetActorLocation()-NinjaPoint).GetSafeNormal2D(); RegisterChild(Fang);
            if (Match) B->FangLaunched(Fang);
        }
    }
}
void AJointComboActor::LaunchWheel()
{
    auto* B=Ninja->FindComponentByClass<UNinjaBuildComponent>(); auto* U=Controller->GetPlayerUpgrades();
    if (!B) return;
    const bool Match=U->HasUpgradeId(TEXT("GreatShuriken"));
    WheelOrigin=NinjaPoint+Direction*120;
    Wheel=Match?B->SpawnShuriken(WheelOrigin,Direction,false):B->SpawnBlade(ENinjaProjectileKind::GreatShuriken,WheelOrigin);
    if (!Wheel.IsValid()) return;
    auto* P=Wheel.Get(); P->bAssistProjectile=P->bJointProjectile=true; P->bJointRouteMatched=Match; P->JointOwner=this; P->AssistDamageSource=Data->AbilityId;
    P->Damage=(Match?B->Attack()->GetEffectiveAttackDamage()*Power:HeroDamage(Ninja.Get()))*Data->NinjaPower;
    P->Radius=FMath::Max(125.f,Match?P->Radius:150.f)*FMath::Max(1.f,Data->ShurikenSizeMultiplier);P->InitialRadius=P->Radius;
    P->Direction=Direction;P->Speed=Match?B->GetShurikenTravelSpeed():650.f;
    P->JointLifetime=Match?B->GetShurikenLifetime():Data->Duration; P->OrbitOrigin=WheelOrigin;
    P->SetActorLocation(WheelOrigin);P->Visual->SetStaticMesh(Data->WeaponMesh?Data->WeaponMesh.Get():B->ShurikenMesh.Get());P->UpdateShurikenVisualScale();
    if (P->Visual->GetStaticMesh()) { TArray<USceneComponent*> Parts;P->Visual->GetChildrenComponents(false,Parts);for(auto* Part:Parts)Part->SetVisibility(false,true); }
    RegisterChild(P); Wheels.Add(P); LastWheelPosition=P->GetActorLocation(); LastWheelYaw=P->GetActorRotation().Yaw;
    if (Match && B->Has(TEXT("ShurikenTwin")) && ++B->ShurikenThrowCount % FMath::Max(1,FMath::RoundToInt(B->Tune(TEXT("ShurikenTwin"),TEXT("Frequency"),4.f)-UpgradeBalance::Rank(U,TEXT("ShurikenTwinFrequency"),1.f)))==0) {
        if (auto* Twin=B->SpawnShuriken(WheelOrigin,Direction.RotateAngleAxis(B->Tune(TEXT("ShurikenTwin"),TEXT("SideAngle"),20.f),FVector::UpVector),false)) {
            Twin->bAssistProjectile=Twin->bJointProjectile=Twin->bJointRouteMatched=true;Twin->JointOwner=this;Twin->AssistDamageSource=Data->AbilityId;
            Twin->Damage=P->Damage;Twin->Radius=P->Radius;Twin->InitialRadius=P->InitialRadius;Twin->JointLifetime=P->JointLifetime;Twin->OrbitOrigin=P->OrbitOrigin;Twin->OrbitAngle=P->OrbitAngle+PI;Twin->UpdateShurikenVisualScale();
            RegisterChild(Twin);Wheels.Add(Twin);
        }
    } else if (Match && !B->Has(TEXT("ShurikenTwin"))) ++B->ShurikenThrowCount;
    // The infusion is an actual travelling Crescent attack aimed at the moving wheel.
    auto* A=Samurai->FindComponentByClass<UAutoAttackComponent>();
    const FVector Aim=(WheelOrigin+Direction*200-SamuraiPoint).GetSafeNormal2D();
    if (U->HasUpgradeId(TEXT("BladeWave")) && A->BladeWaveClass) A->SpawnBladeWavesForAttack(A->GetEffectiveAttackDamage()*Data->SamuraiPower*Power,SamuraiPoint,Aim,Data->AbilityId,this,Data->AreaMultiplier);
    else {
        FActorSpawnParameters Params;Params.Owner=Samurai.Get();
        auto* Wave=GetWorld()->SpawnActor<ASamuraiBladeWave>(A->BladeWaveClass?A->BladeWaveClass:TSubclassOf<ASamuraiBladeWave>(ASamuraiBladeWave::StaticClass()),SamuraiPoint,Aim.Rotation(),Params);
        if (Wave) {Wave->DamageSource=Data->AbilityId;Wave->JointOwner=this;Wave->InitializeBladeWave(Samurai.Get(),nullptr,Aim,HeroDamage(Samurai.Get())*Data->SamuraiPower,220,500,1600,false);RegisterChild(Wave);InfusionWave=Wave;}
    }
    if (!InfusionWave.IsValid()) for (TActorIterator<ASamuraiBladeWave> It(GetWorld());It;++It) if (It->JointOwner.Get()==this) {InfusionWave=*It;break;}
}
void AJointComboActor::InjectWheel()
{
    if (!Wheel.IsValid()) return;
    const FVector Point=Wheel->GetActorLocation();
    Burst(Data->VFX,Point,Data->VFXScale,Data->VFXSeconds,Data->Color,Data->VFXRotation,Data->VFXOffset);
    if (Data->bEnableInfusionAccent) RegisterChild(AIndicatorAttackVFX::Spawn(this,Ground(Point),EEncounterShape::Circle,Wheel->Radius*1.4f,0,0,EIndicatorVFX::Slash));
    NextWave=Age; EmitWaves(Point,Wheel->GetActorRotation().Yaw); NextWave+=FMath::Max(.08f,Data->WaveInterval);
}
void AJointComboActor::EmitWaves(FVector Point,float Yaw)
{
    auto* A=Samurai->FindComponentByClass<UAutoAttackComponent>();auto* U=Controller->GetPlayerUpgrades();
    const bool Match=U->HasUpgradeId(TEXT("BladeWave"));
    for (int32 I=0;I<4;++I) {
        const FVector Aim=FRotator(0,Yaw+I*90.f,0).Vector();
        if (Match && A->BladeWaveClass) {
            A->SpawnBladeWavesForAttack(A->GetEffectiveAttackDamage()*Data->SamuraiPower*Power,Point,Aim,Data->AbilityId,this,Data->AreaMultiplier);
        } else {
            FActorSpawnParameters P;P.Owner=Samurai.Get();
            auto* Wave=GetWorld()->SpawnActor<ASamuraiBladeWave>(A->BladeWaveClass?A->BladeWaveClass:TSubclassOf<ASamuraiBladeWave>(ASamuraiBladeWave::StaticClass()),Point,Aim.Rotation(),P);
            if (Wave) { Wave->DamageSource=Data->AbilityId;Wave->JointOwner=this;Wave->InitializeBladeWave(Samurai.Get(),nullptr,Aim,HeroDamage(Samurai.Get())*Data->SamuraiPower,220*Samurai->GetCharacterStats()->GetFinalAttackAreaMultiplier(),900,1400,false);RegisterChild(Wave); }
        }
    }
}
void AJointComboActor::Tick(float Delta)
{
    Super::Tick(Delta); if (!CanContinue()) { Destroy(); return; }
    if (UGameplayStatics::IsGamePaused(this)) return;
    Age+=FMath::Max(0.f,Delta);
    RevealHelper();
    for (int32 I=DelayedAttacks.Num()-1;I>=0;--I) if (Age>=DelayedAttacks[I].Time) {
        auto Action=MoveTemp(DelayedAttacks[I].Action);DelayedAttacks.RemoveAt(I);Action();
    }
    if (Data->Technique==EComboTechnique::EclipseSeverance && !bPerformersReleased && Age<ControlDuration && Age>=NextFang) {
        if (Age<Windup) GatherChargeTargets();
        LaunchFangs();
        // Never turn a hitch into an all-at-once volley.
        NextFang=Age+FMath::Clamp(Data->FangThrowInterval,.08f,.5f);
    }
    UpdatePerformers();
    if (!bReleased && Age>=Windup) {
        bReleased=true;
        for (auto& FX:Effects) if (FX.Stop<=Windup+.01f && FX.Effect.IsValid()) FX.Effect->Deactivate();
        if (Data->ReleaseSound) UGameplayStatics::PlaySoundAtLocation(this,Data->ReleaseSound,Origin,FMath::Max(0.f,Data->ReleaseSoundVolume));
        if (Data->Technique==EComboTechnique::CrimsonTempest) BloodSlash();
        else if (Data->Technique==EComboTechnique::LunarTempest) LaunchWheel();
        else Burst(Data->VFX,Origin+Direction*Data->Range*.5f,Data->VFXScale,Data->VFXSeconds,Data->Color,Data->VFXRotation,Data->VFXOffset);
    }
    if (!bSecondary && Age>=Windup+.09f) { bSecondary=true; if (Data->Technique==EComboTechnique::CrimsonTempest) Volley(); }
    if (EchoTime>=0 && Age>=EchoTime) { EchoTime=-1; BloodSlash(true); }
    if (Data->Technique==EComboTechnique::LunarTempest && Wheel.IsValid()) {
        if (!bInjected && Age>=Windup+.12f && (Age>=Windup+.4f || !InfusionWave.IsValid() || FVector::Dist2D(InfusionWave->GetActorLocation(),Wheel->GetActorLocation())<=Wheel->Radius+120.f)) { bInjected=true;InjectWheel(); }
        int32 Budget=8;
        while (bInjected && Age>=NextWave && Budget-->0 && Wheel.IsValid()) {
            // Keep emission positions distributed across hitches; never pile a whole wave at one point.
            const float Alpha=FMath::Clamp(1.f-(Age-NextWave)/FMath::Max(.001f,Delta),0.f,1.f);
            const FVector Point=FMath::Lerp(LastWheelPosition,Wheel->GetActorLocation(),Alpha);
            const float Yaw=LastWheelYaw+FMath::FindDeltaAngleDegrees(LastWheelYaw,Wheel->GetActorRotation().Yaw)*Alpha;
            EmitWaves(Point,Yaw);
            for (const auto& Twin:Wheels) if (Twin.IsValid() && Twin!=Wheel) EmitWaves(Twin->GetActorLocation(),Twin->GetActorRotation().Yaw);
            NextWave+=FMath::Max(.08f,Data->WaveInterval);
        }
    }
    if (Wheel.IsValid()) { LastWheelPosition=Wheel->GetActorLocation(); LastWheelYaw=Wheel->GetActorRotation().Yaw; }
    for (int32 I=Effects.Num()-1;I>=0;--I) {
        auto& FX=Effects[I];if (!FX.Effect.IsValid()) { Effects.RemoveAtSwap(I);continue; }
        if (Age>=FX.Stop) FX.Effect->Deactivate();
        if (Age>FX.Stop+FMath::Max(0.f,Data->VFXCompletionSeconds)) {FX.Effect->DestroyComponent();Effects.RemoveAtSwap(I);}
    }
    if (Age>=ControlDuration) ReleasePerformers();
    Children.RemoveAll([](const auto& Child){return !Child.IsValid() || Child->IsActorBeingDestroyed();});
    if (bPerformersReleased && Children.IsEmpty() && Effects.IsEmpty() && DelayedAttacks.IsEmpty()) Destroy();
}
void AJointComboActor::ReleasePerformers(bool bCancelled)
{
    if (bPerformersReleased) return; bPerformersReleased=true;
    if (!bCancelled && bHelperArrived) if (auto* Helper=GetHelper())
        Burst(HelperSmoke(true),Helper->GetActorLocation(),Data->HelperSmokeScale,Data->HelperSmokeSeconds,FLinearColor::White,Data->HelperSmokeRotation,Data->HelperSmokeOffset,true,false);
    EndSlowMotion();
    if (SamuraiVisual.IsValid()) SamuraiVisual->Destroy();if (NinjaVisual.IsValid()) NinjaVisual->Destroy();
    if (Active.IsValid()) Active->GetVisualRoot()->SetHiddenInGame(bSavedVisualHidden,true);
    if (Controller.IsValid()) {
        if (bOwnsDamageGuard) if (auto* HP=Controller->GetPlayerHealthComponent()) HP->SetDamageEnabled(bSavedDamageEnabled && !Controller->IsPlayerDead());
        if (auto* Assist=Controller->FindComponentByClass<UInactiveCharacterAssistComponent>()) Assist->RefreshAssistEffectState();
    }
    bOwnsDamageGuard=false;
    if (CameraRig.IsValid()) CameraRig->EndJointCamera(this,bCancelled);
}
void AJointComboActor::EndPlay(const EEndPlayReason::Type Reason)
{
    ReleasePerformers(true);
    for (auto& Child:Children) if (Child.IsValid()) Child->Destroy();
    Super::EndPlay(Reason);
}
void AJointComboActor::Destroyed()
{
    // Also restore isolated/editor worlds where BeginPlay never ran.
    ReleasePerformers(true);
    Super::Destroyed();
}
