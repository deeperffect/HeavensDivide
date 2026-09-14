#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NinjaBuildProjectile.h"
#include "NinjaBuildComponent.generated.h"
class ANinjaCharacter;
class AEnemyBase;
class UPlayerUpgradeComponent;
class UAutoAttackComponent;
class UStaticMeshComponent;
class UNinjaBuildComponent;
class AShadowClone;
class AAttackProjectileBase;

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class HEAVENSDIVIDE_API UNinjaBuildComponent : public UActorComponent
{
    GENERATED_BODY()
    friend class FNinjaBuildsTest;

  public:
    UNinjaBuildComponent();
    virtual void TickComponent(float Delta, ELevelTick TickType,
                               FActorComponentTickFunction *ThisTickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UPlayerUpgradeComponent *Upgrades() const;
    ANinjaCharacter *Ninja() const;
    UAutoAttackComponent *Attack() const;
    bool Has(FName Id) const;
    bool IsActive() const;
    bool IsRunning() const;
    float Tune(FName Id, FName Key, float Default) const;
    float GetShurikenTravelSpeed() const;
    float GetShurikenLifetime() const;
    TArray<AEnemyBase *> Targets(FVector Position, float Radius) const;
    TArray<AEnemyBase *> Sweep(FVector Start, FVector End, float Radius) const;
    AEnemyBase *Nearest(FVector Position, float Radius, AEnemyBase *Ignore = nullptr) const;
    bool ReplaceVolley(FVector Direction, bool bAssist = false);
    ANinjaBuildProjectile *SpawnShuriken(FVector Position, FVector Direction, bool bUseGrandEntrance);
    ANinjaBuildProjectile *SpawnCloneFang(AShadowClone *Clone, FVector Position);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ninja Builds|Presentation")
    TObjectPtr<class USoundBase> KunaiThrowSound;
    /** Optional flat XY mesh, spinning around Z. Used by player, Tag Team and clones. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ninja Builds|Shuriken", meta=(DisplayName="Shuriken Mesh"))
    TObjectPtr<class UStaticMesh> ShurikenMesh;
    /** Overrides the giant shuriken's hit sound for player, assist and clone attacks. Empty uses the normal projectile hit sound. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ninja Builds|Shuriken")
    TObjectPtr<class USoundBase> ShurikenHitSound;
    /** Overrides the attack montage's throw sound for giant shurikens. Empty keeps the authored montage sound. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ninja Builds|Shuriken")
    TObjectPtr<class USoundBase> ShurikenThrowSound;
    /** Cosmetic scale after fitting the mesh to the attack radius; does not change hit detection. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ninja Builds|Shuriken", meta=(ClampMin="0.01"))
    float ShurikenMeshScale = 1.f;
    void ModifyVolley(FVector &Direction, int32 &Count, float &Spacing);
    void ModifyVolleyWithCounters(FVector &Direction, int32 &Count, float &Spacing, int32 &Volley, int32 &Consecutive);
    float Hit(AEnemyBase *Enemy, float Damage, bool bEmbed = true, bool bAssist = false, bool bShuriken = false);
    static bool ApplyEmbeddedHit(AEnemyBase *Enemy, float Damage, UPlayerUpgradeComponent *Upgrades,
                                 bool bEmbed = true);
    void StageEmbedded(AEnemyBase *Enemy, float Damage);
    UFUNCTION() void ScatterEmbedded(AEnemyBase *Enemy);
    void Scatter(FVector Position, int32 Count, float Damage, float Range);
    ANinjaBuildProjectile *SpawnBlade(ENinjaProjectileKind Kind, FVector Position);
    void ClearProjectiles();
    TWeakObjectPtr<ANinjaBuildProjectile> Fang;
    TArray<TWeakObjectPtr<ANinjaBuildProjectile>> Projectiles;
    int32 VolleyCount = 0, ConsecutiveVolleys = 0;

  private:
    struct FEmbedded
    {
        int32 Count = 0;
        float Damage = 0;
        TArray<TWeakObjectPtr<UStaticMeshComponent>> Visuals;
    };
    TMap<TWeakObjectPtr<AEnemyBase>, FEmbedded> Embedded;
    UPROPERTY() TObjectPtr<class UStaticMesh> EmbeddedBladeMesh;
    mutable TWeakObjectPtr<UAutoAttackComponent> CachedAttack;
    bool bWasActive = false;
    float TargetCheck = 0;
};
