#include "NinjaBuildComponent.h"
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
#include "NinjaCharacter.h"
#include "SurvivorAbilityComponent.h"
#include "PlayerUpgradeComponent.h"
#include "ShadowClone.h"
#include "Sound/SoundBase.h"
#include "SurvivorPlayerController.h"
#include "UObject/ConstructorHelpers.h"

UNinjaBuildComponent::UNinjaBuildComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    static ConstructorHelpers::FObjectFinder<USoundBase> ThrowSound(
        TEXT("/Game/Assets/Sounds/Ninja/KunaiThrow.KunaiThrow"));
    KunaiThrowSound = ThrowSound.Object;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> BladeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    EmbeddedBladeMesh = BladeMesh.Object;
}
ANinjaCharacter *UNinjaBuildComponent::Ninja() const
{
    return Cast<ANinjaCharacter>(GetOwner());
}
UAutoAttackComponent *UNinjaBuildComponent::Attack() const
{
    if (!CachedAttack.IsValid() && GetOwner())
        CachedAttack = GetOwner()->FindComponentByClass<UAutoAttackComponent>();
    return CachedAttack.Get();
}
UPlayerUpgradeComponent *UNinjaBuildComponent::Upgrades() const
{
    auto *PC = GetOwner() ? Cast<ASurvivorPlayerController>(GetOwner()->GetOwner()) : nullptr;
    return PC ? PC->GetPlayerUpgrades() : nullptr;
}
bool UNinjaBuildComponent::Has(FName Id) const
{
    const auto *U = Upgrades();
    return U && U->HasUpgradeId(Id);
}
float UNinjaBuildComponent::Tune(FName Id, FName Key, float Default) const
{
    auto *Card = Upgrades() ? Upgrades()->FindUpgradeDefinition(Id) : nullptr;
    return Card ? Card->GetBalanceValue(Key, Default) : Default;
}
bool UNinjaBuildComponent::IsRunning() const
{
    auto *PC = GetOwner() ? Cast<ASurvivorPlayerController>(GetOwner()->GetOwner()) : nullptr;
    return PC && PC->IsRunInProgress() && !PC->IsPlayerDead();
}
bool UNinjaBuildComponent::IsActive() const
{
    return IsRunning() && Ninja() && Ninja()->GetCharacterMode() == ECharacterMode::Active;
}
TArray<AEnemyBase *> UNinjaBuildComponent::Sweep(FVector Start, FVector End, float Radius) const
{
    TArray<AEnemyBase *> Out;
    if (!GetWorld())
        return Out;
    TSet<AEnemyBase *> Seen;
    TArray<FHitResult> Hits;
    FCollisionObjectQueryParams Types;
    Types.AddObjectTypesToQuery(ECC_Pawn);
    Types.AddObjectTypesToQuery(ECC_GameTraceChannel1);
    GetWorld()->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity, Types,
                                       FCollisionShape::MakeSphere(FMath::Max(1.f, Radius)),
                                       FCollisionQueryParams(SCENE_QUERY_STAT(NinjaBuild), false, GetOwner()));
    for (auto &Hit : Hits)
        if (auto *E = Cast<AEnemyBase>(Hit.GetActor());
            E && !E->IsDead() && E->CanReceivePlayerDamage(EPlayerAttackSource::Ninja) && E->GetHealthComponent() &&
            !E->GetHealthComponent()->IsDead() && !Seen.Contains(E))
        {
            Seen.Add(E);
            Out.Add(E);
        }
    Out.Sort([Start](const AEnemyBase &A, const AEnemyBase &B) {
        return FVector::DistSquared(Start, A.GetActorLocation()) < FVector::DistSquared(Start, B.GetActorLocation());
    });
    if (Out.Num() > 128)
        Out.SetNum(128);
    return Out;
}
TArray<AEnemyBase *> UNinjaBuildComponent::Targets(FVector Position, float Radius) const
{
    return Sweep(Position, Position, Radius);
}
AEnemyBase *UNinjaBuildComponent::Nearest(FVector Position, float Radius, AEnemyBase *Ignore) const
{
    for (auto *E : Targets(Position, Radius))
        if (E != Ignore)
            return E;
    return nullptr;
}
ANinjaBuildProjectile *UNinjaBuildComponent::SpawnBlade(ENinjaProjectileKind Kind, FVector Position)
{
    Projectiles.RemoveAll([](auto &P) { return !P.IsValid(); });
    if (Projectiles.Num() >= 128)
        return nullptr;
    if (Kind == ENinjaProjectileKind::GreatShuriken)
    {
        int32 Wheels = 0;
        for (const auto &P : Projectiles)
            if (P.IsValid() && P->Kind == ENinjaProjectileKind::GreatShuriken && ++Wheels >= 8)
                return nullptr;
    }
    FActorSpawnParameters P;
    P.Owner = GetOwner();
    P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto *Blade = GetWorld()->SpawnActor<ANinjaBuildProjectile>(Position, FRotator::ZeroRotator, P);
    if (Blade)
    {
        Blade->Build = this;
        Blade->Kind = Kind;
        Projectiles.Add(Blade);
        if (Kind != ENinjaProjectileKind::GreatShuriken)
            Blade->SetupKunaiPresentation();
    }
    return Blade;
}
void UNinjaBuildComponent::ClearProjectiles()
{
    for (auto P : Projectiles)
        if (P.IsValid())
            P->Destroy();
    Projectiles.Reset();
    Fang.Reset();
    ConsecutiveVolleys = VolleyCount = 0;
    for (auto &Pair : Embedded)
    {
        if (Pair.Key.IsValid())
            Pair.Key->OnEnemyDied.RemoveDynamic(this, &UNinjaBuildComponent::ScatterEmbedded);
        for (auto V : Pair.Value.Visuals)
            if (V.IsValid())
                V->DestroyComponent();
    }
    Embedded.Reset();
}
void UNinjaBuildComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    ClearProjectiles();
    Super::EndPlay(Reason);
}
void UNinjaBuildComponent::TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction *Tick)
{
    Super::TickComponent(Delta, Type, Tick);
    if (!IsRunning())
    {
        ClearProjectiles();
        return;
    }
    const bool Active = IsActive();
    if (bWasActive && !Active)
        VolleyCount = 0;
    bWasActive = Active;
    for (auto It = Embedded.CreateIterator(); It; ++It)
        if (!It.Key().IsValid())
            It.RemoveCurrent();
    if (!Has(TEXT("ReturningFang")))
    {
        if (Fang.IsValid())
            Fang->Destroy();
        Fang.Reset();
        return;
    }
    TargetCheck -= Delta;
    if (Active && !Ninja()->IsDashing() && !Fang.IsValid() && TargetCheck <= 0 && Attack() &&
        Attack()->IsAutoAttackEnabled())
    {
        TargetCheck = .1f;
        if (Nearest(Ninja()->GetActorLocation(), Attack()->GetEffectiveTargetingRange()))
            if (auto *P =
                    SpawnBlade(ENinjaProjectileKind::ReturningFang, Ninja()->GetActorLocation() + FVector(0, 0, 50)))
            {
                Fang = P;
                P->LaunchFang();
            }
    }
}
bool UNinjaBuildComponent::ReplaceVolley(FVector Direction, bool bAssist)
{
    if (Has(TEXT("ReturningFang")))
    {
        if(bAssist)
            if(auto* P=SpawnBlade(ENinjaProjectileKind::ReturningFang,Ninja()->GetActorLocation()+FVector(0,0,50)))
            {
                P->bAssistProjectile=true;
                P->AssistReturnOrigin=P->GetActorLocation();
                P->LaunchFang();
            }
        return true;
    }
    if (!Has(TEXT("GreatShuriken")))
        return false;
    auto *A = Attack();
    if (!A)
        return true;
    if (Direction.IsNearlyZero())
    {
        if (auto *E = Nearest(Ninja()->GetActorLocation(), A->GetEffectiveTargetingRange()))
            Direction = E->GetActorLocation() - Ninja()->GetActorLocation();
        else
            Direction = Ninja()->GetVisualForwardVector();
    }
    Direction.Z = 0;
    Direction.Normalize();
    if(auto* P=SpawnShuriken(Ninja()->GetActorLocation() + Direction * 65 + FVector(0, 0, 50), Direction, !bAssist)) P->bAssistProjectile=bAssist;
    return true;
}
ANinjaBuildProjectile *UNinjaBuildComponent::SpawnShuriken(FVector Position, FVector Direction, bool bUseGrandEntrance)
{
    auto *A = Attack();
    if (!A)
        return nullptr;
    auto *P = SpawnBlade(ENinjaProjectileKind::GreatShuriken, Position);
    if (!P)
        return nullptr;
    int32 Extra = FMath::Max(0, A->GetEffectiveProjectileCount() - 1);
    if (const auto *G = bUseGrandEntrance ? A->GetReadyGrandEntranceUpgrade() : nullptr)
    {
        Extra += FMath::RoundToInt(G->GetBalanceValue(TEXT("NinjaBonusProjectiles"), 8));
        A->bGrandEntranceReady = false;
    }
    P->Damage = A->GetEffectiveAttackDamage();
    P->Speed = FMath::Max(1.f, Tune(TEXT("GreatShuriken"), TEXT("TravelSpeed"), 900.f));
    P->Direction = Direction.GetSafeNormal();
    P->Radius = FMath::Clamp(Tune(TEXT("GreatShuriken"), TEXT("Radius"), 95) *
                                 (1 + Extra * Tune(TEXT("GreatShuriken"), TEXT("CountSize"), .12f)),
                             25.f, 300.f);
    if (ShurikenMesh)
    {
        P->Visual->SetStaticMesh(ShurikenMesh);
        TArray<USceneComponent*> Children;
        P->Visual->GetChildrenComponents(false, Children);
        for (auto* Child : Children) Child->SetVisibility(false, true);
    }
    P->UpdateShurikenVisualScale();
    return P;
}
ANinjaBuildProjectile *UNinjaBuildComponent::SpawnCloneFang(AShadowClone *InClone, FVector Position)
{
    if (!InClone || !Attack() || !Nearest(Position, Attack()->GetEffectiveTargetingRange()))
        return nullptr;
    auto *P = SpawnBlade(ENinjaProjectileKind::ReturningFang, Position);
    if (P)
    {
        P->Clone = InClone;
        P->bCloneProjectile = true;
        P->LaunchFang();
    }
    return P;
}
void UNinjaBuildComponent::ModifyVolley(FVector &Direction, int32 &Count, float &Spacing)
{
    ModifyVolleyWithCounters(Direction, Count, Spacing, VolleyCount, ConsecutiveVolleys);
}
void UNinjaBuildComponent::ModifyVolleyWithCounters(FVector &Direction, int32 &Count, float &Spacing, int32 &Volley,
                                                    int32 &Consecutive)
{
    if (!Has(TEXT("BarrageStance")))
        return;
    ++Volley;
    if (Has(TEXT("Crescendo")))
    {
        const int32 Rank = Upgrades()->GetUpgradeLevelById(TEXT("Crescendo"));
        const int32 Cap =
            FMath::Clamp(FMath::RoundToInt(Tune(TEXT("Crescendo"), TEXT("BaseCap"), 5) +
                                           FMath::Max(0, Rank - 1) * Tune(TEXT("Crescendo"), TEXT("CapPerRank"), 2)),
                         1, 64);
        const int32 Interval =
            FMath::Clamp(FMath::RoundToInt(Tune(TEXT("Crescendo"), TEXT("AttacksPerProjectile"), 3)), 1, 100);
        Consecutive = FMath::Min(Consecutive + 1, Cap * Interval);
        const int32 Bonus = FMath::Min(Consecutive / Interval, Cap);
        Count += Bonus;
        if (Bonus >= Cap)
            Consecutive = 0; // Fire the peak volley, then begin a new buildup.
    }
    if (Has(TEXT("NeedleRain")) && Volley % 4 == 0)
        Count *= 2;
    if (Has(TEXT("FocusedVolley")))
        Spacing *= .35f;
    Count = FMath::Clamp(Count, 1, 128);
}
float UNinjaBuildComponent::Hit(AEnemyBase *Enemy, float Damage, bool bEmbed, bool bAssist, bool bShuriken)
{
    if (!Enemy || !Upgrades() || Enemy->IsDead() || !Enemy->CanReceivePlayerDamage(EPlayerAttackSource::Ninja))
        return 0;
    if (Enemy->IsMarked() && Enemy->ConsumeMark())
        Damage *= 2;
    const uint8 StatusBefore=(Enemy->HasStatus(EEnemyStatusEffect::Bleed)?1:0)|(Enemy->HasStatus(EEnemyStatusEffect::Poison)?2:0);
    if (!ApplyEmbeddedHit(Enemy, Damage, Upgrades(), bEmbed))
        return 0;
    if ((Attack() && Attack()->ProjectileClass) || (bShuriken && ShurikenHitSound))
    {
        FImpactFeedbackData Feedback;
        if (Attack() && Attack()->ProjectileClass)
            Feedback = Attack()->ProjectileClass->GetDefaultObject<AAttackProjectileBase>()->ImpactFeedback;
        if (bShuriken && ShurikenHitSound) Feedback.HitSound = ShurikenHitSound;
        UImpactFeedbackLibrary::PlayImpactFeedback(
            this, Feedback,
            Enemy->GetActorLocation() + FVector(0, 0, 45), FVector::UpVector, false);
    }
    if(bAssist)
    {
        if(auto* Abilities=GetOwner()->GetOwner()->FindComponentByClass<USurvivorAbilityComponent>())
            Abilities->NotifyPartnerHit(EPlayerAttackSource::Ninja,Enemy,true,StatusBefore);

    }
    if (!Enemy->IsDead() && Has(TEXT("VenomousKunai")))
        Enemy->ApplyStatus(EEnemyStatusEffect::Poison, Upgrades(), EPlayerAttackSource::Ninja);
    return Damage;
}
