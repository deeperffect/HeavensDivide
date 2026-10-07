#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "ComboAbilityComponent.h"
#include "CharacterBase.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FComboAbilityVFXTest, "HeavensDivide.Combat.ComboAbilityVFX",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FComboAbilityVFXTest::RunTest(const FString&)
{
    auto* Class = LoadClass<ACharacterBase>(nullptr, TEXT("/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Samurai.BP_Samurai_C"));
    if (!TestNotNull(TEXT("Samurai blueprint"), Class)) return false;
    auto* Combo = NewObject<UComboAbilityComponent>();
    Combo->ActiveSettings = Class->GetDefaultObject<ACharacterBase>()->ComboAbility;
    if(!TestNotNull(TEXT("Authored ability Niagara exists"),Combo->ActiveSettings.VFX.Get()))return false;
    TestFalse(TEXT("Samurai effect stays in world space"),Combo->ActiveSettings.bAttachVFXToCharacter);
    TestFalse(TEXT("Samurai effect plays once"),Combo->ActiveSettings.bSpawnVFXEveryPulse);
    TestTrue(TEXT("Samurai preserves authored Niagara scale"),Combo->ActiveSettings.VFXRadiusParameter.IsNone());
    TestTrue(TEXT("Samurai preserves authored Niagara timing"),Combo->ActiveSettings.VFXDurationParameter.IsNone());
    // Exercise optional radius scaling independently of the Samurai's authored-look configuration.
    Combo->ActiveSettings.VFX=LoadObject<UNiagaraSystem>(nullptr,TEXT("/Game/HeavensDivide/VFX/Upgrades/NS_SamuraiActive.NS_SamuraiActive"));
    Combo->ActiveSettings.VFXRadiusParameter=TEXT("User._Scale");
    Combo->ActiveSettings.bVFXRadiusParameterIsScale=true;
    const float ReferenceRadius = Combo->ActiveSettings.VFXReferenceRadius;
    TestTrue(TEXT("Reference radius is positive"), ReferenceRadius > 0.f);
    auto* FX = NewObject<UNiagaraComponent>();
    FX->SetAsset(Combo->ActiveSettings.VFX);
    Combo->ActiveVFX = FX;
    const FNiagaraVariable Scale(FNiagaraTypeDefinition::GetFloatDef(), Combo->ActiveSettings.VFXRadiusParameter);
    TestTrue(TEXT("Effect exposes a float scale input"), Combo->ActiveSettings.VFX->GetExposedParameters().IndexOf(Scale) != INDEX_NONE);
    const float AuthoredScale = Combo->ActiveSettings.VFX->GetExposedParameters().GetParameterValue<float>(Scale);
    Combo->ActiveSettings.VFXScale = FVector(.5f);
    for (const float Radius : {600.f, 700.f, 900.f, 1200.f, 600.f})
    {
        Combo->UpdateVFXRadius(Radius);
        TestEqual(TEXT("Current radius controls scale without accumulation"), FX->GetOverrideParameters().GetParameterValue<float>(Scale), Radius / FMath::Max(.01f, ReferenceRadius) * AuthoredScale * .5f);
    }
    Combo->ActiveSettings.bVFXRadiusParameterIsScale = false;
    Combo->ActiveSettings.VFXRadiusParameter = TEXT("User.Radius");
    Combo->ActiveSettings.VFXReferenceRadius = 1;
    Combo->UpdateVFXRadius(900);
    TestEqual(TEXT("Default preserves centimeter radius parameters"), FX->GetOverrideParameters().GetParameterValue<float>(
        FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("User.Radius"))), 900.f);
    Combo->PendingVFX.Add({FX, nullptr, 2.f, 3.f, false});
    Combo->FinishAbility(false);
    TestEqual(TEXT("Normal ability end retains independent VFX lifetime"), Combo->PendingVFX.Num(), 1);
    TestTrue(TEXT("Normal completion does not destroy VFX"), IsValid(FX));
    TestNull(TEXT("Completed ability releases active VFX reference"), Combo->ActiveVFX.Get());
    Combo->UpdateVFXLifetime(1.f);
    TestFalse(TEXT("VFX continues emitting before visibility duration"), Combo->PendingVFX[0].bStopping);
    Combo->UpdateVFXLifetime(1.5f);
    TestTrue(TEXT("Visibility deadline begins graceful completion"), Combo->PendingVFX[0].bStopping);
    TestEqual(TEXT("Overshoot is included in completion timeout"), Combo->PendingVFX[0].CompletionRemaining, 2.5f);
    Combo->UpdateVFXLifetime(2.5f);
    TestTrue(TEXT("Completion timeout destroys lingering effect"), FX->IsBeingDestroyed());
    TestTrue(TEXT("Completion timeout removes tracking"), Combo->PendingVFX.IsEmpty());
    auto* CancelledFX = NewObject<UNiagaraComponent>();
    Combo->PendingVFX.Add({CancelledFX, nullptr, 10.f, 5.f, false});
    Combo->FinishAbility(true);
    TestTrue(TEXT("Cancellation clears effects even after ability ended"), CancelledFX->IsBeingDestroyed());
    TestTrue(TEXT("Cancellation removes pending effects"), Combo->PendingVFX.IsEmpty());
    return true;
}
#endif
