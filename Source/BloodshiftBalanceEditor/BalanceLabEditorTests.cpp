#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "SBalanceLab.h"
#include "BalanceLabDirector.h"
#include "SurvivorPlayerController.h"
#include "PlayerUpgradeComponent.h"
#include "HealthComponent.h"
#include "CharacterManagerComponent.h"
#include "CharacterBase.h"
#include "SwapPresentationComponent.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Framework/Docking/TabManager.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "RenderingThread.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBalanceLabPanelTest, "HeavensDivide.Balance.LabEditor.Panel", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)
bool FBalanceLabPanelTest::RunTest(const FString&)
{
    TestTrue(TEXT("Balance Lab tab registered"), FGlobalTabmanager::Get()->HasTabSpawner(FName(TEXT("BloodshiftBalanceLab"))));
    const auto Widget = SNew(SBalanceLab);
    TestFalse(TEXT("Lab controls disabled outside PIE"), Widget->CanControlLab());
    Widget->Execute(TEXT("build"), TEXT("Blood"), 3);
    TestTrue(TEXT("Inactive command explains how to start"), Widget->GetFeedback().Contains(TEXT("Start Play In Editor")));
    Widget->SlatePrepass(1.f);
    const FVector2D Size(650, 1800);
    FWidgetRenderer Renderer(true);
    auto* Target = Renderer.DrawWidget(Widget, Size);
    for (int32 I = 0; I < 4; ++I) { Widget->SlatePrepass(1.f); Renderer.DrawWidget(Target, Widget, Size, 1.f / 60.f); }
    FlushRenderingCommands();
    TArray<FColor> Pixels;
    if (TestNotNull(TEXT("Panel rendered"), Target) && Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels))
    {
        TArray64<uint8> PNG;
        FImageUtils::PNGCompressImageArray(650, 1800, Pixels, PNG);
        const FString Out = FPaths::ProjectSavedDir() / TEXT("BalanceLabEditor/Panel.png");
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Out), true);
        TestTrue(TEXT("Panel preview saved"), FFileHelper::SaveArrayToFile(PNG, *Out));
    }
    return true;
}

class FExerciseBalanceLabButtons : public IAutomationLatentCommand
{
public:
    explicit FExerciseBalanceLabButtons(FAutomationTestBase* InTest) : Test(InTest), Started(FPlatformTime::Seconds()) {}
    virtual bool Update() override
    {
        auto* Lab = GEditor ? ABalanceLabDirector::Find(GEditor->PlayWorld) : nullptr;
        const auto* Player = Lab ? Cast<ASurvivorPlayerController>(Lab->GetWorld()->GetFirstPlayerController()) : nullptr;
        const auto* Active = Player && Player->GetCharacterManager() ? Player->GetCharacterManager()->GetActiveCharacter() : nullptr;
        if (!Lab || !Lab->IsReady() || (Active && Active->SwapPresentation && Active->SwapPresentation->IsBlockingAttacks()))
        {
            if (FPlatformTime::Seconds() - Started < 60.) return false;
            Test->AddError(TEXT("Lab did not become ready within 60 seconds.")); return true;
        }
        auto* PC = Cast<ASurvivorPlayerController>(Lab->GetWorld()->GetFirstPlayerController());
        auto* U = PC->GetPlayerUpgrades();
        const auto Widget = SNew(SBalanceLab);
        Test->TestTrue(TEXT("Buttons attach to live lab"), Widget->CanControlLab());
        Widget->Execute(TEXT("build"), TEXT("Blood"), 3);
        Test->TestEqual(TEXT("Build button grants chosen damage ranks"), U->GetUpgradeLevelById(TEXT("SamuraiHeavyBlade")), 3);
        Test->TestEqual(TEXT("Build button grants chosen speed ranks"), U->GetUpgradeLevelById(TEXT("SamuraiTempo")), 3);
        Test->TestTrue(TEXT("Build creates dummy"), Lab->Report().Contains(TEXT("Dummy")));
        Widget->Execute(TEXT("grant"), TEXT("BloodCritical"), 1);
        Test->TestTrue(TEXT("Grant reaches runtime upgrade pool"), U->HasUpgradeId(TEXT("BloodCritical")));
        Test->TestTrue(TEXT("Grant feedback shows accepted ranks"), Widget->GetFeedback().Contains(TEXT("Granted 1")));
        Widget->Execute(TEXT("grant"), TEXT("DoesNotExist"), 1);
        Test->TestTrue(TEXT("Invalid grant reports zero ranks"), Widget->GetFeedback().Contains(TEXT("Granted 0")));
        Widget->Execute(TEXT("meta"), TEXT("full"));
        Test->TestEqual(TEXT("Full permanent preview"), U->BalancePreviewMetaProfile, 1);
        Test->TestTrue(TEXT("Meta preserves run cards"), U->HasUpgradeId(TEXT("BloodCritical")));
        Widget->Execute(TEXT("god"), TEXT("off"));
        Test->TestTrue(TEXT("Incoming damage button"), PC->GetPlayerHealthComponent()->IsDamageEnabled());
        Widget->Execute(TEXT("god"), TEXT("on"));
        Test->TestFalse(TEXT("Invulnerability button"), PC->GetPlayerHealthComponent()->IsDamageEnabled());
        Widget->Execute(TEXT("target"), TEXT("Grunt"), 3);
        Test->TestTrue(TEXT("Target count reaches spawner"), Lab->Report().Contains(TEXT("3 alive")));
        Widget->Execute(TEXT("clear"));
        Test->TestTrue(TEXT("Clear removes live targets"), Lab->Report().Contains(TEXT("0 alive")));
        Widget->Execute(TEXT("build"), TEXT("Blood"), 0);
        Test->TestFalse(TEXT("New build clears granted cards"), U->HasUpgradeId(TEXT("BloodCritical")));
        Test->TestEqual(TEXT("New build preserves meta profile"), U->BalancePreviewMetaProfile, 1);
        Widget->Execute(TEXT("meta"), TEXT("fresh"));
        Test->TestEqual(TEXT("Fresh permanent preview"), U->BalancePreviewMetaProfile, 0);
        Widget->Execute(TEXT("combo"));
        Test->TestTrue(TEXT("Combo feedback"), Widget->GetFeedback().Contains(TEXT("meter filled")));
        Widget->Execute(TEXT("report"));
        Test->TestTrue(TEXT("Report writes successfully"), Widget->GetFeedback().Contains(TEXT("Measurement appended")));
        return true;
    }
private:
    FAutomationTestBase* Test;
    double Started;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBalanceLabPIETest, "HeavensDivide.Balance.LabEditor.PlayControls", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)
bool FBalanceLabPIETest::RunTest(const FString&)
{
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Maps/Lvl_BalanceLab")));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FExerciseBalanceLabButtons(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
