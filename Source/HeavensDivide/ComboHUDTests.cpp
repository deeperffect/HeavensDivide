#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PlayerHUDWidget.h"
#include "ComboAbilityComponent.h"
#include "SurvivorPlayerController.h"
#include "CharacterManagerComponent.h"
#include "SamuraiCharacter.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/ProgressBar.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "MaterialShared.h"
#include "ShaderCompiler.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Styling/CoreStyle.h"
#include "ImageUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FComboHUDTest, "HeavensDivide.Combat.ComboHUD",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FComboHUDTest::RunTest(const FString&)
{
    auto* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    auto* GI = NewObject<UGameInstance>(GEngine);
    World->SetGameInstance(GI);
    auto* PC = World->SpawnActor<ASurvivorPlayerController>();
    auto* Samurai = World->SpawnActor<ASamuraiCharacter>();
    auto* Combo = PC->FindComponentByClass<UComboAbilityComponent>();
    auto* HUD = CreateWidget<UPlayerHUDWidget>(GI);
    HUD->SurvivorPlayerController = PC;
    HUD->CharacterManager = PC->GetCharacterManager();
    FindFProperty<FObjectProperty>(UCharacterManagerComponent::StaticClass(), TEXT("ActiveCharacter"))->SetObjectPropertyValue_InContainer(HUD->CharacterManager, Samurai);
    HUD->WidgetTree->RootWidget = HUD->WidgetTree->ConstructWidget<UCanvasPanel>();
    HUD->EnsureComboPresentation();
    if (TestNotNull(TEXT("Ink meter exists"), HUD->ComboMeterBar.Get()) && TestNotNull(TEXT("Glow material is loaded and instanced"), HUD->ComboGlowInstance.Get()))
    {
        TestNotNull(TEXT("Generated ink texture is assigned"), HUD->ComboInkTexture.Get());
        const bool bScreenshots = FParse::Param(FCommandLine::Get(), TEXT("ComboMeterScreenshots"));
        const FString Output = FPaths::ProjectSavedDir() / TEXT("ComboMeter");
        if (bScreenshots) IFileManager::Get().MakeDirectory(*Output, true);
        // UI material shaders may still be queued when a cold editor starts automation.
        if (bScreenshots)
        {
#if WITH_EDITOR
            HUD->ComboGlowMaterial->GetMaterial()->ForceRecompileForRendering(EMaterialShaderPrecompileMode::Synchronous);
#endif
            if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
        }
        int32 PreviousLitPixels = 0;
        for (float Charge : {0.f, 50.f, 100.f})
        {
            Combo->RestoreCombo(Charge);
            HUD->UpdateComboPresentation();
            TestEqual(TEXT("Visible fill matches actual charge"), HUD->ComboMeterBar->GetPercent(), Charge / 100.f);
            float Glow = -1.f;
            HUD->ComboGlowInstance->GetScalarParameterValue(FMaterialParameterInfo(TEXT("GlowStrength")), Glow);
            TestTrue(TEXT("Only a full meter glows"), Charge == 100.f ? Glow > 0.f : Glow == 0.f);
            if (bScreenshots)
            {
                auto Preview = SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                    .BorderBackgroundColor(FLinearColor(.008f, .009f, .012f)).Padding(FMargin(60, 35))
                    [SNew(SBox).WidthOverride(360).HeightOverride(102)[HUD->ComboContainer->TakeWidget()]];
                FWidgetRenderer Renderer(false);
                auto* Target = Renderer.DrawWidget(Preview, FVector2D(480, 172));
                TArray<FColor> Pixels;
                FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
                // The first paint can request Slate shader permutations on demand.
                if (Target) Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags);
                if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
                Renderer.DrawWidget(Target, Preview, FVector2D(480, 172), 0.f);
                if (TestTrue(TEXT("Render ink charge bar"), Target && Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags)))
                {
                    int32 LitPixels = 0;
                    for (int32 Y = 70; Y < 120; ++Y)
                        for (int32 X = 60; X < 420; ++X)
                            if (Pixels[Y * Target->SizeX + X].R > 80) ++LitPixels;
                    if (Charge > 0.f) TestTrue(TEXT("Charge visibly increases rendered ink fill"), LitPixels > PreviousLitPixels + 100);
                    PreviousLitPixels = LitPixels;
                    TArray64<uint8> PNG;
                    FImageUtils::PNGCompressImageArray(Target->SizeX, Target->SizeY, Pixels, PNG);
                    TestTrue(TEXT("Save meter preview"), FFileHelper::SaveArrayToFile(PNG, *(Output / FString::Printf(TEXT("Charge_%03d.png"), FMath::RoundToInt(Charge)))));
                }
            }
        }
        Combo->RestoreCombo(0.f);
        HUD->UpdateComboPresentation();
        float Glow = -1.f;
        HUD->ComboGlowInstance->GetScalarParameterValue(FMaterialParameterInfo(TEXT("GlowStrength")), Glow);
        TestEqual(TEXT("Spending charge removes glow immediately"), Glow, 0.f);
    }
    HUD->ReleaseSlateResources(true);
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}
#endif
