#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "LevelUpWidget.h"
#include "SurvivorPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Slate/WidgetRenderer.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/SVirtualWindow.h"
#include "Input/HittestGrid.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUpgradeCardLayoutTest, "HeavensDivide.UI.UpgradeCardLayout",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)
bool FUpgradeCardLayoutTest::RunTest(const FString&)
{
 auto* GI = NewObject<UGameInstance>(GEngine);
 GI->InitializeStandalone();
 auto* World = GI->GetWorld();
 auto* SavedClass = LoadClass<ASurvivorPlayerController>(nullptr, TEXT("/Game/HeavensDivide/Blueprints/BP_SurvivorPlayerController.BP_SurvivorPlayerController_C"));
 auto* PC = World->SpawnActor<ASurvivorPlayerController>(SavedClass);
 auto* Player = NewObject<ULocalPlayer>(GEngine);
 PC->SetPlayer(Player);
 auto* U = PC->GetPlayerUpgrades();
 U->BeginDirectUpgradeSelection();
 auto* Class = LoadClass<ULevelUpWidget>(nullptr, TEXT("/Game/HeavensDivide/Blueprints/UI/UpgradeUI/WBP_LevelUp.WBP_LevelUp_C"));
 auto* Widget = CreateWidget<ULevelUpWidget>(GI, Class);
 Widget->TakeWidget();
 Widget->InitializeDirectUpgradeWidget(PC);
 TArray<UUpgradeDefinition*> Cards;
 for (TObjectIterator<UUpgradeDefinition> It; It; ++It)
  if (It->GetPathName().StartsWith(TEXT("/Game/HeavensDivide/Upgrades/"))) Cards.Add(*It);
 TestTrue(TEXT("Saved upgrade catalog loaded"), Cards.Num() >= 100);
 Cards.Sort([](const UUpgradeDefinition& A, const UUpgradeDefinition& B) { return A.Description.ToString().Len() > B.Description.ToString().Len(); });
 FWidgetRenderer Renderer(false);
 auto Window = SNew(SVirtualWindow).Size(FVector2D(1920,1080));
 Window->SetContent(Widget->TakeWidget());
 FHittestGrid Grid;
 auto* Target = FWidgetRenderer::CreateTargetFor(FVector2D(1920,1080), TF_Bilinear, false);
 for (int32 Batch = 0; Batch * 3 < Cards.Num(); ++Batch)
 {
  for (int32 I = 0; I < 3; ++I)
  {
   auto* Card = Cards[FMath::Min(Batch * 3 + I, Cards.Num() - 1)];
   Cast<UTextBlock>(Widget->WidgetTree->FindWidget(*FString::Printf(TEXT("UpgradeCardTitle_%d"), I)))->SetText(Card->DisplayName);
   Cast<UTextBlock>(Widget->WidgetTree->FindWidget(*FString::Printf(TEXT("UpgradeCardDescription_%d"), I)))->SetText(Card->Description);
   Cast<UImage>(Widget->WidgetTree->FindWidget(*FString::Printf(TEXT("UpgradeArtwork_%d"), I)))->SetBrushFromTexture(Card->CardArtwork, true);
  }
  Widget->ForceLayoutPrepass();
  // Nested scale boxes normalize against their previous paint geometry.
  // Simulate several layout frames, as the visible menu does after opening.
  for (int32 Frame = 0; Frame < 8; ++Frame)
  {
   Widget->TakeWidget()->Invalidate(EInvalidateWidgetReason::Prepass);
   Widget->ForceLayoutPrepass();
   Renderer.DrawWindow(Target, Grid, Window, 1.f, FVector2D(1920,1080), 1.f / 60.f);
  }
  for (int32 I = 0; I < 3; ++I)
  {
   auto* Text = Widget->WidgetTree->FindWidget(*FString::Printf(TEXT("UpgradeCardDescription_%d"), I));
   auto* Fit = Widget->WidgetTree->FindWidget(*FString::Printf(TEXT("UpgradeDescriptionFit_%d"), I));
   const auto& TG = Text->GetPaintSpaceGeometry();
   const auto& FG = Fit->GetPaintSpaceGeometry();
   const FVector2D Top = FG.AbsoluteToLocal(TG.LocalToAbsolute(FVector2D::ZeroVector));
   const FVector2D Bottom = FG.AbsoluteToLocal(TG.LocalToAbsolute(TG.GetLocalSize()));
   if (Batch == 0)
   {
    AddInfo(FString::Printf(TEXT("Card %d: text %s fit %s top %s bottom %s"), I, *TG.GetLocalSize().ToString(), *FG.GetLocalSize().ToString(), *Top.ToString(), *Bottom.ToString()));
    for (UWidget* W = Text; W; W = Cast<UWidget>(W->GetParent()))
     AddInfo(FString::Printf(TEXT("%s desired=%s painted=%s visible=%d"), *W->GetName(), *W->GetDesiredSize().ToString(), *W->GetPaintSpaceGeometry().GetLocalSize().ToString(), W->IsVisible()));
   }
   TestTrue(TEXT("Description stays inside its allotted frame"), Top.X >= -1 && Top.Y >= -1 && Bottom.X <= FG.GetLocalSize().X + 1 && Bottom.Y <= FG.GetLocalSize().Y + 1);
   TestTrue(TEXT("Description has visible layout"), TG.GetLocalSize().Y > 0 && FG.GetLocalSize().Y > 0);
   TestTrue(*FString::Printf(TEXT("Full description fits without truncation: %s"), *Cards[FMath::Min(Batch * 3 + I, Cards.Num() - 1)]->UpgradeId.ToString()), Text->GetDesiredSize().Y <= FG.GetLocalSize().Y + 1);
   auto* Art = Widget->WidgetTree->FindWidget(*FString::Printf(TEXT("UpgradeArtwork_%d"), I));
   auto* Panel = Widget->WidgetTree->FindWidget(*FString::Printf(TEXT("UpgradeArtworkContainer_%d"), I));
   const auto& AG = Art->GetPaintSpaceGeometry();
   const auto& PG = Panel->GetPaintSpaceGeometry();
   const FVector2D ArtTop = PG.AbsoluteToLocal(AG.LocalToAbsolute(FVector2D::ZeroVector));
   const FVector2D ArtBottom = PG.AbsoluteToLocal(AG.LocalToAbsolute(AG.GetLocalSize()));
   TestTrue(TEXT("Artwork covers its panel without gaps"), ArtTop.X <= 1 && ArtTop.Y <= 1 && ArtBottom.X >= PG.GetLocalSize().X - 1 && ArtBottom.Y >= PG.GetLocalSize().Y - 1);
  }
  if (Batch < 2)
  {
   TArray<FColor> Pixels;
   FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
   if (Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags))
   {
    TArray64<uint8> PNG;
    FImageUtils::PNGCompressImageArray(Target->SizeX, Target->SizeY, Pixels, PNG);
    FFileHelper::SaveArrayToFile(PNG, *(FPaths::ProjectSavedDir()/FString::Printf(TEXT("UpgradeCardsPolished%d.png"), Batch)));
   }
  }
 }
 Widget->ReleaseSlateResources(true);
 World->DestroyWorld(false);
 GEngine->DestroyWorldContext(World);
 GI->Shutdown();
 return true;
}
#endif
