#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/Material.h"
#include "MaterialShared.h"
#include "SceneInterface.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraSystemInstanceController.h"
#include "NiagaraEmitterInstance.h"
#include "SamuraiCharacter.h"
#include "SwapPresentationComponent.h"
#include "RenderingThread.h"
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSwapInkSlashTest, "HeavensDivide.ImpactFeedback.SwapInkSlash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FSwapInkSlashTest::RunTest(const FString&)
{
	auto* Material = LoadObject<UMaterial>(nullptr, TEXT("/Game/HeavensDivide/VFX/Swap/M_SwapInkSlash"));
	auto* System = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/HeavensDivide/VFX/Swap/NS_SwapInkSlash"));
	if (!TestNotNull(TEXT("Swap material"), Material) || !TestNotNull(TEXT("Swap system"), System)) return false;
	UTexture* Shape = nullptr;
	TestTrue(TEXT("Opacity shape texture is assigned"), Material->GetTextureParameterValue(FMaterialParameterInfo(TEXT("Shape")), Shape) && Shape);
	TestEqual(TEXT("Sprite background remains transparent"), Material->GetBlendMode(), BLEND_Translucent);
	auto* Class = LoadClass<ASamuraiCharacter>(nullptr, TEXT("/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_Samurai.BP_Samurai_C"));
	if (TestNotNull(TEXT("Saved Samurai"), Class))
		TestEqual(TEXT("Ninja-to-Samurai swap uses the repaired effect"), Class->GetDefaultObject<ASamuraiCharacter>()->SwapPresentation->ArrivalVFX.Get(), System);
	System->WaitForCompilationComplete(true, false);
	FAssetCompilingManager::Get().FinishAllCompilation();
	if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
	auto* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL()); World->BeginPlay();
	ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
	auto* Resource = Material->GetMaterialResource(World->Scene->GetShaderPlatform());
	TestTrue(TEXT("Swap material compiles without the opaque fallback"), Resource && Resource->GetCompileErrors().IsEmpty() && Resource->GetGameThreadShaderMap());
	auto* Camera = World->SpawnActor<AActor>();
	auto* Capture = NewObject<USceneCaptureComponent2D>(Camera);
	Camera->AddInstanceComponent(Capture); Capture->RegisterComponent();
	Capture->SetWorldLocation(FVector(0, -600, 400));
	Capture->SetWorldRotation((FVector(0, 0, 65) - Capture->GetComponentLocation()).Rotation());
	Capture->ProjectionType = ECameraProjectionMode::Orthographic;
	Capture->OrthoWidth = 500;
	Capture->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDRNoAlpha;
	Capture->bAlwaysPersistRenderingState = true;
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	Capture->ShowFlags.SetLighting(false);
	auto* Target = NewObject<UTextureRenderTarget2D>(Camera);
	Target->RenderTargetFormat = RTF_RGBA8;
	Target->InitAutoFormat(512, 512); Target->UpdateResourceImmediate(); Capture->TextureTarget = Target;
	auto* FX = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, System, FVector::ZeroVector,
		FRotator::ZeroRotator, FVector::OneVector, false, false, ENCPoolMethod::None, false);
	if (!TestNotNull(TEXT("Swap burst spawns"), FX)) return false;
	FX->SetVariableLinearColor(TEXT("User.SwapColor"), FLinearColor(1, .12f, .08f, 1));
	FX->SetForceSolo(true); FX->Activate(true);
	for (int32 Phase = 0; Phase < 3; ++Phase)
	{
		FX->AdvanceSimulation(3, .02f);
		if (auto* Controller = FX->GetSystemInstanceController().Get())
			if (auto* Instance = Controller->GetSystemInstance_Unsafe())
				for (const auto& Emitter : Instance->GetEmitters())
					AddInfo(FString::Printf(TEXT("Phase %d age %.3f particles %d"), Phase, Instance->GetAge(), Emitter->GetNumParticles()));
		FX->UpdateBounds(); FX->MarkRenderStateDirty();
		for (int32 Frame = 0; Frame < 4; ++Frame)
		{
			++GFrameCounter; World->Tick(LEVELTICK_All, .001f);
			FX->MarkRenderDynamicDataDirty(); World->SendAllEndOfFrameUpdates();
			FlushRenderingCommands(); Capture->CaptureScene(); FlushRenderingCommands();
		}
		UKismetRenderingLibrary::ExportRenderTarget(World, Target,
			FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("PickupAndSwapFix/Previews")),
			FString::Printf(TEXT("SwapSlash_%d.png"), Phase));
	}
	FX->AdvanceSimulation(120, .02f);
	TestTrue(TEXT("The burst terminates"), FX->IsComplete());
	FX->DestroyComponent();
	return true;
}
#endif
