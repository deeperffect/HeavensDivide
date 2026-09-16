#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PlayerHUDWidget.h"
#include "SurvivorPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/CoreStyle.h"
#include "ImageUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#if WITH_EDITOR
#include "AssetCompilingManager.h"
#endif

void UpdatePlayerActionHUD(UPlayerHUDWidget& HUD);

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActionHUDTest, "HeavensDivide.UI.ActionHUD",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FActionHUDTest::RunTest(const FString&)
{
    auto* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    auto* GI = NewObject<UGameInstance>(GEngine);
    World->SetGameInstance(GI);
    auto* PC = World->SpawnActor<ASurvivorPlayerController>();
    auto* Class = LoadClass<UPlayerHUDWidget>(nullptr, TEXT("/Game/HeavensDivide/Blueprints/UI/WBP_PlayerHUD.WBP_PlayerHUD_C"));
    auto* HUD = Class ? CreateWidget<UPlayerHUDWidget>(GI, Class) : nullptr;
    if (!TestNotNull(TEXT("Actual HUD Blueprint loads"), HUD)) return false;
    HUD->InitializeFromPlayerController(PC);
    auto SlateHUD = HUD->TakeWidget();
    auto* Charges = Cast<UHorizontalBox>(HUD->WidgetTree->FindWidget(TEXT("DashChargeContainer")));
    auto* Swap = Cast<UUserWidget>(HUD->WidgetTree->FindWidget(TEXT("SwapCooldownWidget")));
    if (TestNotNull(TEXT("Dash charge row"), Charges) && TestNotNull(TEXT("Swap widget"), Swap))
    {
        auto* Icon = Cast<UImage>(Swap->WidgetTree->FindWidget(TEXT("IMG_SwapIcon")));
        TestTrue(TEXT("Swap uses generated brush arrows"), Icon && Icon->GetBrush().GetResourceObject()->GetName() == TEXT("T_SwapInkArrows"));
        for (int32 State = 0; State < 2; ++State)
        {
            TArray<FDashChargeSlotState> Slots;
            for (int32 Index = 0; Index < 3; ++Index)
            {
                FDashChargeSlotState Slot;
                Slot.SlotIndex = Index;
                Slot.State = State == 0 || Index == 0 ? EDashChargeSlotState::Full : (Index == 1 ? EDashChargeSlotState::Recharging : EDashChargeSlotState::Empty);
                Slot.RechargePercent = Slot.State == EDashChargeSlotState::Full ? 1.f : (Index == 1 ? .5f : 0.f);
                Slots.Add(Slot);
            }
            HUD->OnDashChargeSlotsUpdated(Slots);
            if (State == 0) HUD->OnSwapCooldownFinished();
            else { HUD->OnSwapCooldownStarted(4.f); HUD->OnSwapCooldownUpdated(2.f, 4.f, .5f); }
            UpdatePlayerActionHUD(*HUD);
            TestEqual(TEXT("Upgrade-sized row contains three charges"), Charges->GetChildrenCount(), 3);
            for (int32 Index = 0; Index < Charges->GetChildrenCount(); ++Index)
            {
                auto* Charge = Cast<UUserWidget>(Charges->GetChildAt(Index));
                auto* Fill = Charge ? Cast<UProgressBar>(Charge->WidgetTree->FindWidget(TEXT("ChargeFull"))) : nullptr;
                if (TestNotNull(TEXT("Dash fill"), Fill))
                {
                    TestTrue(TEXT("Dash uses shared ink texture"), Fill->GetWidgetStyle().FillImage.GetResourceObject()->GetName() == TEXT("T_ComboInkSlash"));
                    if (Index < 2) TestEqual(TEXT("Blueprint preserves ready/recharge fill"), Fill->GetPercent(), Slots[Index].RechargePercent);
                }
            }
            if (FParse::Param(FCommandLine::Get(), TEXT("ActionHUDScreenshots")))
            {
#if WITH_EDITOR
                FAssetCompilingManager::Get().FinishAllCompilation();
#endif
                // Render the shipped HUD layout so overlap with combo/health is visible.
                auto Preview = SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                    .BorderBackgroundColor(FLinearColor(.008f, .009f, .012f)).Padding(0)[SlateHUD];
                FWidgetRenderer Renderer(false);
                auto* Target = Renderer.DrawWidget(Preview, FVector2D(1920, 1080));
                TArray<FColor> Pixels;
                FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
                if (TestTrue(TEXT("Render action row"), Target && Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags)))
                {
                    int32 IconPixels = 0;
                    for (int32 Y = 857; Y < 913; ++Y)
                        for (int32 X = 1192; X < 1248; ++X)
                            if (Pixels[Y * Target->SizeX + X].R > 80) ++IconPixels;
                    TestTrue(TEXT("Swap artwork visibly renders"), IconPixels > 150);
                    TArray64<uint8> PNG;
                    FImageUtils::PNGCompressImageArray(Target->SizeX, Target->SizeY, Pixels, PNG);
                    const FString Output = FPaths::ProjectSavedDir() / TEXT("ActionHUD");
                    IFileManager::Get().MakeDirectory(*Output, true);
                    TestTrue(TEXT("Save HUD preview"), FFileHelper::SaveArrayToFile(PNG, *(Output / (State == 0 ? TEXT("Ready.png") : TEXT("Recharging.png")))));
                }
            }
        }
    }
    HUD->ReleaseSlateResources(true);
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}
#endif
