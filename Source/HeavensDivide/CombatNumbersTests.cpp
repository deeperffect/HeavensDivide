#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CombatNumberPresentation.h"
#include "PlayerHUDWidget.h"
#include "EnemyBase.h"
#include "HealthComponent.h"
#include "NinjaBuildComponent.h"
#include "SurvivorPlayerController.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatNumbersTest, "HeavensDivide.ImpactFeedback.CombatNumbers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatNumbersTest::RunTest(const FString&)
{
    auto* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    auto* PC = World->SpawnActor<ASurvivorPlayerController>();
    PC->Player = NewObject<ULocalPlayer>(GEngine);
    auto* HUD = NewObject<UPlayerHUDWidget>();
    FindFProperty<FObjectProperty>(PC->GetClass(), TEXT("PlayerHUDWidget"))->SetObjectPropertyValue_InContainer(PC, HUD);
    auto* Enemy = World->SpawnActor<AEnemyBase>();
    FindFProperty<FObjectProperty>(Enemy->GetClass(), TEXT("CachedSurvivorController"))->SetObjectPropertyValue_InContainer(Enemy, PC);
    auto* HP = Enemy->GetHealthComponent();
    HP->RestoreCurrentHealth(100);
    TestTrue(TEXT("Ordinary hit accepted"), Enemy->ApplyPlayerDamage(10, EPlayerAttackSource::Samurai));
    auto& Buffer = HUD->DamageNumbers;
    if (!TestEqual(TEXT("Accepted damage reaches the HUD"), Buffer.GetNumbers().Num(), 1))
    {
        World->DestroyWorld(false); GEngine->DestroyWorldContext(World); return false;
    }
    TestEqual(TEXT("Number uses real HP loss"), Buffer.GetNumbers()[0].Amount, 10.0);
    TestFalse(TEXT("Normal hit is not critical"), Buffer.GetNumbers()[0].bCritical);
    Enemy->ApplyPlayerDamage(20, EPlayerAttackSource::Samurai, true);
    TestEqual(TEXT("Critical does not merge into normal"), Buffer.GetNumbers().Num(), 2);
    TestEqual(TEXT("Crit has a distinct non-color cue"), Buffer.GetNumbers().Last().Text, FString(TEXT("20!")));
    UNinjaBuildComponent::ApplyEmbeddedHit(Enemy, 5, nullptr, false, true);
    TestEqual(TEXT("Ninja hit preserves the explicit crit flag"), Buffer.GetNumbers().Last().Amount, 25.0);
    const int32 BeforeStatus = Buffer.GetNumbers().Num();
    Enemy->ApplyStatusDamage(2, EPlayerAttackSource::Samurai);
    TestEqual(TEXT("Status damage changes health without adding numbers"), Buffer.GetNumbers().Num(), BeforeStatus);
    const int32 BeforeRejected = Buffer.GetNumbers().Num();
    HP->SetDamageEnabled(false);
    Enemy->ApplyPlayerDamage(50, EPlayerAttackSource::Samurai, true);
    HP->SetDamageEnabled(true);
    Enemy->ApplyPlayerDamage(0, EPlayerAttackSource::Samurai);
    TestEqual(TEXT("Invulnerable and zero hits produce no feedback"), Buffer.GetNumbers().Num(), BeforeRejected);
    Buffer.Advance(.12f);
    Enemy->ApplyPlayerDamage(1000, EPlayerAttackSource::Samurai, true);
    TestEqual(TEXT("Lethal number excludes overkill"), Buffer.GetNumbers().Last().Amount, 63.0);
    Enemy->ApplyPlayerDamage(5, EPlayerAttackSource::Samurai);
    TestEqual(TEXT("Dead enemies produce no new feedback"), Buffer.GetNumbers().Num(), BeforeRejected + 1);
    Buffer.Advance(1);
    TestTrue(TEXT("All feedback expires quickly"), Buffer.GetNumbers().IsEmpty());

    Buffer.Add(Enemy, FVector::ZeroVector, .25f, false, true);
    TestTrue(TEXT("Status ticks cannot enter the number buffer"), Buffer.GetNumbers().IsEmpty());
    HUD->ShowEnemyDamageNumber(Enemy, FVector::ZeroVector, 5.f, false, true);
    TestTrue(TEXT("HUD also rejects status ticks"), Buffer.GetNumbers().IsEmpty());
    Buffer.Add(Enemy, FVector::ZeroVector, .25f, false, false);
    Buffer.Advance(.01f);
    Buffer.Add(Enemy, FVector::ZeroVector, .25f, false, false);
    TestEqual(TEXT("Rapid direct hits still aggregate"), Buffer.GetNumbers().Num(), 1);
    TestEqual(TEXT("Fractional direct damage is readable"), Buffer.GetNumbers()[0].Text, FString(TEXT("0.5")));
    TestEqual(TEXT("Aggregation never resets age"), Buffer.GetNumbers()[0].Age, .01f);
    Buffer.Advance(.05f);
    Buffer.Reset();
    for (int32 I = 0; I < 5000; ++I) Buffer.Add(nullptr, FVector::ZeroVector, 10, false, I % 2 == 0);
    TestEqual(TEXT("Heavy swarm traffic has a fixed memory/draw budget"), Buffer.GetNumbers().Num(), FCombatNumberBuffer::Capacity);
    Buffer.Add(nullptr, FVector::ZeroVector, 100, true, false);
    TestTrue(TEXT("Critical feedback survives saturation"), Buffer.GetNumbers().Last().bCritical);
    Buffer.Reset();
    for (int32 I = 0; I < FCombatNumberBuffer::Capacity; ++I) Buffer.Add(nullptr, FVector::ZeroVector, 10, true, false);
    Buffer.Add(nullptr, FVector::ZeroVector, 1, false, true);
    TestTrue(TEXT("Status traffic cannot evict critical hits"), Buffer.GetNumbers().Last().bCritical);
    Buffer.Advance(0);
    TestEqual(TEXT("Frozen time does not advance animations"), Buffer.GetNumbers().Last().Age, 0.f);
    FCombatNumber Normal, Critical;
    Critical.bCritical = true;
    TestTrue(TEXT("Crit starts large on its very first frame"), CombatNumbers::Animate(Critical).Scale >= 1.65f);
    Normal.Age = Critical.Age = .055f; Critical.bCritical = true;
    TestTrue(TEXT("Critical pop is larger"), CombatNumbers::Animate(Critical).Scale > CombatNumbers::Animate(Normal).Scale);
    TestTrue(TEXT("Critical has brief spark accents"), CombatNumbers::Animate(Critical).Burst > 0);
    Critical.Age = .25f;
    TestEqual(TEXT("Sparks disappear before text"), CombatNumbers::Animate(Critical).Burst, 0.f);
    Critical.Age = Critical.Lifetime();
    TestEqual(TEXT("End of animation is fully transparent"), CombatNumbers::Animate(Critical).Opacity, 0.f);
    HUD->SurvivorPlayerController = PC;
    HUD->PlayerHealth = PC->GetPlayerHealthComponent();
    HUD->PlayerHealth->RestoreCurrentHealth(20);
    HUD->UpdateDamageFeedback(.2f);
    auto* Selecting = FindFProperty<FBoolProperty>(PC->GetClass(), TEXT("bLevelUpSelectionActive"));
    Selecting->SetPropertyValue_InContainer(PC, true);
    const float Phase = HUD->LowHealthPhase;
    HUD->UpdateDamageNumbers(2.f); HUD->UpdateDamageFeedback(2.f);
    TestEqual(TEXT("Upgrade menu freezes number animations despite real-time UMG ticks"), Buffer.GetNumbers().Last().Age, 0.f);
    TestEqual(TEXT("Upgrade menu freezes low-health pulse"), HUD->LowHealthPhase, Phase);
    TestEqual(TEXT("Upgrade menu hides low-health warning"), HUD->GetLowHealthFeedbackOpacity(), 0.f);
    Selecting->SetPropertyValue_InContainer(PC, false);
    TestTrue(TEXT("Closing the menu restores the warning"), HUD->GetLowHealthFeedbackOpacity() > 0.f);
    HUD->bShowDamageNumbers = false; HUD->UpdateDamageNumbers(.01f);
    TestTrue(TEXT("Disabling numbers clears active effects"), Buffer.GetNumbers().IsEmpty());
    HUD->UnbindCharacterHealth();
    World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
    return true;
}
#endif
