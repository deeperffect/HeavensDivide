#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PlayerHUDWidget.h"
#include "HealthComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerDamageFeedbackTest, "HeavensDivide.ImpactFeedback.PlayerDamage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlayerDamageFeedbackTest::RunTest(const FString&)
{
    auto* HUD = NewObject<UPlayerHUDWidget>();
    auto* Health = NewObject<UHealthComponent>();
    Health->RestoreCurrentHealth(100.0f);
    HUD->PlayerHealth = Health;
    HUD->BindCharacterHealth();
    HUD->BindCharacterHealth();
    TestEqual(TEXT("Initially clear"), HUD->GetDamageFeedbackOpacity(), 0.0f);
    Health->ApplyDamage(10);
    const float Peak = HUD->GetDamageFeedbackOpacity();
    TestEqual(TEXT("Actual damage flashes"), Peak, HUD->DamageVignetteOpacity);
    HUD->UpdateDamageFeedback(HUD->DamageVignetteDuration * .5f);
    TestTrue(TEXT("Flash fades smoothly"), HUD->GetDamageFeedbackOpacity() > 0 && HUD->GetDamageFeedbackOpacity() < Peak);
    Health->ApplyDamage(10);
    TestEqual(TEXT("Repeat hit refreshes without stacking"), HUD->GetDamageFeedbackOpacity(), Peak);
    HUD->UpdateDamageFeedback(1);
    TestEqual(TEXT("Flash fully expires"), HUD->GetDamageFeedbackOpacity(), 0.0f);
    Health->Heal(10);
    TestEqual(TEXT("Healing does not flash"), HUD->GetDamageFeedbackOpacity(), 0.0f);
    Health->SetDamageEnabled(false);
    Health->ApplyDamage(10);
    TestEqual(TEXT("Rejected damage does not flash"), HUD->GetDamageFeedbackOpacity(), 0.0f);
    Health->SetDamageEnabled(true);
    Health->ApplyDamage(0);
    TestEqual(TEXT("Zero damage does not flash"), HUD->GetDamageFeedbackOpacity(), 0.0f);
    HUD->bEnableDamageVignette = false;
    Health->ApplyDamage(10);
    TestEqual(TEXT("Effect can be disabled"), HUD->GetDamageFeedbackOpacity(), 0.0f);
    HUD->bEnableDamageVignette = true;
    Health->ApplyDamage(1000);
    TestEqual(TEXT("Lethal damage flashes"), HUD->GetDamageFeedbackOpacity(), Peak);
    HUD->UnbindCharacterHealth();
    TestEqual(TEXT("HUD cleanup clears feedback"), HUD->GetDamageFeedbackOpacity(), 0.0f);
    Health->RestoreCurrentHealth(100);
    Health->ApplyDamage(10);
    TestEqual(TEXT("Old health source is unbound"), HUD->GetDamageFeedbackOpacity(), 0.0f);
    HUD->PlayerHealth = Health;
    HUD->BindCharacterHealth();
    Health->RestoreCurrentHealth(30);
    HUD->UpdateDamageFeedback(.2f);
    const float LowOpacity = HUD->GetLowHealthFeedbackOpacity();
    TestTrue(TEXT("Thirty percent health activates a restrained warning"), LowOpacity > 0 && LowOpacity < .25f);
    TestFalse(TEXT("Thirty percent is low, not critical"), HUD->bCriticalHealth);
    HUD->UpdateDamageFeedback(.3f);
    TestTrue(TEXT("Warning pulses over time"), !FMath::IsNearlyEqual(LowOpacity, HUD->GetLowHealthFeedbackOpacity()));
    Health->Heal(10);
    HUD->UpdateDamageFeedback(.25f);
    TestEqual(TEXT("Healing out of danger clears warning"), HUD->GetLowHealthFeedbackOpacity(), 0.f);
    Health->RestoreCurrentHealth(15);
    HUD->UpdateDamageFeedback(.2f);
    TestTrue(TEXT("Fifteen percent upgrades warning to critical"), HUD->bCriticalHealth);
    TestTrue(TEXT("Critical warning remains bounded"), HUD->GetLowHealthFeedbackOpacity() <= HUD->LowHealthEdgeOpacity);
    HUD->bEnableLowHealthWarning = false;
    HUD->UpdateDamageFeedback(.1f);
    TestEqual(TEXT("Low-health warning can be disabled independently"), HUD->GetLowHealthFeedbackOpacity(), 0.f);
    HUD->bEnableLowHealthWarning = true;
    HUD->UpdateDamageFeedback(.2f);
    Health->ApplyDamage(100);
    TestEqual(TEXT("Death immediately suppresses persistent warning"), HUD->GetLowHealthFeedbackOpacity(), 0.f);
    TestEqual(TEXT("Death retains the original hit flash"), HUD->GetDamageFeedbackOpacity(), Peak);
    HUD->UnbindCharacterHealth();
    TestEqual(TEXT("Cleanup resets pulse phase"), HUD->LowHealthPhase, 0.f);
    return true;
}
#endif
