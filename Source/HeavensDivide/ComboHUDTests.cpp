#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PlayerHUDWidget.h"
#include "ComboRadialWidget.h"
#include "ComboAbilityDefinition.h"
#include "ComboAbilityComponent.h"
#include "SurvivorPlayerController.h"
#include "CharacterManagerComponent.h"
#include "SamuraiCharacter.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "PlayerUpgradeComponent.h"
#include "HeavensDivideGameUserSettings.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Styling/CoreStyle.h"
#include "ImageUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/UnrealType.h"
#include "AssetCompilingManager.h"

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
    PC->GetPlayerUpgrades()->BalancePreviewMetaProfile = 0;
    auto* Samurai = World->SpawnActor<ASamuraiCharacter>();
    auto* Combo = PC->FindComponentByClass<UComboAbilityComponent>();
    auto* HUDClass = LoadClass<UPlayerHUDWidget>(nullptr, TEXT("/Game/HeavensDivide/Blueprints/UI/WBP_PlayerHUD.WBP_PlayerHUD_C"));
    auto* HUD = HUDClass ? CreateWidget<UPlayerHUDWidget>(GI, HUDClass) : nullptr;
    if (!TestNotNull(TEXT("Shipped HUD loads"), HUD))
    {
        World->DestroyWorld(false); GEngine->DestroyWorldContext(World); return false;
    }
    FindFProperty<FObjectProperty>(UCharacterManagerComponent::StaticClass(), TEXT("ActiveCharacter"))->SetObjectPropertyValue_InContainer(PC->GetCharacterManager(), Samurai);
    HUD->InitializeFromPlayerController(PC);
    const bool bScreenshots = FParse::Param(FCommandLine::Get(), TEXT("ComboMeterScreenshots"));
    const FString Output = FPaths::ProjectSavedDir() / TEXT("RadialComboHUD/Previews");
    if (bScreenshots) IFileManager::Get().MakeDirectory(*Output, true);
    const auto Render = [&](TSharedRef<SWidget> Widget, FVector2D Size, const FString& Name)
    {
        FAssetCompilingManager::Get().FinishAllCompilation();
        auto Preview = SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
            .BorderBackgroundColor(FLinearColor(.008f,.018f,.022f)).Padding(0)[Widget];
        FWidgetRenderer Renderer(false);
        auto* Target = Renderer.DrawWidget(Preview, Size);
        TArray<FColor> Pixels;
        FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
        if (TestTrue(TEXT("HUD pixels render"), Target && Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags)))
        {
            TArray64<uint8> PNG;
            FImageUtils::PNGCompressImageArray(Target->SizeX, Target->SizeY, Pixels, PNG);
            TestTrue(TEXT("Save HUD preview"), FFileHelper::SaveArrayToFile(PNG, *(Output / (Name + TEXT(".png")))));
        }
        return Pixels;
    };
    if (TestNotNull(TEXT("Circular meter replaces ink bar"), HUD->ComboRadialMeter.Get()))
    {
        auto* Meter = HUD->ComboRadialMeter.Get();
        HUD->EnsureComboPresentation();
        TestEqual(TEXT("Repeated setup does not duplicate badge"), HUD->ComboRadialMeter.Get(), Meter);
        Combo->RestoreCombo(100); HUD->UpdateComboPresentation();
        TestEqual(TEXT("Locked meter stays empty"), Meter->GetPercent(), 0.f);
        TestFalse(TEXT("Locked meter never flashes"), Meter->IsReady());
        TestTrue(TEXT("Locked state is labelled"), HUD->ComboAbilityText->GetText().ToString().Contains(TEXT("LOCKED")));
        if (bScreenshots) Render(HUD->ComboContainer->TakeWidget(), HUD->ComboMeterSize, TEXT("Locked"));
        PC->GetPlayerUpgrades()->BalancePreviewMetaProfile = 1;
        for (float Charge : {0.f, 25.f, 50.f, 75.f, 99.f, 100.f})
        {
            Combo->RestoreCombo(Charge); HUD->UpdateComboPresentation();
            TestEqual(TEXT("Ring follows actual meter"), Meter->GetPercent(), Charge / 100.f);
            TestEqual(TEXT("Only full charge is ready"), Meter->IsReady(), Charge == 100.f);
            TestEqual(TEXT("Center icon belongs to equipped ability"), Meter->GetIcon(), ComboCatalog::Definition(Combo->GetActiveAbilityId())->Icon.Get());
            if (bScreenshots)
            {
                const auto Pixels = Render(Meter->TakeWidget(), FVector2D(136,150), FString::Printf(TEXT("Ring_%03d"), FMath::RoundToInt(Charge)));
                if (Pixels.Num() == 136*150)
                    for (int32 Quarter=0; Quarter<4; ++Quarter)
                    {
                        const float Angle = (-45.f + Quarter*90.f) * PI / 180.f;
                        const int32 X = FMath::RoundToInt(68.f + 55.f*FMath::Cos(Angle));
                        const int32 Y = FMath::RoundToInt(65.f + 55.f*FMath::Sin(Angle));
                        TestEqual(TEXT("Rendered arc fills clockwise from the top"), Pixels[Y*136+X].R > 140, Charge > (Quarter+.5f)*25.f);
                    }
                if (Charge == 100.f && Pixels.Num() == 136*150)
                    for (int32 Degrees = 0; Degrees < 360; Degrees += 5)
                    {
                        const float Angle = Degrees * PI / 180.f;
                        const int32 X = FMath::RoundToInt(68.f + 55.f*FMath::Cos(Angle));
                        const int32 Y = FMath::RoundToInt(65.f + 55.f*FMath::Sin(Angle));
                        TestTrue(TEXT("Full circumference stays filled and unobscured by the icon"), Pixels[Y*136+X].R > 140);
                    }
                Render(HUD->ComboContainer->TakeWidget(), HUD->ComboMeterSize, FString::Printf(TEXT("Badge_%03d"), FMath::RoundToInt(Charge)));
            }
        }
        TestTrue(TEXT("Ready pulse changes brightness gently over time"), Meter->GetReadyPulse(.25f/HUD->ComboGlowPulseSpeed) > Meter->GetReadyPulse(.75f/HUD->ComboGlowPulseSpeed));
        if (bScreenshots)
        {
            for (int32 Height : {720,1080})
            {
                auto ScaledHUD = SNew(SDPIScaler).DPIScale(Height/1080.f)[HUD->TakeWidget()];
                Render(ScaledHUD, FVector2D(Height*16.f/9.f,Height), FString::Printf(TEXT("FullHUD_%d"), Height));
            }
            for (const auto& Entry : ComboCatalog::Entries())
            {
                auto* Definition = ComboCatalog::Definition(Entry.Id);
                if (TestNotNull(TEXT("Combo has a saved icon"), Definition ? Definition->Icon.Get() : nullptr))
                {
                    Meter->SetPresentation(Definition->Icon, 1.f, true, true, FText::FromString(TEXT("Q")), HUD->ComboLabelFont,
                        HUD->ComboChargingColor, HUD->ComboReadyColor, 1.f, 1.1f);
                    HUD->ComboAbilityText->SetText(FText::FromString(Entry.Name));
                    Render(HUD->ComboContainer->TakeWidget(), HUD->ComboMeterSize, Entry.Id.ToString());
                }
            }
            Meter->SetPresentation(nullptr, 1, true, false, FText::FromString(TEXT("Page Down")), HUD->ComboLabelFont,
                HUD->ComboChargingColor, HUD->ComboReadyColor, 1, 1.1f);
            TestEqual(TEXT("Unequipped meter cannot show stale charge"), Meter->GetPercent(), 0.f);
            TestFalse(TEXT("Unequipped meter cannot flash"), Meter->IsReady());
            Render(Meter->TakeWidget(), FVector2D(272,150), TEXT("WideSlot_LongKey"));
        }
        Combo->RestoreCombo(0); HUD->UpdateComboPresentation();
        TestFalse(TEXT("Spending meter clears ready immediately"), Meter->IsReady());
        TestEqual(TEXT("Spending meter stops flashing immediately"), Meter->GetReadyPulse(0), 0.f);
        const auto* Settings = UHeavensDivideGameUserSettings::GetHeavensDivideGameUserSettings();
        const FKey Key = Settings->GetKeyBinding(TEXT("ComboAbility"));
        if (!Key.IsGamepadKey() && !Key.IsMouseButton() && Key != EKeys::SpaceBar)
            TestEqual(TEXT("HUD displays the configured keyboard binding"), Meter->GetKeyLabel().ToString(), Key.GetDisplayName().ToString());
        HUD->bShowComboMeter = false; HUD->UpdateComboPresentation();
        TestEqual(TEXT("HUD toggle hides entire badge"), HUD->ComboContainer->GetVisibility(), ESlateVisibility::Collapsed);
    }
    HUD->ReleaseSlateResources(true);
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}
#endif
