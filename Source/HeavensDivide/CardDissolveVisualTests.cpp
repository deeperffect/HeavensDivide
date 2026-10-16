#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "LevelUpWidget.h"
#include "SurvivorPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MaterialShared.h"
#include "SceneInterface.h"
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Slate/WidgetRenderer.h"
#include "UObject/UnrealType.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "PipelineStateCache.h"

class FCardDissolvePreviewFrames final : public IAutomationLatentCommand
{
public:
    explicit FCardDissolvePreviewFrames(TFunction<bool()> InWork):Work(MoveTemp(InWork)){}
    virtual bool Update() override {return Work();}
private:
    TFunction<bool()> Work;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCardDissolveVisualTest,"HeavensDivide.UI.CardDissolveVisuals",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FCardDissolveVisualTest::RunTest(const FString&)
{
    auto* GI=NewObject<UGameInstance>(GEngine);GI->InitializeStandalone();auto* W=GI->GetWorld();
    auto* PC=W->SpawnActor<ASurvivorPlayerController>();PC->SetPlayer(NewObject<ULocalPlayer>(GEngine));
    auto* Upgrades=PC->GetPlayerUpgrades();Upgrades->BeginDirectUpgradeSelection();
    auto* Card=LoadObject<UUpgradeDefinition>(nullptr,TEXT("/Game/HeavensDivide/Upgrades/Global/DA_Upgrade_GlobalDamage"));
    TArray<TObjectPtr<UUpgradeDefinition>> Choices{Card,Card,Card};
    for(const TCHAR* Name:{TEXT("CurrentUpgradeChoices"),TEXT("CurrentPresentationChoices")})
    {
        auto* Property=FindFProperty<FArrayProperty>(Upgrades->GetClass(),Name);
        Property->CopyCompleteValue(Property->ContainerPtrToValuePtr<void>(Upgrades),&Choices);
    }
    auto* Widget=CreateWidget<ULevelUpWidget>(GI,LoadClass<ULevelUpWidget>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/UI/UpgradeUI/WBP_LevelUp.WBP_LevelUp_C")));
    const auto Slate=Widget->TakeWidget();Widget->InitializeDirectUpgradeWidget(PC);
    for(int32 I=0;I<3;++I)Widget->UpgradeTitleTexts[I]->SetText(FText::FromString(I==0?TEXT("Selected - Gold"):I==1?TEXT("Banished - Burn"):TEXT("Unaffected")));
    // Compare both variants with the same card art in one rendered frame.
    Widget->BeginChoiceExit(0);auto* Gold=Widget->ChoiceExitMaterial.Get();
    Widget->BeginChoiceExit(1,true);auto* Burn=Widget->ChoiceExitMaterial.Get();
    Widget->bChoiceExitActive=false; // Freeze samples at exact shader progress below.
    auto* Base=LoadObject<UMaterial>(nullptr,TEXT("/Game/HeavensDivide/Materials/UI/M_UpgradeCardSelectionDissolve"));
    FAssetCompilingManager::Get().FinishAllCompilation();if(GShaderCompilingManager)GShaderCompilingManager->FinishAllCompilation();
    const auto* Resource=Base->GetMaterialResource(W->Scene->GetShaderPlatform());
    TestTrue(TEXT("Card effect compiles"),Resource&&Resource->GetCompileErrors().IsEmpty()&&Resource->GetGameThreadShaderMap());
    TestNotNull(TEXT("Gold retainer material"),Gold);TestNotNull(TEXT("Burn retainer material"),Burn);
    const FString Directory=FPaths::ProjectSavedDir()/TEXT("CardDissolve/Previews");IFileManager::Get().MakeDirectory(*Directory,true);
    auto Renderer=MakeShared<FWidgetRenderer>(false);auto* Target=Renderer->DrawWidget(Slate,{1920,1080});
    Target->AddToRoot();Widget->AddToRoot();
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCardDissolvePreviewFrames>(
        [this,GI,W,Widget,Slate,Gold,Burn,Renderer,Target,Directory,First=TArray<FColor>(),Started=FPlatformTime::Seconds(),Frame=0,Stage=0]() mutable
        {
            const float Progress=Stage==0?0.f:Stage==1?.45f:Stage==2?.75f:1.f;
            if(Gold)Gold->SetScalarParameterValue(TEXT("Progress"),Progress);
            if(Burn)Burn->SetScalarParameterValue(TEXT("Progress"),Progress);
            Widget->ForceLayoutPrepass();Renderer->DrawWidget(Target,Slate,{1920,1080},0.f);
            FlushRenderingCommands();PipelineStateCache::WaitForAllTasks();
            // Retainer capture and material uniforms need actual Slate frames.
            if(++Frame<12)return false;
            TArray<FColor> Pixels;FReadSurfaceDataFlags Flags;Flags.SetLinearToGamma(false);
            if(TestTrue(TEXT("Card effect preview renders"),Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags)))
            {
                if(Stage==0)
                {
                    // On a cold start the effect PSO can finish after the card
                    // texture and test shader resource are ready. Require an
                    // actual intact card before capturing the baseline.
                    int32 Visible=0;
                    for(int32 Y=230;Y<820;++Y)for(int32 X=260;X<650;++X)
                    {
                        const auto& Pixel=Pixels[Y*1920+X];
                        if(FMath::Max3(Pixel.R,Pixel.G,Pixel.B)>40)++Visible;
                    }
                    if(Visible<10000&&FPlatformTime::Seconds()-Started<30)return false;
                    TestTrue(TEXT("Gold card starts fully visible"),Visible>10000);
                    First=Pixels;
                }
                else
                {
                    int32 Changed=0;
                    for(int32 I=0;I<Pixels.Num();++I)
                        if(FMath::Abs(int32(Pixels[I].R)-First[I].R)+FMath::Abs(int32(Pixels[I].G)-First[I].G)+FMath::Abs(int32(Pixels[I].B)-First[I].B)>40)++Changed;
                    TestTrue(TEXT("Dissolve visibly changes rendered cards"),Changed>5000);
                }
                TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(1920,1080,Pixels,PNG);
                FFileHelper::SaveArrayToFile(PNG,*(Directory/FString::Printf(TEXT("CardDissolve_%d.png"),Stage)));
            }
            if(++Stage<4){Frame=0;return false;}
            Target->RemoveFromRoot();Widget->ReleaseSlateResources(true);Widget->RemoveFromRoot();
            W->DestroyWorld(false);GEngine->DestroyWorldContext(W);GI->Shutdown();return true;
        }));
    return true;
}
#endif
