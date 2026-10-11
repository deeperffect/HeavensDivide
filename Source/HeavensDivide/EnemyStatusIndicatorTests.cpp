#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "EnemyBase.h"
#include "EnemyHealthBarWidget.h"
#include "EnemyStatusIndicatorWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UnrealType.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "ImageUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#if WITH_EDITOR
#include "AssetCompilingManager.h"
#endif

// Opt-in visual proof of the shipped assets at their actual component sizes.
// It is kept out of the ordinary, headless status regression run.
static bool SaveEnemyUIPreview(APlayerController* PC)
{
    auto* HealthClass = LoadClass<UEnemyHealthBarWidget>(nullptr,
        TEXT("/Game/HeavensDivide/Blueprints/UI/EnemyUI/WBP_EnemyHealthBar.WBP_EnemyHealthBar_C"));
    if (!HealthClass) return false;
    TArray<TStrongObjectPtr<UUserWidget>> KeepAlive;
    auto Rows = SNew(SVerticalBox);
    const FSlateFontInfo LabelFont = FCoreStyle::GetDefaultFontStyle("Regular", 10);
    const float Fractions[] = {1.f, .65f, .2f, 0.f};
    const int32 Counts[] = {1, 2, 12, 123};
    for (int32 Size = 0; Size < 2; ++Size)
    {
        const float IconSize = Size == 0 ? 24.f : 42.f;
        const float BarWidth = Size == 0 ? 60.f : 120.f;
        const float BarHeight = Size == 0 ? 16.f : 18.f;
        Rows->AddSlot().AutoHeight().Padding(18, 12, 18, 8)
            [SNew(STextBlock).Font(LabelFont).ColorAndOpacity(FLinearColor(.85f,.75f,.55f))
                .Text(FText::FromString(Size == 0 ? TEXT("SMALL ENEMIES  /  60 x 16 bar  /  24px icons")
                                               : TEXT("LARGE ENEMIES  /  120 x 18 bar  /  42px icons")))];
        auto Row = SNew(SHorizontalBox);
        for (int32 State = 0; State < 4; ++State)
        {
            auto* Health = CreateWidget<UEnemyHealthBarWidget>(PC, HealthClass);
            if (!Health) return false;
            KeepAlive.Emplace(Health);
            Health->UpdateHealthBar(Fractions[State] * 100.f, 100.f, Fractions[State]);
            auto Icons = SNew(SHorizontalBox);
            for (EEnemyStatusEffect Status : {EEnemyStatusEffect::Bleed, EEnemyStatusEffect::Poison})
            {
                auto* Icon = CreateWidget<UEnemyStatusIndicatorWidget>(PC);
                KeepAlive.Emplace(Icon);
                Icon->SetStatusPresentation(Status, Counts[State], false, Size == 0 ? 9 : 10);
                Icons->AddSlot().AutoWidth().Padding(1, 0)
                    [SNew(SBox).WidthOverride(IconSize).HeightOverride(IconSize)[Icon->TakeWidget()]];
            }
            Row->AddSlot().FillWidth(1).HAlign(HAlign_Center)
                [SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Icons]
                    + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 3, 0, 10)
                        [SNew(SBox).WidthOverride(BarWidth).HeightOverride(BarHeight)[Health->TakeWidget()]]
                    + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                        [SNew(STextBlock).Font(LabelFont).Text(FText::FromString(
                            FString::Printf(TEXT("%d%% health / %d stack%s"), FMath::RoundToInt(Fractions[State]*100),
                                Counts[State], Counts[State] == 1 ? TEXT("") : TEXT("s"))))]];
        }
        Rows->AddSlot().AutoHeight().Padding(12, 0, 12, 12)[Row];
    }
#if WITH_EDITOR
    FAssetCompilingManager::Get().FinishAllCompilation();
#endif
    auto Preview = SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
        .BorderBackgroundColor(FLinearColor(.011f, .022f, .028f)).Padding(0)[Rows];
    FWidgetRenderer Renderer(false);
    auto* Target = Renderer.DrawWidget(Preview, FVector2D(720, 300));
    TArray<FColor> Pixels;
    FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
    if (!Target || !Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags)) return false;
    TArray64<uint8> PNG;
    FImageUtils::PNGCompressImageArray(Target->SizeX, Target->SizeY, Pixels, PNG);
    const FString Output = FPaths::ProjectSavedDir() / TEXT("EnemyUIStyle");
    IFileManager::Get().MakeDirectory(*Output, true);
    return FFileHelper::SaveArrayToFile(PNG, *(Output / TEXT("NativeSizePreview.png")));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyStatusIndicatorTest, "HeavensDivide.UI.EnemyStatusIcons",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEnemyStatusIndicatorTest::RunTest(const FString&)
{
    auto* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->SetGameInstance(NewObject<UGameInstance>(GEngine));
    World->InitializeActorsForPlay(FURL());
    auto* PC = World->SpawnActor<APlayerController>();
    PC->SetPlayer(NewObject<ULocalPlayer>(GEngine));
    auto* Widget = CreateWidget<UEnemyStatusIndicatorWidget>(PC);
    auto* Bleed = LoadObject<UTexture2D>(nullptr, TEXT("/Game/HeavensDivide/Blueprints/UI/EnemyUI/BleedIcon.BleedIcon"));
    auto* Poison = LoadObject<UTexture2D>(nullptr, TEXT("/Game/HeavensDivide/Blueprints/UI/EnemyUI/PoisonIcon.PoisonIcon"));
    TestNotNull(TEXT("Shared bleed artwork loads"), Bleed);
    TestNotNull(TEXT("Shared poison artwork loads"), Poison);
    if (TestNotNull(TEXT("Status widget initializes"), Widget))
    {
        auto* Icon = Cast<UImage>(Widget->WidgetTree->FindWidget(TEXT("StatusIcon")));
        auto* Glyph = Cast<UTextBlock>(Widget->WidgetTree->FindWidget(TEXT("StatusGlyph")));
        auto* Count = Cast<UTextBlock>(Widget->WidgetTree->FindWidget(TEXT("StackCountText")));
        if (TestNotNull(TEXT("Icon image exists"), Icon) && TestNotNull(TEXT("Placeholder exists"), Glyph)
            && TestNotNull(TEXT("Stack label exists"), Count))
        {
            auto Check = [&](const FString& Label, EEnemyStatusEffect Status, UTexture2D* Override)
            {
                Widget->SetStatusPresentation(Status, 12, false, 7, Override);
                TestTrue(Label + TEXT(" displays the status artwork"), Icon->GetBrush().GetResourceObject() == (Status == EEnemyStatusEffect::Bleed ? Bleed : Poison));
                TestEqual(Label + TEXT(" image is visible"), Icon->GetVisibility(), ESlateVisibility::HitTestInvisible);
                TestEqual(Label + TEXT(" hides the letter placeholder"), Glyph->GetVisibility(), ESlateVisibility::Collapsed);
                TestEqual(Label + TEXT(" keeps the authoritative stack count"), Count->GetText().ToString(), FString(TEXT("12")));
            };
            Check(TEXT("Unassigned bleed"), EEnemyStatusEffect::Bleed, nullptr);
            Check(TEXT("Unassigned poison"), EEnemyStatusEffect::Poison, nullptr);
            // Exercise saved roster defaults, including the tactical enemies/boss
            // whose missing assignments originally displayed B and P.
            const TCHAR* Enemies[] = {
                TEXT("Mobs/BP_EnemyBear"), TEXT("Mobs/BP_EnemyCreature"), TEXT("Mobs/BP_EnemyDevilRanged"),
                TEXT("Mobs/BP_EnemyFish"), TEXT("Mobs/BP_EnemyFloatingSkeleton"), TEXT("Mobs/BP_EnemyGoblinBomb"),
                TEXT("Mobs/BP_EnemyGoblinCrawler"), TEXT("Mobs/BP_EnemyGrunt"), TEXT("Mobs/BP_TestDummy"),
                TEXT("Elites/BP_EnemyGorilla"), TEXT("Elites/BP_EnemyOgre"), TEXT("Bosses/SamuraiBoss/BP_SamuraiBoss"),
                TEXT("Bosses/CinderOracle/BP_CinderOracle"), TEXT("Tactical/BP_AshenMarcher"), TEXT("Tactical/BP_AshSeer"),
                TEXT("Tactical/BP_FangStalker"), TEXT("Tactical/BP_FrostOracle"), TEXT("Tactical/BP_GraveCantor"),
                TEXT("Tactical/BP_HexSniper"), TEXT("Tactical/BP_HornLancer"), TEXT("Tactical/BP_MireWeaver"),
                TEXT("Tactical/BP_OgreWarden"), TEXT("Tactical/BP_StormGorilla"), TEXT("Tactical/BP_WarDrummer")};
            for (const TCHAR* Enemy : Enemies)
            {
                const FString Path = FString(TEXT("/Game/HeavensDivide/Blueprints/EnemyCharacters/")) + Enemy;
                auto* Class = LoadClass<AEnemyBase>(nullptr, *(Path + TEXT(".") + FPaths::GetCleanFilename(Path) + TEXT("_C")));
                if (!TestNotNull(FString(Enemy) + TEXT(" loads"), Class)) continue;
                auto* Defaults = Class->GetDefaultObject();
                for (EEnemyStatusEffect Status : {EEnemyStatusEffect::Bleed, EEnemyStatusEffect::Poison})
                {
                    const FName Property = Status == EEnemyStatusEffect::Bleed ? TEXT("BleedStatusIcon") : TEXT("PoisonStatusIcon");
                    auto* Override = Cast<UTexture2D>(FindFProperty<FObjectPropertyBase>(AEnemyBase::StaticClass(), Property)->GetObjectPropertyValue_InContainer(Defaults));
                    Check(FString(Enemy) + TEXT(" ") + Property.ToString(), Status, Override);
                }
            }
            Widget->SetStatusPresentation(EEnemyStatusEffect::Bleed, 1, false, 7);
            TestEqual(TEXT("Single stack stays hidden when requested"), Count->GetVisibility(), ESlateVisibility::Collapsed);
            Widget->SetStatusPresentation(EEnemyStatusEffect::Poison, 1, true, 7);
            TestEqual(TEXT("Single stack can still be displayed"), Count->GetVisibility(), ESlateVisibility::HitTestInvisible);
            Widget->SetStatusPresentation(EEnemyStatusEffect::Bleed, 2, false, 7, Poison);
            TestTrue(TEXT("Explicit custom artwork is preserved"), Icon->GetBrush().GetResourceObject() == Poison);
        }
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("EnemyUIStyleScreenshots")))
        TestTrue(TEXT("Save actual-size enemy UI preview"), SaveEnemyUIPreview(PC));
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}
#endif
