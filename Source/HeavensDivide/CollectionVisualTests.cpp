#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "SCollectionJournal.h"
#include "CollectionProgressSubsystem.h"
#include "RunReportSubsystem.h"
#include "SynergyMetaProgressionSubsystem.h"
#include "UpgradeDefinition.h"
#include "BloodshiftMenuStyle.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SBox.h"
#include "Slate/WidgetRenderer.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Kismet/GameplayStatics.h"
#include "RenderingThread.h"
#include "AssetCompilingManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCollectionVisualTest,"HeavensDivide.Collection.Visual",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FCollectionVisualTest::RunTest(const FString&)
{
    const FString Cmd=FCommandLine::Get(),Slot=TEXT("Automation_Journal_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
    FCommandLine::Set(*(TEXT("-MetaSaveSlot=")+Slot+TEXT(" ")+Cmd));
    auto* GI=NewObject<UGameInstance>(GEngine);GI->InitializeStandalone();FCommandLine::Set(*Cmd);
    auto* Progress=GI->GetSubsystem<UCollectionProgressSubsystem>();auto* Meta=GI->GetSubsystem<USynergyMetaProgressionSubsystem>();
    const auto Definitions=Meta->GetCollectionUpgradeDefinitions(EUpgradeCategory::Samurai);
    for(int32 I=0;I<FMath::Min(14,Definitions.Num());++I)Progress->RecordUpgrade(Definitions[I],I%2==0?Definitions[I]->MaxLevel:1);
    FRunScoreEntry Run;Run.RunId=FGuid::NewGuid();Run.MapId=TEXT("FallenGrove");Run.Seconds=620;Run.NormalKills=455;Run.Objectives=3;Progress->RecordRun(Run);
    auto Journal=SNew(SCollectionJournal).GameInstance(GI);
    auto Frame=SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(BloodshiftMenu::Ink).Padding(28)[Journal];
    FAssetCompilingManager::Get().FinishAllCompilation();
    const FString Dir=FPaths::ProjectSavedDir()/TEXT("CollectionCombat/Previews");IFileManager::Get().MakeDirectory(*Dir,true);
    FWidgetRenderer Renderer(false);
    auto Render=[&](const TCHAR* Name)
    {
        auto* Target=Renderer.DrawWidget(Frame,FVector2D(1200,760));
        for(int32 I=0;I<5;++I)Renderer.DrawWidget(Target,Frame,FVector2D(1200,760),1.f/60);
        FlushRenderingCommands();TArray<FColor> Pixels;FReadSurfaceDataFlags Flags;Flags.SetLinearToGamma(false);
        TestTrue(TEXT("Journal render readable"),Target&&Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags));
        TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(1200,760,Pixels,PNG);FFileHelper::SaveArrayToFile(PNG,*(Dir/Name));
    };
    TestTrue(TEXT("Catalog contains real upgrade assets"),Journal->Definitions.Num()>50);
    Render(TEXT("Collection.png"));
    Journal->Category=5;Journal->Selected=INDEX_NONE;Journal->RebuildEntries();Render(TEXT("Achievements.png"));
    Journal->Filter=1;Journal->RebuildEntries();TestTrue(TEXT("Incomplete achievements filter excludes completed goals"),Journal->Tiles.Num()<CollectionJournal::Achievements().Num());
    auto* W=GI->GetWorld();GI->Shutdown();GEngine->DestroyWorldContext(W);W->DestroyWorld(false);UGameplayStatics::DeleteGameInSlot(Slot,0);
    return true;
}
#endif
