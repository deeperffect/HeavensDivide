#include "AttackProjectileBase.h"
#include "AutoAttackComponent.h"
#include "CharacterManagerComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnemyBase.h"
#include "EnemyStatusEffectComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "HealthComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NinjaBuildComponent.h"
#include "NinjaCharacter.h"
#include "PlayerUpgradeComponent.h"
#include "ShadowClone.h"
#include "Sound/SoundBase.h"
#include "SurvivorPlayerController.h"
#include "UObject/ConstructorHelpers.h"

bool UNinjaBuildComponent::ApplyEmbeddedHit(AEnemyBase *Enemy, float Damage, UPlayerUpgradeComponent *U, bool bEmbed)
{
    if (!Enemy || !Enemy->CanReceivePlayerDamage(EPlayerAttackSource::Ninja) || !Enemy->GetHealthComponent() ||
        !Enemy->GetHealthComponent()->IsDamageEnabled() || !FMath::IsFinite(Damage) || Damage <= 0)
        return false;
    if (bEmbed && U && U->HasUpgradeId(TEXT("EmbeddedBlades")))
        if (auto *PC = Cast<ASurvivorPlayerController>(U->GetOwner());
            PC && PC->GetCharacterManager() && PC->GetCharacterManager()->GetNinja())
            if (auto *B = PC->GetCharacterManager()->GetNinja()->FindComponentByClass<UNinjaBuildComponent>())
                B->StageEmbedded(Enemy, Damage);
    return Enemy->ApplyPlayerDamage(Damage, EPlayerAttackSource::Ninja);
}

void UNinjaBuildComponent::StageEmbedded(AEnemyBase *Enemy, float Damage)
{
    if (!Enemy || !Upgrades() || (Embedded.Num() >= 512 && !Embedded.Contains(Enemy)))
        return;
    auto &E = Embedded.FindOrAdd(Enemy);
    const int32 Added = FMath::Min(1 + Upgrades()->GetUpgradeLevelById(TEXT("FragmentLoad")), 12 - E.Count);
    if (Added <= 0)
        return;
    E.Count += Added;
    E.Damage += Damage * Added * Tune(TEXT("EmbeddedBlades"), TEXT("DamageFraction"), .4f) *
                (1 + Upgrades()->GetAccumulatedUpgradeMagnitude(TEXT("FragmentDamage")));
    Enemy->OnEnemyDied.AddUniqueDynamic(this, &UNinjaBuildComponent::ScatterEmbedded);
    if (E.Visuals.Num() < 4)
    {
        auto *V = NewObject<UStaticMeshComponent>(Enemy);
        V->SetStaticMesh(EmbeddedBladeMesh);
        V->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        V->SetCastShadow(false);
        V->SetupAttachment(Enemy->GetRootComponent());
        V->RegisterComponent();
        V->SetRelativeLocation(FVector(0, 20 * (E.Visuals.Num() - 1.f), 35));
        V->SetRelativeRotation(FRotator(0, 30 + E.Visuals.Num() * 40, 25));
        V->SetRelativeScale3D(FVector(.55f, .05f, .04f));
        E.Visuals.Add(V);
    }
}

void UNinjaBuildComponent::ScatterEmbedded(AEnemyBase *Enemy)
{
    auto *Found = Embedded.Find(Enemy);
    if (!Found)
        return;
    auto E = *Found;
    Embedded.Remove(Enemy);
    Enemy->OnEnemyDied.RemoveDynamic(this, &UNinjaBuildComponent::ScatterEmbedded);
    for (auto V : E.Visuals)
        if (V.IsValid())
            V->DestroyComponent();
    if (IsRunning())
        Scatter(Enemy->GetActorLocation() + FVector(0, 0, 45), E.Count, E.Damage / FMath::Max(1, E.Count),
                Tune(TEXT("EmbeddedBlades"), TEXT("Range"), 700) *
                    (1 + Upgrades()->GetAccumulatedUpgradeMagnitude(TEXT("FragmentReach"))));
}

void UNinjaBuildComponent::Scatter(FVector Position, int32 Count, float Damage, float Range)
{
    auto Enemies = Targets(Position, Range);
    if (Enemies.IsEmpty())
        return;
    for (int32 i = 0; i < FMath::Min(Count, 24); ++i)
        if (auto *P = SpawnBlade(ENinjaProjectileKind::Fragment, Position))
        {
            P->Damage = Damage;
            P->Speed = 1400;
            P->Direction =
                (Enemies[i % Enemies.Num()]->GetActorLocation() + FVector(0, 0, 45) - Position).GetSafeNormal();
        }
}
