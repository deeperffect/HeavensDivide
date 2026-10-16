#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "ObjectiveBeaconComponent.h"
#include "ObjectiveInteractionComponent.h"
#include "SurvivalFieldObjective.h"
#include "BloodShrine.h"
#include "Components/StaticMeshComponent.h"
#include "Components/MaterialBillboardComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MaterialShared.h"
#include "SceneInterface.h"
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#include "RenderingThread.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

class FBeaconPreviewFrames final : public IAutomationLatentCommand
{
public:
    explicit FBeaconPreviewFrames(TFunction<bool()> InWork):Work(MoveTemp(InWork)){}
    virtual bool Update() override {return Work();}
private:
    TFunction<bool()> Work;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FObjectiveBeaconVisualTest,"HeavensDivide.UI.ObjectiveBeaconVisuals",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FObjectiveBeaconVisualTest::RunTest(const FString&)
{
    auto* W=UWorld::CreateWorld(EWorldType::Game,false);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
    W->SetGameInstance(NewObject<UGameInstance>(GEngine));W->InitializeActorsForPlay(FURL());
    auto* Circle=W->SpawnActor<ASurvivalFieldObjective>(FVector(-430,0,5),FRotator::ZeroRotator);
    Circle->SetActorTickEnabled(false);Circle->ObjectiveInteraction->bEnableSpawnEmergence=false;
    Circle->InitializeHoldZone();
    auto* ShrineClass=LoadClass<ABloodShrine>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/Objectives/BP_BloodShrine.BP_BloodShrine_C"));
    auto* Shrine=W->SpawnActor<ABloodShrine>(ShrineClass,FVector(430,0,5),FRotator(0,-90,0));
    Shrine->FindComponentByClass<UObjectiveInteractionComponent>()->bEnableSpawnEmergence=false;
    auto* Floor=W->SpawnActor<AStaticMeshActor>(FVector(0,0,0),FRotator::ZeroRotator);
    Floor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
    Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane")));
    Floor->SetActorScale3D(FVector(40));Floor->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    auto* Light=W->SpawnActor<ADirectionalLight>();Light->GetLightComponent()->SetMobility(EComponentMobility::Movable);Light->SetActorRotation(FRotator(-45,55,0));
    Cast<UDirectionalLightComponent>(Light->GetLightComponent())->SetIntensity(3.f);
    auto* Fill=W->SpawnActor<ADirectionalLight>();Fill->GetLightComponent()->SetMobility(EComponentMobility::Movable);Fill->SetActorRotation(FRotator(-25,-145,0));
    Cast<UDirectionalLightComponent>(Fill->GetLightComponent())->SetIntensity(.65f);
    auto* Camera=W->SpawnActor<AActor>();auto* Capture=NewObject<USceneCaptureComponent2D>(Camera);
    Camera->AddInstanceComponent(Capture);Capture->RegisterComponent();
    const FVector CameraPosition(0,-1700,1700);
    Capture->SetWorldLocation(CameraPosition);Capture->SetWorldRotation((FVector(0,0,220)-CameraPosition).Rotation());
    Capture->ProjectionType=ECameraProjectionMode::Orthographic;Capture->OrthoWidth=1850;
    Capture->CaptureSource=ESceneCaptureSource::SCS_FinalColorLDR;
    Capture->bCaptureEveryFrame=false;Capture->bCaptureOnMovement=false;Capture->bAlwaysPersistRenderingState=true;
    Capture->PostProcessSettings.bOverride_AutoExposureMethod=true;Capture->PostProcessSettings.AutoExposureMethod=EAutoExposureMethod::AEM_Manual;
    Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true;Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=false;
    auto* Target=NewObject<UTextureRenderTarget2D>(Camera);Target->RenderTargetFormat=RTF_RGBA8;
    Target->InitAutoFormat(1400,1000);Target->UpdateResourceImmediate();Capture->TextureTarget=Target;
    // A render-only world has no GameMode to dispatch BeginPlay to its actors.
    Circle->DispatchBeginPlay();Shrine->DispatchBeginPlay();W->BeginPlay();
    auto* BloodBeacon=Shrine->FindComponentByClass<UObjectiveBeaconComponent>();
    TestTrue(TEXT("Both saved objectives show their beacons"),Circle->Beacon->IsArrowVisible()&&BloodBeacon&&BloodBeacon->IsArrowVisible());
    for(AActor* Actor:{static_cast<AActor*>(Circle),static_cast<AActor*>(Shrine)})
    {
        auto* Arrow=Actor->FindComponentByClass<UMaterialBillboardComponent>();
        TestTrue(TEXT("Arrow is above the visible model, independent of its imported scale"),Arrow&&Arrow->GetComponentLocation().Z>Cast<UStaticMeshComponent>(Arrow->GetAttachParent())->Bounds.GetBox().Max.Z+100.);
        TArray<UPrimitiveComponent*> Primitives;Actor->GetComponents(Primitives);
        for(auto* Primitive:Primitives)if(Primitive->GetName().StartsWith(TEXT("ObjectiveBeacon")))
        {
            TestTrue(TEXT("Beacon visuals add no collision or shadow work"),!Primitive->IsCollisionEnabled()&&!Primitive->CastShadow);
            TestTrue(TEXT("Beacon is registered and visible in game"),Primitive->IsRegistered()&&Primitive->IsVisible()&&!Primitive->bHiddenInGame);
            AddInfo(FString::Printf(TEXT("%s: location %s, scale %s, material %s"),*Primitive->GetName(),*Primitive->GetComponentLocation().ToString(),*Primitive->GetComponentScale().ToString(),*GetNameSafe(Primitive->GetMaterial(0))));
            if(auto* Material=Primitive->GetMaterial(0))
            {
                FLinearColor Color;Material->GetVectorParameterValue(FMaterialParameterInfo(TEXT("Tint")),Color);
                AddInfo(TEXT("Beacon Tint ")+Color.ToString());
            }
        }
    }
    FAssetCompilingManager::Get().FinishAllCompilation();if(GShaderCompilingManager)GShaderCompilingManager->FinishAllCompilation();
    for(const TCHAR* Name:{TEXT("M_ObjectiveBeaconAura"),TEXT("M_ObjectiveBeaconArrow")})
    {
        auto* Mat=LoadObject<UMaterial>(nullptr,*(FString(TEXT("/Game/HeavensDivide/Materials/Objectives/"))+Name));
        auto* Resource=Mat?Mat->GetMaterialResource(W->Scene->GetShaderPlatform()):nullptr;
        TestTrue(TEXT("World beacon shader compiles"),Resource&&Resource->GetCompileErrors().IsEmpty()&&Resource->GetGameThreadShaderMap());
        if(Mat)for(const TCHAR* PropertyName:{TEXT("EmissiveColor"),TEXT("Opacity")})
        {
            auto* Data=Mat->GetEditorOnlyData();
            if(auto* Property=Data->GetClass()->FindPropertyByName(PropertyName))
            {
                FString Export;Property->ExportText_InContainer(0,Export,Data,Data,Data,PPF_None);
                AddInfo(FString(Name)+TEXT(" ")+PropertyName+TEXT(" ")+Export);
            }
        }
    }
    const FString Directory=FPaths::ProjectSavedDir()/TEXT("ObjectivePresentation/Previews");IFileManager::Get().MakeDirectory(*Directory,true);
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FBeaconPreviewFrames>(
        [this,W,Circle,BloodBeacon,Capture,Target,Directory,Started=FPlatformTime::Seconds(),Frame=0,Stage=0]() mutable
        {
            W->Tick(LEVELTICK_TimeOnly,.045f);W->SendAllEndOfFrameUpdates();FlushRenderingCommands();Capture->CaptureScene();FlushRenderingCommands();
            if(++Frame<10)return false;
            TArray<FColor> Pixels;FReadSurfaceDataFlags Flags;Flags.SetLinearToGamma(false);
            TestTrue(TEXT("Objective landmark preview renders"),Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags));
            int32 Red=0,Gold=0,RedArrow=0,GoldArrow=0;
            for(int32 Index=0;Index<Pixels.Num();++Index)
            {
                const auto& Pixel=Pixels[Index];
                if(Pixel.R>100&&Pixel.R>Pixel.G*1.5f&&Pixel.R>Pixel.B*1.5f){++Red;if(Index/1400<430)++RedArrow;}
                if(Pixel.R>100&&Pixel.G>70&&Pixel.G>Pixel.B*1.35f){++Gold;if(Index/1400<430)++GoldArrow;}
            }
            // First-use PSO preparation can outlive shader compilation on a cold cache.
            if((Red<=250||Gold<=250||(Stage==0&&(RedArrow<=250||GoldArrow<=250)))&&FPlatformTime::Seconds()-Started<30.)return false;
            TestTrue(TEXT("Crimson and gold effects are visible in the world render"),Red>250&&Gold>250);
            if(Stage<2)TestTrue(TEXT("Overhead arrows render only while objectives are available"),Stage==0?(RedArrow>250&&GoldArrow>250):(RedArrow<50&&GoldArrow<50));
            TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(1400,1000,Pixels,PNG);
            const TCHAR* Names[]={TEXT("ObjectivesAvailable.png"),TEXT("ObjectivesActive.png"),TEXT("ObjectivesPerspectiveLeft.png"),TEXT("ObjectivesPerspectiveRight.png")};
            FFileHelper::SaveArrayToFile(PNG,*(Directory/Names[Stage]));
            if(Stage==0)
            {
                Circle->Beacon->SetBeaconState(true,false);BloodBeacon->SetBeaconState(true,false);
            }
            else if(Stage<3)
            {
                Circle->Beacon->SetBeaconState(true,true);BloodBeacon->SetBeaconState(true,true);
                Capture->ProjectionType=ECameraProjectionMode::Perspective;Capture->FOVAngle=55;
                const FVector ViewPosition(Stage==1?900:-900,-1500,1350);
                Capture->SetWorldLocation(ViewPosition);Capture->SetWorldRotation((FVector(0,0,220)-ViewPosition).Rotation());
            }
            if(++Stage<4){Frame=0;return false;}
            W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
        }));
    return true;
}
#endif
