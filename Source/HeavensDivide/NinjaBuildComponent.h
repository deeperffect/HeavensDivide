#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "NinjaBuildComponent.generated.h"
class ANinjaCharacter;
class AEnemyBase;
class UPlayerUpgradeComponent;
class UAutoAttackComponent;
class UStaticMeshComponent;
class UNinjaBuildComponent;
class AShadowClone;
class AAttackProjectileBase;

UCLASS()
class HEAVENSDIVIDE_API ANinjaBuildProjectile : public AActor
{
 GENERATED_BODY()
public:
 ANinjaBuildProjectile();
 virtual void Tick(float Delta) override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 void SetupKunaiPresentation();
 TWeakObjectPtr<AAttackProjectileBase> KunaiPresentation;
 TWeakObjectPtr<AShadowClone> Clone;
 bool bCloneProjectile=false;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> Visual;
 TWeakObjectPtr<UNinjaBuildComponent> Build;
 TWeakObjectPtr<AEnemyBase> Target;
 TWeakObjectPtr<AEnemyBase> LastVictim;
 TMap<TWeakObjectPtr<AEnemyBase>,float> LastHits;
 TMap<TWeakObjectPtr<AEnemyBase>,int32> HitCounts;
 // 0 = returning fang, 1 = great shuriken, 2 = scattered fragment.
 int32 Kind=0, Streak=0;
 float Damage=0, Speed=1000, Radius=14, Age=0, FlightAge=0, SlowRemaining=0;
 float InitialRadius=0, TravelDistance=0;
 FVector Direction=FVector::ForwardVector;
 bool bReturning=false, bPursued=false, bBoostNext=false;
 void LaunchFang();
 void Finish();
};

UCLASS(ClassGroup=(Combat),meta=(BlueprintSpawnableComponent))
class HEAVENSDIVIDE_API UNinjaBuildComponent : public UActorComponent
{
 GENERATED_BODY()
 friend class FNinjaBuildsTest;
public:
 UNinjaBuildComponent();
 virtual void TickComponent(float Delta,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction) override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 UPlayerUpgradeComponent* Upgrades() const;
 ANinjaCharacter* Ninja() const;
 UAutoAttackComponent* Attack() const;
 bool Has(FName Id) const;
 bool IsActive() const;
 bool IsRunning() const;
 float Tune(FName Id,FName Key,float Default) const;
 TArray<AEnemyBase*> Targets(FVector Position,float Radius) const;
 TArray<AEnemyBase*> Sweep(FVector Start,FVector End,float Radius) const;
 AEnemyBase* Nearest(FVector Position,float Radius,AEnemyBase* Ignore=nullptr) const;
 bool ReplaceVolley(FVector Direction);
 ANinjaBuildProjectile* SpawnShuriken(FVector Position,FVector Direction,bool bUseGrandEntrance);
 ANinjaBuildProjectile* SpawnCloneFang(AShadowClone* Clone,FVector Position);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Ninja Builds|Presentation")
 TObjectPtr<class USoundBase> KunaiThrowSound;
 void ModifyVolley(FVector& Direction,int32& Count,float& Spacing);
 void ModifyVolleyWithCounters(FVector& Direction,int32& Count,float& Spacing,int32& Volley,int32& Consecutive);
 float Hit(AEnemyBase* Enemy,float Damage,bool bEmbed=true);
 static bool ApplyEmbeddedHit(AEnemyBase* Enemy,float Damage,UPlayerUpgradeComponent* Upgrades,bool bEmbed=true);
 void StageEmbedded(AEnemyBase* Enemy,float Damage);
 UFUNCTION() void ScatterEmbedded(AEnemyBase* Enemy);
 void Scatter(FVector Position,int32 Count,float Damage,float Range);
 ANinjaBuildProjectile* SpawnBlade(int32 Kind,FVector Position);
 void ClearProjectiles();
 TWeakObjectPtr<ANinjaBuildProjectile> Fang;
 TArray<TWeakObjectPtr<ANinjaBuildProjectile>> Projectiles;
 int32 VolleyCount=0, ConsecutiveVolleys=0;
private:
 struct FEmbedded {int32 Count=0;float Damage=0;TArray<TWeakObjectPtr<UStaticMeshComponent>> Visuals;};
 TMap<TWeakObjectPtr<AEnemyBase>,FEmbedded> Embedded;
 bool bWasActive=false;
 float TargetCheck=0;
};
