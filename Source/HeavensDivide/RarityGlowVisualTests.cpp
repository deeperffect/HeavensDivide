#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "LevelUpWidget.h"
#include "SurvivorPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/OverlaySlot.h"
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
#include "GameFramework/WorldSettings.h"
#include "Slate/WidgetRenderer.h"
#include "UObject/UnrealType.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "PipelineStateCache.h"

class FRarityPreviewFrames final : public IAutomationLatentCommand
{
public:
    explicit FRarityPreviewFrames(TFunction<bool()> InWork):Work(MoveTemp(InWork)){}
    virtual bool Update() override {return Work();}
private:
    TFunction<bool()> Work;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRarityGlowVisualTest,"HeavensDivide.UI.RarityGlowVisuals",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FRarityGlowVisualTest::RunTest(const FString&)
{
    auto* GI=NewObject<UGameInstance>(GEngine);GI->InitializeStandalone();auto* W=GI->GetWorld();
    auto* PC=W->SpawnActor<ASurvivorPlayerController>(LoadClass<ASurvivorPlayerController>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController.BP_SurvivorPlayerController_C")));
    PC->SetPlayer(NewObject<ULocalPlayer>(GEngine));auto* Upgrades=PC->GetPlayerUpgrades();Upgrades->BeginDirectUpgradeSelection();
    auto* Card=LoadObject<UUpgradeDefinition>(nullptr,TEXT("/Game/HeavensDivide/Upgrades/Global/DA_Upgrade_GlobalDamage"));
    TArray<TObjectPtr<UUpgradeDefinition>> Choices{Card,Card,Card};
    for(const TCHAR* Name:{TEXT("CurrentUpgradeChoices"),TEXT("CurrentPresentationChoices")})
    {
        auto* Property=FindFProperty<FArrayProperty>(Upgrades->GetClass(),Name);
        Property->CopyCompleteValue(Property->ContainerPtrToValuePtr<void>(Upgrades),&Choices);
    }
    TArray<FUpgradeOffer> Offers;
    for(EUpgradeRarity Rarity:{EUpgradeRarity::Rare,EUpgradeRarity::Epic,EUpgradeRarity::Legendary})
    {
        FUpgradeOffer Offer;Offer.UpgradeDefinition=Card;Offer.RolledRarity=Rarity;Offer.bDisplaysRarity=true;
        Offer.ResolvedDescription=Card->GetBalanceDescription();Offers.Add(Offer);
    }
    auto* Property=FindFProperty<FArrayProperty>(Upgrades->GetClass(),TEXT("CurrentUpgradeOffers"));
    Property->CopyCompleteValue(Property->ContainerPtrToValuePtr<void>(Upgrades),&Offers);
    auto* Widget=CreateWidget<ULevelUpWidget>(GI,LoadClass<ULevelUpWidget>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/UI/UpgradeUI/WBP_LevelUp.WBP_LevelUp_C")));
    const auto Slate=Widget->TakeWidget();Widget->InitializeDirectUpgradeWidget(PC);
    auto* Base=LoadObject<UMaterial>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/UI/UpgradeUI/Materials/M_UI_UpgradeRarityGlow"));
    FAssetCompilingManager::Get().FinishAllCompilation();if(GShaderCompilingManager)GShaderCompilingManager->FinishAllCompilation();
    const auto* Resource=Base->GetMaterialResource(W->Scene->GetShaderPlatform());
    TestTrue(TEXT("Rarity material compiles without missing textures"),Resource&&Resource->GetCompileErrors().IsEmpty()&&Resource->GetGameThreadShaderMap());
    float PreviousIntensity=0,PreviousWidth=0;
    for(int32 I=0;I<3;++I)
    {
        auto* Image=Widget->UpgradeRarityGlowImages[I].Get();auto* MID=Image->GetDynamicMaterial();
        TestTrue(TEXT("Every rolled rarity has a visible material"),Image->IsVisible()&&MID);
        if(!MID)continue;
        const float Intensity=MID->K2_GetScalarParameterValue(TEXT("Intensity")),Width=MID->K2_GetScalarParameterValue(TEXT("HaloWidth"));
        TestTrue(TEXT("Higher rarities gain intensity and coverage"),Intensity>PreviousIntensity&&Width>PreviousWidth);
        PreviousIntensity=Intensity;PreviousWidth=Width;
        TestTrue(TEXT("Halo extends beyond every side of the frame"),Cast<UOverlaySlot>(Image->Slot)->GetPadding().Left<0);
        Cast<UTextBlock>(Widget->WidgetTree->FindWidget(*FString::Printf(TEXT("UpgradeCardTitle_%d"),I)))->SetText(
            FText::FromString(I==0?TEXT("Rare"):I==1?TEXT("Epic"):TEXT("Legendary")));
    }
    W->GetWorldSettings()->SetTimeDilation(.0001f);
    const float Before=Widget->UpgradeRarityGlowImages[0]->GetDynamicMaterial()->K2_GetScalarParameterValue(TEXT("AnimationTime"));
    Widget->UpdateRarityAnimation(.4f);
    TestTrue(TEXT("Glow animation advances independently of the frozen gameplay clock"),
        FMath::IsNearlyEqual(Widget->UpgradeRarityGlowImages[0]->GetDynamicMaterial()->K2_GetScalarParameterValue(TEXT("AnimationTime"))-Before,.4f));
    const FString Directory=FPaths::ProjectSavedDir()/TEXT("ObjectivePresentation/Previews");IFileManager::Get().MakeDirectory(*Directory,true);
    auto Renderer=MakeShared<FWidgetRenderer>(false);
    auto* Target=Renderer->DrawWidget(Slate,{1920,1080});Target->AddToRoot();Widget->AddToRoot();
    // Material uniform caches update in the main Slate renderer between engine
    // frames; a synchronous DrawWidget loop cannot stand in for those frames.
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FRarityPreviewFrames>(
        [this,GI,W,Widget,Slate,Renderer,Target,Directory,First=TArray<FColor>(),Started=FPlatformTime::Seconds(),Frame=0,Stage=0]() mutable
        {
            if(Frame==0)Widget->UpdateRarityAnimation(.55f);
            Widget->ForceLayoutPrepass();Renderer->DrawWidget(Target,Slate,{1920,1080},.016f);
            FlushRenderingCommands();PipelineStateCache::WaitForAllTasks();
            if(++Frame<10)return false;
            TArray<FColor> Pixels;FReadSurfaceDataFlags Flags;Flags.SetLinearToGamma(false);
            if(TestTrue(TEXT("Rarity preview renders"),Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags)))
            {
                int32 Colored=0;
                for(const auto& Pixel:Pixels)if(Pixel.B>80&&Pixel.B>Pixel.G*1.5f)++Colored;
                if(Colored<1500&&FPlatformTime::Seconds()-Started<30)return false;
                TestTrue(TEXT("Blue and violet halos are visible around the cards"),Colored>1500);
                if(Stage==0)First=Pixels;
                else
                {
                    int32 Changed=0;for(int32 I=0;I<Pixels.Num();++I)if(FMath::Abs(int32(Pixels[I].R)-First[I].R)+FMath::Abs(int32(Pixels[I].G)-First[I].G)+FMath::Abs(int32(Pixels[I].B)-First[I].B)>18)++Changed;
                    TestTrue(TEXT("The rendered glow moves between frames"),Changed>1500);
                }
                TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(1920,1080,Pixels,PNG);
                FFileHelper::SaveArrayToFile(PNG,*(Directory/FString::Printf(TEXT("CardRarities_%d.png"),Stage)));
            }
            if(++Stage<3){Frame=0;return false;}
            Target->RemoveFromRoot();Widget->ReleaseSlateResources(true);Widget->RemoveFromRoot();
            W->DestroyWorld(false);GEngine->DestroyWorldContext(W);GI->Shutdown();return true;
        }));
    return true;
}
#endif
