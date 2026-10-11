#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "SBloodshiftBalance.h"
#include "UpgradeDefinition.h"
#include "BloodshiftBalanceData.h"
#include "ScopedTransaction.h"
#include "Editor.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "RenderingThread.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBalanceWorkspaceTest,"HeavensDivide.Balance.Workspace",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FBalanceWorkspaceTest::RunTest(const FString&)
{
    const auto Widget=SNew(SBloodshiftBalance);
    const auto& Catalog=Widget->GetCatalog();
    TestTrue(TEXT("Unified catalogue includes gameplay entries"),Catalog.Entries.Num()>180);
    TSharedPtr<FBalanceEntry> Critical;
    int32 UpgradeCount=0;bool SamuraiAttack=false,NinjaAttack=false,Oracle=false;
    for(const auto& E:Catalog.Entries)
    {
        if(auto* U=Cast<UUpgradeDefinition>(E->Object.Get()))
        {++UpgradeCount;if(U->UpgradeId==TEXT("BloodCritical"))Critical=E;}
        SamuraiAttack|=E->Name.Contains(TEXT("Samurai"))&&E->Name.Contains(TEXT("Auto Attack"));
        NinjaAttack|=E->Name.Contains(TEXT("Ninja"))&&E->Name.Contains(TEXT("Auto Attack"));
        Oracle|=E->Name.Contains(TEXT("Oracle"));
    }
    TestTrue(TEXT("All upgrade assets available"),UpgradeCount>=140);
    TestTrue(TEXT("Samurai attack template listed"),SamuraiAttack);
    TestTrue(TEXT("Ninja attack template listed"),NinjaAttack);
    TestTrue(TEXT("Boss defaults listed"),Oracle);
    if(!TestTrue(TEXT("Blood Critical found"),Critical.IsValid()))return false;
    auto* Card=CastChecked<UUpgradeDefinition>(Critical->Object.Get());
    const auto Before=Card->BalanceParameters;
    const bool WasDirty=Card->GetOutermost()->IsDirty();
    {
        const FScopedTransaction Transaction(FText::FromString(TEXT("Balance validation")));
        Card->SetFlags(RF_Transactional);Card->Modify();
        Card->BalanceParameters.Add(TEXT("Chance"),.42f);FBalanceCatalog::Changed(*Critical);
    }
    TestEqual(TEXT("Edited number is authoritative"),Card->GetBalanceValue(TEXT("Chance"),0),.42f);
    TestTrue(TEXT("Card copy reflects edited proc chance"),Card->GetBalanceDescription().ToString().Contains(TEXT("42%")));
    GEditor->UndoTransaction();
    TestTrue(TEXT("Undo restores tuning"),Card->BalanceParameters.OrderIndependentCompareEqual(Before));
    GEditor->RedoTransaction();
    TestEqual(TEXT("Redo restores changed tuning"),Card->GetBalanceValue(TEXT("Chance"),0),.42f);
    GEditor->UndoTransaction();Card->BalanceParameters=Before;Card->GetOutermost()->SetDirtyFlag(WasDirty);
    Widget->SelectByName(Critical->Object->GetPathName());
    Widget->SlatePrepass(1.f);
    const FString Out=FPaths::ProjectSavedDir()/TEXT("BalanceWorkspace/Preview.png");
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Out),true);
    FWidgetRenderer Renderer(true);
    auto* Target=Renderer.DrawWidget(Widget,FVector2D(1600,1000));
    for(int32 I=0;I<4;++I){Widget->Invalidate(EInvalidateWidgetReason::Prepass);Widget->SlatePrepass(1.f);Renderer.DrawWidget(Target,Widget,FVector2D(1600,1000),1.f/60.f);}
    FlushRenderingCommands();
    TArray<FColor> Pixels;
    if(TestNotNull(TEXT("Balance editor rendered"),Target)&&Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels))
    {
        TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(1600,1000,Pixels,PNG);
        TestTrue(TEXT("Preview saved"),FFileHelper::SaveArrayToFile(PNG,*Out));
    }
    return true;
}
#endif
