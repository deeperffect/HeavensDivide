#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "BloodShrineWidget.h"
#include "SurvivorPlayerController.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/StaticMeshActor.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MaterialShared.h"
#include "SceneInterface.h"
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#include "ImageUtils.h"
#include "RenderingThread.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

class FObjectiveProgressPreviewFrames final : public IAutomationLatentCommand
{
public:
    explicit FObjectiveProgressPreviewFrames(TFunction<bool()> InWork):Work(MoveTemp(InWork)){}
    virtual bool Update() override { return Work(); }
private:
    TFunction<bool()> Work;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectiveProgressVisualTest,"HeavensDivide.UI.ObjectiveProgressVisuals",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FObjectiveProgressVisualTest::RunTest(const FString&)
{
    auto* World=UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->SetGameInstance(NewObject<UGameInstance>(GEngine));World->InitializeActorsForPlay(FURL());
    const FString Directory=FPaths::ProjectSavedDir()/TEXT("ObjectiveProgress/Previews");
    IFileManager::Get().MakeDirectory(*Directory,true);
    auto Save=[this,Directory](UTextureRenderTarget2D* Target,const TCHAR* Name)
    {
        TArray<FColor> Pixels;FReadSurfaceDataFlags Flags;Flags.SetLinearToGamma(false);
        if(!TestTrue(TEXT("Objective preview is readable"),Target&&Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags)))return;
        TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(Target->SizeX,Target->SizeY,Pixels,PNG);
        TestTrue(TEXT("Objective preview saved"),FFileHelper::SaveArrayToFile(PNG,*(Directory/Name)));
    };
    auto* PC=World->SpawnActor<ASurvivorPlayerController>();PC->SetPlayer(NewObject<ULocalPlayer>(GEngine));
    auto* HUD=CreateWidget<UBloodShrineWidget>(PC);const auto Slate=HUD->TakeWidget();HUD->ShowChallenge(7,30,23.f);
    FWidgetRenderer Renderer(false);auto* HUDTarget=Renderer.DrawWidget(Slate,{1440,900});
    for(int32 Pass=0;Pass<3;++Pass)
    {
        ++GFrameCounter;HUD->ForceLayoutPrepass();Renderer.DrawWidget(HUDTarget,Slate,{1440,900},.016f);FlushRenderingCommands();
    }
    Save(HUDTarget,TEXT("BloodShrineCounter.png"));
    World->BeginPlay();
    auto* Material=LoadObject<UMaterial>(nullptr,TEXT("/Game/HeavensDivide/Materials/M_EmberCircleProgress.M_EmberCircleProgress"));
    if(!TestNotNull(TEXT("Saved circle progress material exists"),Material))
    {
        World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return false;
    }
    for(int32 Stage=0;Stage<5;++Stage)
    {
        auto* Plane=World->SpawnActor<AStaticMeshActor>(FVector((Stage-2)*520,0,5),FRotator::ZeroRotator);
        auto* Mesh=Plane->GetStaticMeshComponent();Mesh->SetMobility(EComponentMobility::Movable);
        Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane")));
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->SetCastShadow(false);Plane->SetActorScale3D(FVector(4.8f));
        Mesh->SetMaterial(0,Material);Mesh->CreateDynamicMaterialInstance(0)->SetScalarParameterValue(TEXT("Progress"),Stage*.25f);
    }
    auto* Camera=World->SpawnActor<AActor>();auto* Capture=NewObject<USceneCaptureComponent2D>(Camera);
    Camera->AddInstanceComponent(Capture);Capture->RegisterComponent();
    Capture->SetWorldLocation(FVector(0,0,3000));Capture->SetWorldRotation(FRotator(-90,-90,0));
    Capture->ProjectionType=ECameraProjectionMode::Orthographic;Capture->OrthoWidth=2800;
    Capture->CaptureSource=ESceneCaptureSource::SCS_FinalColorLDR;
    Capture->bCaptureEveryFrame=false;Capture->bCaptureOnMovement=false;Capture->bAlwaysPersistRenderingState=true;
    Capture->PostProcessSettings.bOverride_AutoExposureMethod=true;Capture->PostProcessSettings.AutoExposureMethod=EAutoExposureMethod::AEM_Manual;
    Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true;Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=false;
    auto* Target=NewObject<UTextureRenderTarget2D>(Camera);Target->RenderTargetFormat=RTF_RGBA8;
    Target->ClearColor=FLinearColor::Black;Target->InitAutoFormat(1400,500);Target->UpdateResourceImmediate();Capture->TextureTarget=Target;
    FAssetCompilingManager::Get().FinishAllCompilation();if(GShaderCompilingManager)GShaderCompilingManager->FinishAllCompilation();
    auto* Resource=Material->GetMaterialResource(World->Scene->GetShaderPlatform());
    TestTrue(TEXT("Circle progress shader compiles"),Resource&&Resource->GetCompileErrors().IsEmpty()&&Resource->GetGameThreadShaderMap());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FObjectiveProgressPreviewFrames>(
        [this,World,Capture,Target,Save,Started=FPlatformTime::Seconds(),Frames=0]() mutable
        {
            World->SendAllEndOfFrameUpdates();FlushRenderingCommands();Capture->CaptureScene();FlushRenderingCommands();
            if(++Frames<8)return false;
            TArray<FColor> Pixels;Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
            int32 Blue[5]={},Yellow[5]={};
            for(int32 Index=0;Index<Pixels.Num();++Index)
            {
                const auto& Pixel=Pixels[Index];
                const int32 Stage=FMath::Clamp(FMath::RoundToInt(((Index%1400)-180.f)/260.f),0,4);
                if(Pixel.B>80&&Pixel.B>Pixel.R*1.5f)++Blue[Stage];
                if(Pixel.R>80&&Pixel.G>60&&Pixel.R>Pixel.B*1.5f)++Yellow[Stage];
            }
            if((Blue[0]<200||Yellow[4]<200)&&FPlatformTime::Seconds()-Started<40)return false;
            for(int32 Stage=0;Stage<5;++Stage)
            {
                const int32 Total=Blue[Stage]+Yellow[Stage];
                TestTrue(TEXT("Every ring progress stage is visible"),Total>200);
                TestTrue(FString::Printf(TEXT("Yellow coverage matches %d percent progress"),Stage*25),
                    FMath::IsNearlyEqual(float(Yellow[Stage])/FMath::Max(1,Total),Stage*.25f,.08f));
            }
            Save(Target,TEXT("CircleProgress.png"));
            World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return true;
        }));
    return true;
}
#endif
