#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "CombatNumberPresentation.h"
#include "PlayerHUDWidget.h"
#include "SurvivorPlayerController.h"
#include "CharacterManagerComponent.h"
#include "SamuraiCharacter.h"
#include "NinjaCharacter.h"
#include "HealthComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Rendering/DrawElementTypes.h"
#include "Styling/CoreStyle.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "AssetCompilingManager.h"
#include "UObject/UnrealType.h"
#include "RenderingThread.h"

class SCombatNumberPreview : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SCombatNumberPreview) {} SLATE_END_ARGS()
    void Construct(const FArguments&) {}
    float Age = .055f;
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1920, 1080); }
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& Out,
        int32 Layer, const FWidgetStyle&, bool) const override
    {
        for (int32 I = 0; I < 2; ++I)
        {
            FCombatNumber N; N.Age = Age; N.bCritical = I == 1;
            N.Text = I == 1 ? TEXT("248!") : TEXT("124");
            const FVector2D Anchor(G.GetLocalSize().X * (.44f + .12f * I), G.GetLocalSize().Y * .44f);
            CombatNumbers::Draw(N, Anchor, G, Out, Layer + 1);
            FSlateDrawElement::MakeText(Out, Layer + 3,
                G.ToPaintGeometry(FVector2D(160,30),FSlateLayoutTransform(Anchor + FVector2D(-55,38))),
                I == 1 ? TEXT("CRITICAL") : TEXT("NORMAL"),
                FCoreStyle::GetDefaultFontStyle("Regular", 13), ESlateDrawEffect::None, FLinearColor(.8f,.8f,.75f));
        }
        return Layer + 3;
    }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatFeedbackVisualTest, "HeavensDivide.ImpactFeedback.Visual",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)
bool FCombatFeedbackVisualTest::RunTest(const FString&)
{
    auto* W = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W); W->InitializeActorsForPlay(FURL());
    auto* GI = NewObject<UGameInstance>(GEngine); W->SetGameInstance(GI);
    auto* PC = W->SpawnActor<ASurvivorPlayerController>();
    auto* Samurai = W->SpawnActor<ASamuraiCharacter>(); auto* Ninja = W->SpawnActor<ANinjaCharacter>();
    for (auto Pair : {TPair<FName,ACharacterBase*>(TEXT("ActiveCharacter"),Samurai),
        TPair<FName,ACharacterBase*>(TEXT("SamuraiCharacter"),Samurai), TPair<FName,ACharacterBase*>(TEXT("NinjaCharacter"),Ninja)})
        FindFProperty<FObjectProperty>(UCharacterManagerComponent::StaticClass(),Pair.Key)->SetObjectPropertyValue_InContainer(PC->GetCharacterManager(),Pair.Value);
    auto* Class = LoadClass<UPlayerHUDWidget>(nullptr, TEXT("/Game/HeavensDivide/Blueprints/UI/WBP_PlayerHUD.WBP_PlayerHUD_C"));
    auto* HUD = Class ? CreateWidget<UPlayerHUDWidget>(GI, Class) : nullptr;
    if (!TestNotNull(TEXT("Shipped HUD loads with new feedback defaults"), HUD))
    { W->DestroyWorld(false); GEngine->DestroyWorldContext(W); return false; }
    TestTrue(TEXT("Damage numbers enabled in the shipped Blueprint"), HUD->bShowDamageNumbers);
    TestTrue(TEXT("Low health enabled in the shipped Blueprint"), HUD->bEnableLowHealthWarning);
    TestTrue(TEXT("Shipped HUD uses the stronger vignette"),HUD->LowHealthEdgeOpacity>=.30f);
    HUD->InitializeFromPlayerController(PC);
    PC->GetPlayerHealthComponent()->RestoreCurrentHealth(12);
    HUD->UpdateDamageFeedback(.2f);
    auto PreviewNumbers = SNew(SCombatNumberPreview);
    auto Content = SNew(SOverlay)
        + SOverlay::Slot()[HUD->TakeWidget()]
        + SOverlay::Slot()[PreviewNumbers];
    FAssetCompilingManager::Get().FinishAllCompilation();
    const FString Directory = FPaths::ProjectSavedDir()/TEXT("CombatFeedback/Previews");
    IFileManager::Get().MakeDirectory(*Directory, true);
    const auto Render = [&](int32 Height, float Age, const TCHAR* Name)
    {
        PreviewNumbers->Age = Age;
        auto Frame = SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
            .BorderBackgroundColor(FLinearColor(.10f,.115f,.075f)).Padding(0)
            [SNew(SDPIScaler).DPIScale(Height/1080.f)[Content]];
        FWidgetRenderer Renderer(false);
        auto* Target = Renderer.DrawWidget(Frame, FVector2D(Height*16/9,Height));
        for (int32 I=0; I<3; ++I) Renderer.DrawWidget(Target, Frame, FVector2D(Height*16/9,Height), 0.f);
        FlushRenderingCommands();
        TArray<FColor> Pixels; FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
        if (TestTrue(TEXT("Combat feedback renders"), Target && Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags)))
        {
            TArray64<uint8> PNG; FImageUtils::PNGCompressImageArray(Target->SizeX,Target->SizeY,Pixels,PNG);
            FFileHelper::SaveArrayToFile(PNG,*(Directory/FString::Printf(TEXT("%s_%d.png"),Name,Height)));
        }
        return Pixels;
    };
    Render(1080,0.f,TEXT("CriticalStart"));
    Render(1080,.055f,TEXT("CriticalPop")); Render(720,.055f,TEXT("CriticalPop"));
    Render(1080,.32f,TEXT("Float")); Render(1080,.55f,TEXT("CritTail"));
    PC->GetPlayerHealthComponent()->Heal(15); HUD->UpdateDamageFeedback(.2f);
    Render(1080,.1f,TEXT("LowHealth"));
    HUD->LowHealthPhase=.5f;
    const auto Trough=Render(1080,.1f,TEXT("LowHealthTrough"));
    HUD->LowHealthPhase=0.f;
    const auto Peak=Render(1080,.1f,TEXT("LowHealthPeak"));
    Render(720,.1f,TEXT("LowHealthPeak"));
    if(Peak.Num()==1920*1080&&Trough.Num()==Peak.Num())
    {
        TestTrue(TEXT("Red screen edge visibly pulses"),Peak[540*1920+10].R>Trough[540*1920+10].R+8);
        TestEqual(TEXT("Vignette keeps the combat center clear"),Peak[650*1920+960],Trough[650*1920+960]);
    }
    PC->GetPlayerHealthComponent()->Heal(50); HUD->UpdateDamageFeedback(.25f);
    TestEqual(TEXT("Healed HUD warning is gone"), HUD->GetLowHealthFeedbackOpacity(),0.f);
    Render(1080,.62f,TEXT("Recovered"));
    HUD->ReleaseSlateResources(true); W->DestroyWorld(false); GEngine->DestroyWorldContext(W);
    return true;
}
#endif
