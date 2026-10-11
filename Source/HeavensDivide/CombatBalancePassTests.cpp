#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "SurvivorPlayerController.h"
#include "PlayerUpgradeComponent.h"
#include "AutoAttackComponent.h"
#include "CharacterBase.h"
#include "HealthComponent.h"
#include "ExperienceComponent.h"
#include "BalanceLabDirector.h"
#include "EnemyBase.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatBalancePassTest, "HeavensDivide.Balance.FixedHealthPass", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatBalancePassTest::RunTest(const FString&)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    const auto Cleanup = [&] { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* Class = LoadClass<ASurvivorPlayerController>(nullptr, TEXT("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController.BP_SurvivorPlayerController_C"));
    auto* PC = World->SpawnActor<ASurvivorPlayerController>(Class);
    if (!TestNotNull(TEXT("Saved player controller"), PC)) { Cleanup(); return false; }
    auto* U = PC->GetPlayerUpgrades();
    FPlayerUpgradeRunState State; U->CaptureRunState(State);
    State.SamuraiMastery = State.NinjaMastery = 100; U->RestoreRunState(State);
    if (auto* Legacy = FindFProperty<FFloatProperty>(U->GetClass(), TEXT("PowerPerMasteryPoint"))) Legacy->SetPropertyValue_InContainer(U, 10.f);
    TestEqual(TEXT("100 Samurai mastery grants no hidden damage"), U->GetSamuraiPowerMultiplier(), 1.f);
    TestEqual(TEXT("100 Ninja mastery grants no hidden damage"), U->GetNinjaPowerMultiplier(), 1.f);
    TestEqual(TEXT("Mastery investment is preserved"), U->GetSamuraiMasteryPoints(), 100);
    U->BalancePreviewMetaProfile = 0; TestEqual(TEXT("Fresh lab profile ignores permanent bleed"), U->GetMetaSkillBonus(TEXT("Bleed")), 0.f);
    U->BalancePreviewMetaProfile = 1; TestEqual(TEXT("Full lab profile does not activate an unchosen Blood stance"), U->GetMetaSkillBonus(TEXT("Bleed")), 0.f);
    U->BalancePreviewMetaProfile = -1;
    auto* H = PC->GetPlayerHealthComponent(); H->RestoreCurrentHealth(100);
    TestTrue(TEXT("Saved player has short hit protection"), FMath::IsNearlyEqual(H->DamageGracePeriod, .25f));
    H->ApplyDamage(10); H->ApplyDamage(20);
    TestEqual(TEXT("Same-frame swarm hits cannot stack"), H->GetCurrentHealth(), 90.f);
    World->Tick(LEVELTICK_All, .26f); H->ApplyDamage(20);
    TestEqual(TEXT("Protection expires in game time"), H->GetCurrentHealth(), 70.f);
    auto* Enemy = World->SpawnActor<AEnemyBase>(); Enemy->GetHealthComponent()->RestoreCurrentHealth(100);
    Enemy->GetHealthComponent()->ApplyDamage(10); Enemy->GetHealthComponent()->ApplyDamage(20);
    TestEqual(TEXT("Enemy still accepts simultaneous attacks"), Enemy->GetHealthComponent()->GetCurrentHealth(), 70.f);
    auto* XP = NewObject<UExperienceComponent>(); XP->RestoreRunState(10, 0);
    TestEqual(TEXT("Existing XP curve preserved"), XP->GetXPToNextLevel(), 107);
    FindFProperty<FFloatProperty>(XP->GetClass(), TEXT("XPRequirementScale"))->SetPropertyValue_InContainer(XP, 2.f);
    TestEqual(TEXT("XP tuning actually changes requirements"), XP->GetXPToNextLevel(), 214);
    for (const TCHAR* Name : {TEXT("Samurai"), TEXT("Ninja")})
    {
        FString Path = FString(TEXT("/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_")) + Name + TEXT(".BP_") + Name + TEXT("_C");
        auto* C = LoadClass<ACharacterBase>(nullptr, *Path);
        if (TestNotNull(TEXT("Saved character class"), C))
        {
            auto* A = C->GetDefaultObject<ACharacterBase>()->FindComponentByClass<UAutoAttackComponent>();
            TestTrue(TEXT("Authored base damage matches budget"), FMath::IsNearlyEqual(A->GetBaseAttackDamage(), FCString::Strcmp(Name,TEXT("Samurai")) == 0 ? 8.f : 10.f));
            TestTrue(TEXT("Authored cadence matches budget"), FMath::IsNearlyEqual(A->GetBaseAttackInterval(), 1.2f));
        }
    }
    auto* Crescent = U->FindUpgradeDefinition(TEXT("BladeWave")); auto* Iai = U->FindUpgradeDefinition(TEXT("Iaijutsu"));
    if (TestNotNull(TEXT("Crescent stance"), Crescent)) TestTrue(TEXT("Crescent has viable base hit"), FMath::IsNearlyEqual(Crescent->GetBalanceValue(TEXT("WaveDamageMultiplier"), 0), 1.5f));
    if (TestNotNull(TEXT("Iaijutsu stance"), Iai)) TestTrue(TEXT("Charged damage compensates for cadence"), FMath::IsNearlyEqual(Iai->GetBalanceValue(TEXT("DamageMultiplier"), 0), 2.5f));
    if (Iai)
    {
        TestEqual(TEXT("Authored Iaijutsu charge"), Iai->GetBalanceValue(TEXT("ChargeDuration"), 0), 2.f);
        TestEqual(TEXT("Authored Iaijutsu half-width"), Iai->GetBalanceValue(TEXT("SlashRadius"), 0), 200.f);
    }
    auto* Lab = World->SpawnActor<ABalanceLabDirector>(); TestTrue(TEXT("Lab is discoverable for progression isolation"), ABalanceLabDirector::Find(World) == Lab);
    Cleanup(); return true;
}
#endif
