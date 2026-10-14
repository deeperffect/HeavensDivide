#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "PlayerDeathScene.h"
#include "PlayerDeathWidget.h"
#include "GameOverWidget.h"
#include "SurvivorPlayerController.h"
#include "CharacterManagerComponent.h"
#include "CharacterBase.h"
#include "SwapAfterimage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Camera/PlayerCameraManager.h"
#include "SceneView.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/Material.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"
#include "Slate/WidgetRenderer.h"
#include "AssetCompilingManager.h"
#include "RenderingThread.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerDeathPresentationTest,"HeavensDivide.UI.PlayerDeathPresentation",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPlayerDeathPresentationTest::RunTest(const FString&)
{
 auto* W=UWorld::CreateWorld(EWorldType::Game,false);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 W->SetGameInstance(NewObject<UGameInstance>(GEngine));W->SetGameMode(FURL());W->InitializeActorsForPlay(FURL());
 // InitializeActorsForPlay resets the clock; seed a rendered camera frame afterwards.
 W->TimeSeconds=1.;
 auto* ControllerClass=LoadClass<ASurvivorPlayerController>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController.BP_SurvivorPlayerController_C"));
 auto* PC=W->SpawnActor<ASurvivorPlayerController>(ControllerClass);
 PC->SetPlayer(NewObject<ULocalPlayer>(GEngine));PC->PlayerState=W->SpawnActor<APlayerState>();
 PC->bIsPlayerDead=true; PC->RunEndState=ERunEndState::Defeat;
 PC->GameOverWidgetClass=LoadClass<UGameOverWidget>(nullptr,TEXT("/Game/HeavensDivide/Blueprints/UI/WBP_GameOver.WBP_GameOver_C"));
 const FString Output=FPaths::ProjectSavedDir()/TEXT("DeathTransitions/Previews"); IFileManager::Get().MakeDirectory(*Output,true);
 for (const FString Name : {TEXT("Samurai"),TEXT("Ninja")}) {
  auto* C=LoadClass<ACharacterBase>(nullptr,*(TEXT("/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_")+Name+TEXT(".BP_")+Name+TEXT("_C")));
  auto* Hero=W->SpawnActor<ACharacterBase>(C,FVector(0,0,88),FRotator::ZeroRotator);
  PC->Possess(Hero);
  Hero->SetVisualFacingRotation(FRotator(0, Name==TEXT("Samurai")?35.f:-80.f, 0));
  FMinimalViewInfo View;
  View.Location = Hero->GetActorLocation() + FVector(580,-250,850);
  View.Rotation = (Hero->GetActorLocation()+FVector(120,50,0)-View.Location).Rotation();
  View.FOV=55.f; View.AspectRatio=1280.f/720.f; View.OrthoWidth=1500;
  View.ProjectionMode=Name==TEXT("Samurai")?ECameraProjectionMode::Perspective:ECameraProjectionMode::Orthographic;
  View.bAutoCalculateOrthoPlanes=false; View.OrthoNearClipPlane=0; View.OrthoFarClipPlane=10000;
  PC->PlayerCameraManager->FillCameraCache(View);
  FindFProperty<FObjectProperty>(UCharacterManagerComponent::StaticClass(),TEXT("ActiveCharacter"))->SetObjectPropertyValue_InContainer(PC->GetCharacterManager(),Hero);
  auto* OriginalViewTarget=PC->GetViewTarget();
  auto* OriginalMaterial=Hero->GetMesh()->GetMaterial(0);const FTransform OriginalTransform=Hero->GetActorTransform();
  if (!TestTrue(TEXT("Both shipped characters can start the death sequence"),PC->BeginDeathPresentation())) continue;
  TestTrue(TEXT("Combat is actually paused during death"),UGameplayStatics::IsGamePaused(W));
  auto* Scene=PC->DeathScene.Get();auto* Silhouette=Scene->GetSilhouette();
  auto* Mesh=Silhouette->FindComponentByClass<USkeletalMeshComponent>();
  TestTrue(TEXT("Only the scene capture sees the cosmetic mesh"),bool(Mesh->bVisibleInSceneCaptureOnly));
  TestTrue(TEXT("Saved silhouette material includes clothing shaders"),Mesh->GetMaterial(0)->GetMaterial()->GetUsageByFlag(MATUSAGE_Clothing));
  for(int32 Slot=0;Slot<Mesh->GetNumMaterials();++Slot)
   TestEqual(TEXT("All body and cloth material slots use the same silhouette"),Mesh->GetMaterial(Slot),Mesh->GetMaterial(0));
  auto* Capture=Scene->FindComponentByClass<USceneCaptureComponent2D>();
  TestTrue(TEXT("Death capture keeps gameplay camera position"),Capture->GetComponentLocation().Equals(View.Location,.01));
  TestTrue(TEXT("Death capture keeps gameplay camera rotation"),Capture->GetComponentRotation().Equals(View.Rotation,.01));
  TestEqual(TEXT("Death keeps the original projection mode"),Capture->ProjectionType.GetValue(),View.ProjectionMode.GetValue());
  TestEqual(TEXT("Death keeps original field of view"),Capture->FOVAngle,View.FOV);
  TestEqual(TEXT("Death keeps original orthographic zoom"),Capture->OrthoWidth,View.OrthoWidth);
  TestTrue(TEXT("Death copies the original character facing and scale"),Mesh->GetComponentTransform().Equals(Hero->GetMesh()->GetComponentTransform(),.01));
  TestEqual(TEXT("Capture preserves full screen aspect ratio"),Scene->GetTexture()->SizeX*720,Scene->GetTexture()->SizeY*1280);
  FMinimalViewInfo ChangedView=View;ChangedView.Location+=FVector(400,0,200);ChangedView.FOV=90;
  PC->PlayerCameraManager->SetCameraCachePOV(ChangedView);PC->UpdateCameraManager(.1f);
  TestTrue(TEXT("Underlying gameplay camera stays fixed during the fade"),PC->PlayerCameraManager->GetCameraCacheView().Location.Equals(View.Location));
  TestEqual(TEXT("Camera freeze does not change the original view target"),PC->GetViewTarget(),OriginalViewTarget);
  FAssetCompilingManager::Get().FinishAllCompilation();
  // A neutral backdrop makes the transition's opacity measurable without a
  // game viewport. The actual runtime backdrop is the paused gameplay view.
  auto Composite=SNew(SOverlay)
    +SOverlay::Slot()[SNew(SColorBlock).Color(FLinearColor(.2f,.25f,.2f,1))]
    +SOverlay::Slot()[PC->DeathPresentationWidget->TakeWidget()];
  int32 InitialBackground=0;
  int64 HalfTitleLight=0;
  TArray<FTransform> FirstPose; TArray<FColor> Previous;
  for (int32 Stage=0;Stage<5;++Stage) {
   const float Time=Stage==0?0.f:Stage==1?PC->DeathFadeSeconds*.5f:Stage==2?PC->DeathFadeSeconds:Stage==3?PC->DeathFadeSeconds+PC->DeathTitleFadeSeconds*.5f:Scene->GetDuration()-.1f;
   Scene->SetElapsed(Time); PC->DeathPresentationWidget->SetElapsed(Time);
   // These samples are taken synchronously in one engine frame. Recreate the
   // render proxy so GPU skin buffers cannot reuse that frame's earlier pose.
   Mesh->MarkRenderStateDirty(); W->SendAllEndOfFrameUpdates(); FlushRenderingCommands();
   Scene->SetElapsed(Time); FlushRenderingCommands();
   if(Stage==2)FirstPose=Mesh->GetComponentSpaceTransforms();
   if(Stage==3){float Change=0;for(int32 I=0;I<FirstPose.Num();++I)Change+=FVector::Distance(FirstPose[I].GetLocation(),Mesh->GetComponentSpaceTransforms()[I].GetLocation());TestTrue(TEXT("Character silhouette plays the death animation with a paused world"),Change>20.f);}
   if(Stage==4){float Change=0;for(int32 I=0;I<FirstPose.Num();++I)Change+=FVector::Distance(FirstPose[I].GetLocation(),Mesh->GetComponentSpaceTransforms()[I].GetLocation());TestTrue(TEXT("Final death pose does not return to the starting pose"),Change>20.f);}
   FWidgetRenderer Renderer(false);auto* Target=Renderer.DrawWidget(Composite,FVector2D(1280,720));
   TArray<FColor> Pixels;FReadSurfaceDataFlags Flags;Flags.SetLinearToGamma(false);
   if(TestTrue(TEXT("Death presentation renders"),Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags))) {
    int32 CharacterPixels=0,Black=0,Changed=0;
    for(int32 I=0;I<Pixels.Num();++I){const auto P=Pixels[I];if(I/1280>230 && (Name==TEXT("Samurai") ? P.R>80&&P.G<25&&P.B<30 : P.B>80&&P.G<25&&P.R>P.B*.5f&&P.R<P.B*.95f))++CharacterPixels;if(P.R<5&&P.G<5&&P.B<5)++Black;if(Previous.Num()==Pixels.Num()&&Previous[I]!=P)++Changed;}
    const int32 Background=Pixels[0].G;
    if(Stage==0) { InitialBackground=Background; TestEqual(TEXT("Death starts fully transparent over gameplay"),Background,64); TestEqual(TEXT("Initial fade opacity is zero"),PC->DeathPresentationWidget->GetFadeOpacity(),0.f); }
    if(Stage==1) { TestTrue(TEXT("Mid fade darkens gameplay without cutting to black"),Background>10 && Background<InitialBackground-10); TestEqual(TEXT("Mid fade opacity is one half"),PC->DeathPresentationWidget->GetFadeOpacity(),.5f,.001f); }
    if(Stage>=2) { TestTrue(TEXT("Silhouette renders saturated red for Samurai and purple for Ninja"),CharacterPixels>300); TestTrue(TEXT("Completed fade stays fully black around the character"),Black>Pixels.Num()*.95); }
    if(Stage==3)TestTrue(TEXT("Rendered silhouette changes as the death animation plays"),Changed>300);
    int64 TitleLight=0;
    for(int32 I=1280*40;I<1280*140;++I)TitleLight+=Pixels[I].R;
    if(Stage==2)TestEqual(TEXT("Title waits for the gameplay fade to finish"),PC->DeathPresentationWidget->GetTitleOpacity(),0.f);
    if(Stage==3){HalfTitleLight=TitleLight;TestEqual(TEXT("Title fades gradually over its own longer interval"),PC->DeathPresentationWidget->GetTitleOpacity(),.5f,.001f);}
    if(Stage==4)TestTrue(TEXT("Rendered title is brighter at full reveal than halfway through"),HalfTitleLight>1000 && TitleLight>HalfTitleLight*1.7 && TitleLight<HalfTitleLight*2.3);
    Previous=Pixels;TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(1280,720,Pixels,PNG);
    FFileHelper::SaveArrayToFile(PNG,*(Output/(Name+FString::Printf(TEXT("Death_%d.png"),Stage))));
   }
  }
  // Render only cloth sections, so a correctly colored body cannot conceal a
  // default-material fallback on the skirt. Check the saved material in a new
  // editor process; an unsaved automatic usage fix is not sufficient for cook.
  const auto& LOD=Mesh->GetSkeletalMeshAsset()->GetResourceForRendering()->LODRenderData[0];
  int32 ClothSections=0;
  for(const auto& Section:LOD.RenderSections)if(Section.HasClothingData())++ClothSections;
  if(Name==TEXT("Samurai"))TestTrue(TEXT("Regression fixture contains real Samurai cloth sections"),ClothSections>0);
  if(ClothSections>0) {
   Mesh->SetForcedLOD(1);
   for(int32 Index=0;Index<LOD.RenderSections.Num();++Index) {
    const auto& Section=LOD.RenderSections[Index];
    Mesh->ShowMaterialSection(Section.MaterialIndex,Index,Section.HasClothingData(),0);
   }
   TInlineComponentArray<UStaticMeshComponent*> Weapons(Silhouette);
   for(auto* Weapon:Weapons)Weapon->SetVisibility(false);
   Scene->SetElapsed(PC->DeathFadeSeconds);Mesh->MarkRenderStateDirty();W->SendAllEndOfFrameUpdates();FlushRenderingCommands();
   Scene->SetElapsed(PC->DeathFadeSeconds);FlushRenderingCommands();
   // Sample the final UI, using the same render-target color conversion as
   // gameplay, rather than comparing raw scene-capture texels to UI colors.
   PC->DeathPresentationWidget->SetElapsed(PC->DeathFadeSeconds);
   FWidgetRenderer ClothRenderer(false);auto* ClothTarget=ClothRenderer.DrawWidget(Composite,FVector2D(1280,720));
   TArray<FColor> ClothPixels;FReadSurfaceDataFlags ClothFlags;ClothFlags.SetLinearToGamma(false);
   if(TestTrue(TEXT("Cloth-only silhouette capture renders"),ClothTarget->GameThread_GetRenderTargetResource()->ReadPixels(ClothPixels,ClothFlags))) {
    int32 Lit=0,Colored=0;
    for(const auto Pixel:ClothPixels) {
     if(Pixel.R>10||Pixel.G>10||Pixel.B>10)++Lit;
     if(Name==TEXT("Samurai") ? Pixel.R>80&&Pixel.G<25&&Pixel.B<30 : Pixel.B>80&&Pixel.G<25&&Pixel.R>Pixel.B*.5f&&Pixel.R<Pixel.B*.95f)++Colored;
    }
    TestTrue(TEXT("Cloth is visible in the character's silhouette color"),Colored>50);
    TestTrue(TEXT("Cloth never falls back to a grey/default material"),Colored>=Lit*.95f);
    TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(1280,720,ClothPixels,PNG);
    FFileHelper::SaveArrayToFile(PNG,*(Output/(Name+TEXT("ClothOnly.png"))));
   }
   Mesh->ShowAllMaterialSections(0);Mesh->SetForcedLOD(0);
   for(auto* Weapon:Weapons)Weapon->SetVisibility(true);
  }
  TestEqual(TEXT("Real character materials are untouched"),Hero->GetMesh()->GetMaterial(0),OriginalMaterial);
  TestTrue(TEXT("Real character is never teleported for the death shot"),Hero->GetActorTransform().Equals(OriginalTransform));
  const float HoldEnd=FMath::Max(Scene->GetDuration(),Scene->GetFadeDuration()+PC->DeathTitleFadeSeconds+.8f);
  PC->UpdateDeathPresentation(HoldEnd);
  TestFalse(TEXT("Results do not pop in when the death animation ends"),PC->bGameOverPresented);
  PC->UpdateDeathPresentation(PC->DeathOutroFadeSeconds*.5f);
  TestFalse(TEXT("Results wait while the silhouette fades to black"),PC->bGameOverPresented);
  TestEqual(TEXT("Title fades out with the silhouette"),PC->DeathPresentationWidget->GetTitleOpacity(),.5f,.001f);
  TestTrue(TEXT("World remains paused through the outro"),UGameplayStatics::IsGamePaused(W));
  PC->UpdateDeathPresentation(PC->DeathOutroFadeSeconds*.5f+.001f);
  TestTrue(TEXT("Results are constructed behind the black overlay"),PC->bGameOverPresented);
  if (TestNotNull(TEXT("Real results widget exists for the transition"),PC->GameOverWidget.Get())) {
   TestNotNull(TEXT("Death overlay stays alive during results reveal"),PC->DeathPresentationWidget.Get());
   TestFalse(TEXT("Mouse stays hidden until results are ready"),PC->bShowMouseCursor);
   TestEqual(TEXT("Results cannot receive clicks during their entrance"),PC->GameOverWidget->GetVisibility(),ESlateVisibility::HitTestInvisible);
   auto ResultsComposite=SNew(SOverlay)
     +SOverlay::Slot()[SNew(SColorBlock).Color(FLinearColor(.2f,.25f,.2f,1))]
     +SOverlay::Slot()[PC->GameOverWidget->TakeWidget()]
     +SOverlay::Slot()[PC->DeathPresentationWidget->TakeWidget()];
   auto RenderResults=[&](const TCHAR* Suffix) {
    FWidgetRenderer Renderer(false);auto* Target=Renderer.DrawWidget(ResultsComposite,FVector2D(1280,720));
    for(int32 Pass=0;Pass<3;++Pass){++GFrameCounter;Renderer.DrawWidget(Target,ResultsComposite,FVector2D(1280,720),.016f);}
    TArray<FColor> Pixels;FReadSurfaceDataFlags Flags;Flags.SetLinearToGamma(false);
    int64 Light=0;
    if(TestTrue(TEXT("Results transition renders"),Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags))) {
     for(const FColor P:Pixels)Light+=P.R+P.G+P.B;
     TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(1280,720,Pixels,PNG);
     FFileHelper::SaveArrayToFile(PNG,*(Output/(Name+Suffix+TEXT(".png"))));
    }
    return Light;
   };
   const int64 BlackLight=RenderResults(TEXT("Results_Black"));
   PC->UpdateDeathPresentation(PC->DeathResultsFadeSeconds*.5f);
   TestTrue(TEXT("Results fade continues while gameplay is paused"),UGameplayStatics::IsGamePaused(W));
   const int64 HalfLight=RenderResults(TEXT("Results_Half"));
   PC->UpdateDeathPresentation(PC->DeathResultsFadeSeconds*.5f);
   const int64 FullLight=RenderResults(TEXT("Results_Full"));
   TestTrue(TEXT("Transition passes through black without exposing gameplay"),BlackLight<1280*720);
   TestTrue(TEXT("Results become progressively visible, not a one-frame cut"),HalfLight>BlackLight+10000 && FullLight>HalfLight*1.6 && FullLight<HalfLight*2.4);
   TestEqual(TEXT("Results accept input once the fade completes"),PC->GameOverWidget->GetVisibility(),ESlateVisibility::Visible);
   TestTrue(TEXT("Cursor returns when results are ready"),PC->bShowMouseCursor);
   PC->GameOverWidget->RemoveFromParent();PC->GameOverWidget=nullptr;
  }
  TestNull(TEXT("Finishing removes death overlay"),PC->DeathPresentationWidget.Get());
  TestTrue(TEXT("Finishing destroys captured actor"),Silhouette->IsActorBeingDestroyed());
  TestFalse(TEXT("Death-owned pause is released"),UGameplayStatics::IsGamePaused(W));
  PC->bGameOverPresented=false;UGameplayStatics::SetGlobalTimeDilation(W,1.f);
  PC->PlayerCameraManager->FillCameraCache(View);
  TestTrue(TEXT("Sequence supports a fresh run"),PC->BeginDeathPresentation());
  auto* Cancelled=PC->DeathScene->GetSilhouette();PC->CancelDeathPresentation();
  TestTrue(TEXT("Travel/cancellation destroys silhouette"),Cancelled->IsActorBeingDestroyed());
  TestFalse(TEXT("Travel/cancellation cannot strand pause"),UGameplayStatics::IsGamePaused(W));
  TestTrue(TEXT("Death can restart after cancellation"),PC->BeginDeathPresentation());
  PC->UpdateDeathPresentation(FMath::Max(PC->DeathScene->GetDuration(),PC->DeathFadeSeconds+PC->DeathTitleFadeSeconds+.8f)+PC->DeathOutroFadeSeconds+.05f);
  TestNotNull(TEXT("Cancellation fixture reaches partially revealed results"),PC->GameOverWidget.Get());
  PC->CancelDeathPresentation();
  TestNull(TEXT("Cancellation removes partially revealed results"),PC->GameOverWidget.Get());
  TestFalse(TEXT("Cancellation clears pending result state"),PC->bGameOverPresented);
  TestFalse(TEXT("Cancellation during results releases pause"),UGameplayStatics::IsGamePaused(W));
  TestEqual(TEXT("Cancellation during results restores world time"),UGameplayStatics::GetGlobalTimeDilation(W),1.f);
  FSceneViewProjectionData Letterbox;
  Letterbox.SetViewRectangle(FIntRect(0,0,1280,720));
  Letterbox.SetConstrainedViewRectangle(FIntRect(160,0,1120,720));
  Letterbox.ViewOrigin=View.Location;
  FMinimalViewInfo NarrowView=View;NarrowView.AspectRatio=4.f/3.f;
  Letterbox.ProjectionMatrix=NarrowView.CalculateProjectionMatrix();
  auto* Constrained=W->SpawnActor<APlayerDeathScene>();
  TestTrue(TEXT("Constrained gameplay viewport is supported"),Constrained->Initialize(Hero,PC->DeathSilhouetteColor,1,NarrowView,Letterbox,.28f));
  TestTrue(TEXT("Letterbox capture stays within the same screen rectangle"),Constrained->GetViewRect().Min.Equals(FVector2D(.125,0)) && Constrained->GetViewRect().Max.Equals(FVector2D(.875,1)));
  TestTrue(TEXT("Final projection matrix is copied exactly"),Constrained->FindComponentByClass<USceneCaptureComponent2D>()->CustomProjectionMatrix.Equals(Letterbox.ProjectionMatrix));
  Constrained->Destroy();
  Hero->Destroy();
 }
 PC->CancelDeathPresentation();W->DestroyWorld(false);GEngine->DestroyWorldContext(W);return true;
}
#endif
