#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ComboAbilityDefinition.h"
#include "JointComboActor.generated.h"

class ACharacterBase;
class ASamuraiCharacter;
class ANinjaCharacter;
class ASurvivorPlayerController;
class ASwapAfterimage;
class AEnemyBase;
class ANinjaBuildProjectile;
class ASamuraiBladeWave;
class APlayerCameraRig;
class UNiagaraComponent;

/** One synchronized performance; native stance actors retain ownership of their hit/proc rules. */
UCLASS(Transient, NotBlueprintable)
class HEAVENSDIVIDE_API AJointComboActor : public AActor
{
    GENERATED_BODY()
public:
    AJointComboActor();
    static AJointComboActor* Spawn(ACharacterBase* Leader, UComboAbilityDefinition* Definition, float Power);
    FVector GetBloodSlashCenter() const;
    float GetBloodSlashRadius() const;
    TArray<AEnemyBase*> GetBloodSlashTargets() const;
    void RecordVictim(AEnemyBase* Enemy);
    void PlayHitImpact(AEnemyBase* Enemy, TOptional<FVector> HitPosition = {});
    void RegisterChild(AActor* Child);
    void RegisterEffect(UNiagaraComponent* Effect);
    void ScheduleAttack(float Delay, TFunction<void()> Action);
    float GetWindup() const { return Windup; }
    float GetControlDuration() const { return ControlDuration; }
    float GetAge() const { return Age; }
    virtual void Tick(float Delta) override;
    virtual void Destroyed() override;
    void ReleasePerformers(bool bCancelled = false);
private:
    friend class FJointComboTest;
    friend class FComboAbilityVFXTest;
    void Initialize(ACharacterBase* Leader, UComboAbilityDefinition* Definition, float Power);
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    bool CanContinue() const;
    float HeroDamage(ACharacterBase* Hero) const;
    FVector Ground(FVector Point) const;
    void Burst(UNiagaraSystem* System, FVector Point, float Scale, float Seconds, FLinearColor Color, FRotator Rotation = {}, FVector Offset = {}, bool bGroundAnchor = true, bool bTint = true);
    void BloodSlash(bool bEcho = false);
    void Volley();
    void StartLane();
    void LaunchFangs();
    void GatherChargeTargets();
    void UpdatePerformers();
    void RevealHelper();
    UNiagaraSystem* HelperSmoke(bool bDeparture) const;
    ASwapAfterimage* GetHelper() const;
    float GetReleaseSpeed() const;
    float GetWorldSpeed() const;
    void BeginSlowMotion();
    void EndSlowMotion();
    void CompensateActor(AActor* Actor);
    void UpdateSlowMotion(UWorld* World, ELevelTick TickType, float Delta);
    void LaunchWheel();
    void InjectWheel();
    void EmitWaves(FVector Point, float Yaw);
    TArray<AEnemyBase*> Enemies(FVector Point, float Radius) const;
    UPROPERTY() TObjectPtr<UComboAbilityDefinition> Data;
    TWeakObjectPtr<ACharacterBase> Active;
    TWeakObjectPtr<ASamuraiCharacter> Samurai;
    TWeakObjectPtr<ANinjaCharacter> Ninja;
    TWeakObjectPtr<ASurvivorPlayerController> Controller;
    TWeakObjectPtr<APlayerCameraRig> CameraRig;
    TWeakObjectPtr<ASwapAfterimage> SamuraiVisual, NinjaVisual;
    TWeakObjectPtr<ANinjaBuildProjectile> Wheel;
    TMap<TWeakObjectPtr<AEnemyBase>,FVector> Victims;
    TSet<TWeakObjectPtr<AEnemyBase>> FiredVictims;
    TArray<TWeakObjectPtr<AEnemyBase>> FangQueue;
    TArray<TWeakObjectPtr<ANinjaBuildProjectile>> Wheels;
    TWeakObjectPtr<ASamuraiBladeWave> InfusionWave;
    FVector LastWheelPosition;
    TArray<TWeakObjectPtr<AActor>> Children;
    struct FEffect { TWeakObjectPtr<UNiagaraComponent> Effect; float Stop; };
    TArray<FEffect> Effects;
    FVector Origin, Direction, SamuraiPoint, NinjaPoint, WheelOrigin;
    float Age=0, Windup=.65f, ControlDuration=1.8f, Power=1, NextWave=0;
    float HelperArrivalTime=0;
    TMap<TWeakObjectPtr<AEnemyBase>,float> LastHitEffects;
    bool bHelperArrived=false;
    float NextFang=.06f, LastFang=-1.f, LastWheelYaw=0;
    int32 FangThrowCount = 0;
    float SavedCinematicSpeed=1, AppliedCinematicSpeed=1, SlowCompensation=1;
    TMap<TWeakObjectPtr<AActor>, float> CompensatedActors;
    struct FEffectClock { bool bSolo; float SavedRate, AppliedRate; };
    TMap<TWeakObjectPtr<UNiagaraComponent>, FEffectClock> SoloEffects;
    struct FDelayedAttack { float Time; TFunction<void()> Action; };
    TArray<FDelayedAttack> DelayedAttacks;
    FDelegateHandle SlowTickHandle;
    bool bSlowClockStarted=false;
    float EchoTime=-1;
    bool bReleased=false, bSecondary=false, bInjected=false, bPerformersReleased=false;
    bool bSavedVisualHidden=false, bSavedDamageEnabled=true, bOwnsDamageGuard=false;
};
