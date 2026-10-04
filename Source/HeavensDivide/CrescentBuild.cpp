#include "CrescentBuild.h"
#include "CharacterManagerComponent.h"
#include "InactiveCharacterAssistComponent.h"
#include "SamuraiCharacter.h"
#include "SurvivorPlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"

bool CrescentBuild::TryKillAssist(UWorld* World)
{
    auto* PC = World ? Cast<ASurvivorPlayerController>(World->GetFirstPlayerController()) : nullptr;
    auto* U = PC ? PC->GetPlayerUpgrades() : nullptr;
    auto* Manager = PC ? PC->GetCharacterManager() : nullptr;
    auto* Active = Manager ? Manager->GetActiveCharacter() : nullptr;
    if (!U || !U->HasUpgradeId(TEXT("BladeWave")) || !Active || !Active->IsA<ASamuraiCharacter>()
        || Active->GetCharacterMode() != ECharacterMode::Active) return false;
    if (FMath::FRand() >= Chance(U, TEXT("CrescentAssist"), TEXT("CrescentAssistChance"), .05f, .05f)) return false;
    // Defer until the death has finished; the assist rechecks active stance and busy state.
    auto* Assist = PC->FindComponentByClass<UInactiveCharacterAssistComponent>();
    if (!Assist) return false;
    World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(Assist,
        [Assist] { Assist->TryBloodAssist(); }));
    return true;
}
