#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EncounterHazard.generated.h"

class UDecalComponent;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UMaterialInterface;
UENUM(BlueprintType)
enum class EEncounterShape : uint8 { Circle, Lane, Ring };

/** A locked ground warning followed by damage in exactly the same footprint. */
UCLASS()
class HEAVENSDIVIDE_API AEncounterHazard : public AActor
{
    GENERATED_BODY()
public:
    AEncounterHazard();
    virtual void Tick(float Delta) override;
    void Initialize(AActor* Source, EEncounterShape Shape, float RadiusOrLength, float WidthOrInnerRadius,
        float WarningSeconds, float Damage, float ActiveSeconds = .18f, float Delay = 0.f);
    bool ContainsPoint(FVector Point, float Padding = 0.f) const;
    static AEncounterHazard* Spawn(AActor* Source, FVector Center, EEncounterShape Shape, float Size, float Width,
        float Warning, float Damage, float Active = .18f, float Yaw = 0.f, float Delay = 0.f, bool bConstrainToArena = true);
    UPROPERTY(VisibleAnywhere) TObjectPtr<UDecalComponent> Visual;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> GroundSurface;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> ImpactColumn;
    bool HasVisibleWarning() const;
private:
    friend class FEncounterExpansionTest;
    friend class FBossSpellPressureTest;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Material;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> ImpactMaterial;
    UPROPERTY() TObjectPtr<UMaterialInterface> SurfaceTemplate;
    UPROPERTY() TObjectPtr<UMaterialInterface> LaneTemplate;
    UPROPERTY() TObjectPtr<UMaterialInterface> ImpactTemplate;
    TWeakObjectPtr<AActor> DamageSource;
    EEncounterShape Footprint = EEncounterShape::Circle;
    float Size = 100, Width = 0, Warning = 1, Damage = 10, ActiveTime = .18f, Delay = 0;
    float Age = 0, NextDamage = 0, ImpactAge = 0;
    bool bImpacted = false;
};
