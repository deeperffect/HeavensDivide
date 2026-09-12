#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "EnemyBase.h"
#include "BuildFamilyCatalog.h"
#include "UpgradePresentation.h"
#include "SurvivorAbilityComponent.generated.h"
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class ACharacterBase;
class ASurvivorPlayerController;
class UPlayerUpgradeComponent;
class UUpgradeDefinition;
class UNiagaraComponent;

/** Short-lived, collision-free ability rings and connecting streaks. */
UCLASS()
class HEAVENSDIVIDE_API AAbilityAccent : public AActor
{
 GENERATED_BODY()
public:
 AAbilityAccent();
 virtual void Tick(float Delta) override;
 void MoveAnchor(FVector Position){SetActorLocation(Position+PresentationOffset);}
 void SetRemainingLifetime(float Seconds){Lifetime=Age+FMath::Max(0.05f,Seconds);SetLifeSpan(Seconds+0.1f);}
 void Initialize(FVector End, float Radius, FLinearColor Color, float Duration, bool bBeam, const FUpgradePresentation* Settings=nullptr, int32 Stage=0);
private:
 UPROPERTY() TObjectPtr<UNiagaraComponent> Niagara;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> Visual;
 UPROPERTY() TObjectPtr<UMaterialInterface> RingMaterial;
 UPROPERTY() TObjectPtr<UMaterialInterface> BeamMaterial;
 UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Material;
 float Age=0, Lifetime=0.3f;
 FName NiagaraStartParameter,NiagaraEndParameter;
 FVector NiagaraLocalEnd=FVector::ZeroVector;
 FVector PresentationOffset=FVector::ZeroVector;
 FVector InitialScale=FVector::OneVector;
 FName FallbackIntensityParameter=TEXT("Intensity");
 bool bFadeFallback=true;
 bool bIsBeam=false;
};

/** Independent automatic ability families. One 10 Hz scheduler per player. */
UCLASS(ClassGroup=(Combat),meta=(BlueprintSpawnableComponent))
class HEAVENSDIVIDE_API USurvivorAbilityComponent : public UActorComponent
{
	friend class FMetaSkillTreeTest;
 GENERATED_BODY()
 friend class FSurvivorAbilitiesTest;
 friend class FBuildFamiliesTest;
 friend class FUpgradeTuningTest;
public:
 USurvivorAbilityComponent();
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 // Called by the existing attack notify path, never by swap or passive pulses.
 bool ExecuteSetupAssist(ACharacterBase* Character);
 void HandleSamuraiMeleeHit(FVector HitPosition);
 void NotifyPartnerHit(EPlayerAttackSource Source, AEnemyBase* Enemy);
 void RegisterFamilyHit(int32 Family, AEnemyBase* Enemy, float Damage);
 void BladeWaveImpact(AEnemyBase* Enemy, float Damage, bool bSplinter);
 void GrantBuildPreview(FString FamilyId, int32 Branch = 0);
 float Tuning(int32 Family,FName Key,float Fallback,int32 Slot=-1) const;
 UUpgradeDefinition* TuningDefinition(int32 Family,int32 Slot=-1) const;
 FBuildFamilySpec FamilySpec(int32 Family) const;
 void ApplyConfiguredStatus(int32 Family,AEnemyBase* Enemy,EEnemyStatusEffect Status,EPlayerAttackSource Source);
 const FUpgradePresentation* FamilyPresentation(int32 Family,int32 Slot=-1) const;
 AAbilityAccent* FamilyAccent(int32 Family,FVector Position,FVector End,float Radius,FLinearColor Color,float Duration=0.35f,bool bBeam=false,int32 Slot=-1,int32 Stage=0);


private:
 struct FPendingPulse
 {
  FVector Position;
  int32 Family=INDEX_NONE;
  int32 VisualFamily=INDEX_NONE, VisualSlot=-1, VisualStage=0;
  bool bFollowOwner=false, bPush=false, bReaction=false;
  TWeakObjectPtr<ACharacterBase> FollowOwner;
  TWeakObjectPtr<AEnemyBase> FollowTarget;
  float Damage=0, Radius=0, Remaining=0, Interval=0.5f;
  int32 Ticks=1;
  EPlayerAttackSource Source=EPlayerAttackSource::Other;
  FLinearColor Color;
  bool bPoison=false, bFinalBurst=false, bSkyStrike=false, bGarden=false, bBleed=false;
  TWeakObjectPtr<AAbilityAccent> FieldVisual;
 };
 struct FBuildCast
 {
  int32 Family=0, Step=0, Steps=1; uint8 Branches=0;
  FVector Origin=FVector::ZeroVector, Direction=FVector::ForwardVector, End=FVector::ZeroVector;
  float Damage=0, Area=1, Remaining=0.1f, Interval=0.3f, Life=8;
  TWeakObjectPtr<ACharacterBase> Character;
  TWeakObjectPtr<AEnemyBase> Target;
  TSet<TWeakObjectPtr<AEnemyBase>> HitEnemies;
 };
 struct FBuildMark
 {
  TWeakObjectPtr<AEnemyBase> Enemy;
  int32 Family=0; float Damage=0, Remaining=6; bool bSpent=false;
 };
 void UpdateBuildFamilies(ACharacterBase* Character);
 bool ActivateBuildFamily(int32 Family,ACharacterBase* Character);
 bool StepBuildCast(FBuildCast& Cast);
 bool BuildDamage(FBuildCast& Cast,AEnemyBase* Enemy,float Multiplier=1,bool bStatus=false,bool bMark=false,bool bPush=false,bool bOnce=false);
 void BuildCircle(FBuildCast& Cast,FVector Position,float Radius,float Multiplier=1,bool bStatus=false,bool bMark=false,bool bPush=false,bool bOnce=false,float InnerRadius=0);
 void BuildLine(FBuildCast& Cast,FVector Start,FVector End,float Width,float Multiplier=1,bool bStatus=false,bool bMark=false);
 bool Branch(int32 Family,int32 Index) const;
 float BuildMagnitude(int32 Family,const TCHAR* Suffix) const;
 void ClearBuildFamilies();
 TArray<FBuildCast> BuildCasts;
 TArray<FBuildMark> BuildMarks;
 float BuildCooldowns[20]={};
 float ReactionGates[20]={};
 bool bResolvingReaction=false;
 bool bTrailInitialized=false;
 FVector LastTrailPosition=FVector::ZeroVector;
 void UpdateAbilities();
 bool ActivateAbility(int32 Index, ACharacterBase* Character);
 TArray<AEnemyBase*> FindEnemies(FVector Position,float Radius,EPlayerAttackSource Source) const;
 void Pulse(const FPendingPulse& Pulse);
 AAbilityAccent* Accent(FVector Position,FVector End,float Radius,FLinearColor Color,float Duration=0.35f,bool bBeam=false,const FUpgradePresentation* Settings=nullptr,int32 Stage=0);
 FVector FindCrowdCenter(const TArray<AEnemyBase*>& Targets, float Radius) const;
 float Magnitude(int32 Index,const TCHAR* Suffix) const;
 float Cooldown(int32 Index) const;
 float Power(ACharacterBase* Character) const;
 mutable TMap<FName,TWeakObjectPtr<UUpgradeDefinition>> TuningCache;
 TMap<FName,double> LastVisualSpawn;
 UPROPERTY() TObjectPtr<ASurvivorPlayerController> Controller;
 UPROPERTY() TObjectPtr<UPlayerUpgradeComponent> Upgrades;
 TArray<FPendingPulse> Pending;
 TArray<TWeakObjectPtr<AAbilityAccent>> ActiveAccents;
 FTimerHandle Scheduler;
 float Cooldowns[4]={0,0,0,0};
};
