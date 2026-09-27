#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "AnimNotify_SpawnSamuraiSlashNiagara.h"
#include "Animation/AnimMontage.h"
#include "SamuraiCharacter.h"
#include "CharacterStatsComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "NiagaraComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSamuraiSlashScaleTest, "HeavensDivide.Combat.SamuraiSlashAreaScale",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSamuraiSlashScaleTest::RunTest(const FString&)
{
    auto* Montage = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/HeavensDivide/Blueprints/PlayerCharacters/Montages/Samurai/AM_AutoAttackSamurai.AM_AutoAttackSamurai"));
    if (!TestNotNull(TEXT("Saved autoattack montage"), Montage)) return false;
    UAnimNotify_SpawnSamuraiSlashNiagara* Notify = nullptr;
    for (const auto& Event : Montage->Notifies)
        if (auto* Candidate = Cast<UAnimNotify_SpawnSamuraiSlashNiagara>(Event.Notify)) Notify = Candidate;
    if (!TestNotNull(TEXT("Area-aware slash notify saved in montage"), Notify)) return false;
    Notify = DuplicateObject<UAnimNotify_SpawnSamuraiSlashNiagara>(Notify, GetTransientPackage());
    const FVector BaseScale = *FindFProperty<FStructProperty>(Notify->GetClass(), TEXT("Scale"))->ContainerPtrToValuePtr<FVector>(Notify);
    const float BonusStrength = *FindFProperty<FFloatProperty>(Notify->GetClass(), TEXT("AreaBonusScaleMultiplier"))->ContainerPtrToValuePtr<float>(Notify);
    *FindFProperty<FStructProperty>(Notify->GetClass(), TEXT("LocationOffset"))->ContainerPtrToValuePtr<FVector>(Notify) = FVector(30, 20, 10);
    auto* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    auto* Samurai = World->SpawnActor<ASamuraiCharacter>();
    for (float Area : {0.5f, 1.f, 2.f})
    {
        FCharacterStatModifier Modifier;
        Modifier.ModifierId = TEXT("SlashScaleTest");
        Modifier.Stat = ECharacterStatType::AttackAreaMultiplier;
        Modifier.Operation = EStatModifierOperation::Multiply;
        Modifier.Value = Area;
        Samurai->GetCharacterStats()->AddModifier(Modifier);
        Notify->Notify(Samurai->GetMesh(), Montage, FAnimNotifyEventReference());
        UNiagaraComponent* Effect = nullptr;
        for (USceneComponent* Child : Samurai->GetMesh()->GetAttachChildren())
            if (auto* FX = Cast<UNiagaraComponent>(Child)) Effect = FX;
        if (TestNotNull(TEXT("Slash spawned on mesh"), Effect))
        {
            // At double area there is one bonus unit; respect the saved editable
            // bonus strength instead of requiring the original default of one.
            const float ExpectedMultiplier = Area == 2.f ? 1.f + FMath::Max(0.f, BonusStrength) : Area;
            TestTrue(TEXT("Area applied once using authored scale and bonus strength"),
                Effect->GetRelativeScale3D().Equals(BaseScale * ExpectedMultiplier));
            TestTrue(TEXT("Authored offset preserved at every area"), Effect->GetRelativeLocation().Equals(FVector(30, 20, 10)));
            Effect->DestroyComponent();
        }
    }
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}
#endif
