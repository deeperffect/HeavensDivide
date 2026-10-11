#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "ChestRewardWidget.h"
#include "EncounterHazard.h"
#include "SurvivorPlayerController.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "MaterialShared.h"
#include "SceneInterface.h"
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#include "ImageUtils.h"
#include "RenderingThread.h"
#include "TextureResource.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

class FCombatPreviewFrames final : public IAutomationLatentCommand
{
public:
    explicit FCombatPreviewFrames(TFunction<bool()> InWork):Work(MoveTemp(InWork)){}
    virtual bool Update() override { return Work(); }
private:
    TFunction<bool()> Work;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatPolishVisualTest,"HeavensDivide.UI.CombatPolishVisuals",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FCombatPolishVisualTest::RunTest(const FString&)
{
    auto* W=UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
    W->SetGameInstance(NewObject<UGameInstance>(GEngine));W->InitializeActorsForPlay(FURL());
    auto* PC=W->SpawnActor<ASurvivorPlayerController>();PC->SetPlayer(NewObject<ULocalPlayer>(GEngine));
    const FString Directory=FPaths::ProjectSavedDir()/TEXT("CombatPolish/Previews");IFileManager::Get().MakeDirectory(*Directory,true);
    auto Save=[this,Directory](UTextureRenderTarget2D* Target,const FString& Name)
    {
        TArray<FColor> Pixels;FReadSurfaceDataFlags Flags;Flags.SetLinearToGamma(false);
        if(!TestTrue(TEXT("Rendered preview has readable pixels"),Target&&Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags)))return;
        TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(Target->SizeX,Target->SizeY,Pixels,PNG);
        TestTrue(TEXT("Preview saved"),FFileHelper::SaveArrayToFile(PNG,*(Directory/Name)));
    };
    auto* Screen=CreateWidget<UChestRewardWidget>(PC);FWidgetRenderer Renderer(false);
    for(int32 Stage=0;Stage<3;++Stage)
    {
        Screen->Present(nullptr,123,Stage==0?.3f:Stage==1?1.5f:3.f,3.6f);
        auto Slate=Screen->TakeWidget();auto* Target=Renderer.DrawWidget(Slate,{1280,800});
        for(int32 I=0;I<3;++I){Screen->ForceLayoutPrepass();Renderer.DrawWidget(Target,Slate,{1280,800},.016f);}
        Save(Target,FString::Printf(TEXT("ChestStage%d.png"),Stage));
    }
    W->BeginPlay();
    auto* Owner=W->SpawnActor<AActor>();
    auto* Camera=W->SpawnActor<AActor>();auto* Capture=NewObject<USceneCaptureComponent2D>(Camera);Camera->AddInstanceComponent(Capture);Capture->RegisterComponent();
    Capture->SetWorldLocation(FVector(0,-1700,2300));Capture->SetWorldRotation((-Capture->GetComponentLocation()).Rotation());
    Capture->ProjectionType=ECameraProjectionMode::Perspective;Capture->FOVAngle=40;
    Capture->CaptureSource=ESceneCaptureSource::SCS_FinalColorLDR;Capture->bAlwaysPersistRenderingState=true;Capture->bCaptureEveryFrame=false;Capture->bCaptureOnMovement=false;
    Capture->PostProcessSettings.bOverride_AutoExposureMethod=true;Capture->PostProcessSettings.AutoExposureMethod=EAutoExposureMethod::AEM_Manual;
    Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true;Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=false;
    Capture->PostProcessSettings.bOverride_AutoExposureBias=true;Capture->PostProcessSettings.AutoExposureBias=0;
    auto* Target=NewObject<UTextureRenderTarget2D>(Camera);Target->RenderTargetFormat=RTF_RGBA8;Target->ClearColor=FLinearColor(.02f,.03f,.04f);Target->InitAutoFormat(1200,800);Target->UpdateResourceImmediate();Capture->TextureTarget=Target;
    auto* Floor=W->SpawnActor<AStaticMeshActor>(FVector(0,0,-10),FRotator::ZeroRotator);
    Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane")));
    Floor->GetStaticMeshComponent()->SetReceivesDecals(false);Floor->SetActorScale3D(FVector(24));
    TArray<AEncounterHazard*> Hazards;
    for(int32 Row=0;Row<2;++Row)for(int32 Column=0;Column<3;++Column)
    {
        const EEncounterShape Shape=Column==0?EEncounterShape::Circle:Column==1?EEncounterShape::Ring:EEncounterShape::Lane;
        auto* H=AEncounterHazard::Spawn(Owner,FVector((Column-1)*540,(Row-.5f)*530,0),Shape,Column==2?390:170,Column==0?0:Column==1?85:100,1,10,.3f,Column==2?25:0);
        if(!TestNotNull(TEXT("Visual hazard spawns"),H))continue;
        H->Tick(Row==0?.55f:1.05f);Hazards.Add(H);
        TestTrue(TEXT("Surface warns even when ground rejects decals"),H->HasVisibleWarning());
    }
    FAssetCompilingManager::Get().FinishAllCompilation();if(GShaderCompilingManager)GShaderCompilingManager->FinishAllCompilation();
    for(const TCHAR* Name:{TEXT("M_AttackIndicatorCircle"),TEXT("M_AttackIndicatorRectangle"),TEXT("M_EncounterEnergy")})
    {
        auto* M=LoadObject<UMaterial>(nullptr,*(FString(TEXT("/Game/HeavensDivide/Materials/"))+Name));
        auto* R=M?M->GetMaterialResource(W->Scene->GetShaderPlatform()):nullptr;
        TestTrue(TEXT("Encounter shader compiles"),R&&R->GetCompileErrors().IsEmpty()&&R->GetGameThreadShaderMap());
        if(R)for(const auto& Error:R->GetCompileErrors())AddError(Error);
    }
    TestTrue(TEXT("Capture includes translucency"),Capture->ShowFlags.Translucency);
    for(auto* H:Hazards){H->GroundSurface->MarkRenderStateDirty();H->ImpactColumn->MarkRenderStateDirty();}
    W->SendAllEndOfFrameUpdates();FlushRenderingCommands();
    Screen->RemoveFromParent();
    // Yield real engine frames so async shader/PSO and material render-state
    // updates complete; manually incrementing GFrameCounter cannot do that.
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCombatPreviewFrames>(
        [this,W,Capture,Target,Save,Started=FPlatformTime::Seconds(),Frames=0]() mutable
        {
            if(FPlatformTime::Seconds()-Started<1.5)return false;
            W->Tick(LEVELTICK_All,.00001f);W->SendAllEndOfFrameUpdates();FlushRenderingCommands();
            Capture->CaptureScene();FlushRenderingCommands();
            if(++Frames<8)return false;
            TArray<FColor> Pixels;Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
            int32 Colored=0,WarningColored[3]={0,0,0};
            for(int32 Index=0;Index<Pixels.Num();++Index)
            {
                const FColor& Pixel=Pixels[Index];
                if(Pixel.R>Pixel.G*1.3f&&Pixel.R>80)
                {
                    ++Colored;
                    if(Index/Target->SizeX>=Target->SizeY/2)++WarningColored[(Index%Target->SizeX)/(Target->SizeX/3)];
                }
            }
            // Newly compiled materials can have ready shader maps while their
            // async PSOs still defer drawing. Wait for every warning footprint.
            const bool bWarningsVisible=WarningColored[0]>100 && WarningColored[1]>100 && WarningColored[2]>100;
            if(!bWarningsVisible && FPlatformTime::Seconds()-Started<45.0)return false;
            TestTrue(TEXT("Attack colors are visible in the rendered scene"),Colored>500);
            TestTrue(TEXT("Circle, ring and lane warnings all render before impact"),bWarningsVisible);
            Save(Target,TEXT("WarningsAndImpacts.png"));
            W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
        }));
    return true;
}
#endif
