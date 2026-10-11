#include "Modules/ModuleManager.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "ToolMenus.h"
#include "HAL/IConsoleManager.h"
#include "SBloodshiftBalance.h"
#include "SBalanceLab.h"
#include "PropertyEditorModule.h"
#include "UpgradeBalanceDetails.h"

static const FName BalanceTab(TEXT("BloodshiftBalance"));
static const FName LabTab(TEXT("BloodshiftBalanceLab"));
class FBloodshiftBalanceEditorModule : public IModuleInterface
{
    IConsoleObject* Command=nullptr;
    void Open() { FGlobalTabmanager::Get()->TryInvokeTab(BalanceTab); }
    void OpenLab() { FGlobalTabmanager::Get()->TryInvokeTab(LabTab); }
public:
    virtual void StartupModule() override
    {
        FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor").RegisterCustomClassLayout(TEXT("UpgradeDefinition"),FOnGetDetailCustomizationInstance::CreateStatic(&FUpgradeBalanceDetails::Make));
        if(IsRunningCommandlet())return;
        FGlobalTabmanager::Get()->RegisterNomadTabSpawner(BalanceTab,FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&)
            {return SNew(SDockTab).TabRole(ETabRole::NomadTab)[SNew(SBloodshiftBalance)];}))
            .SetDisplayName(FText::FromString(TEXT("Bloodshift Balance")))
            .SetTooltipText(FText::FromString(TEXT("Edit all gameplay balance from one workspace.")));
        FGlobalTabmanager::Get()->RegisterNomadTabSpawner(LabTab,FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&)
            {return SNew(SDockTab).TabRole(ETabRole::NomadTab)[SNew(SBalanceLab)];}))
            .SetDisplayName(FText::FromString(TEXT("Balance Lab")))
            .SetTooltipText(FText::FromString(TEXT("Builds, targets, upgrades and measurements for the live combat lab.")));
        UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this,&FBloodshiftBalanceEditorModule::RegisterMenus));
        Command=IConsoleManager::Get().RegisterConsoleCommand(TEXT("Bloodshift.Balance"),TEXT("Open the Bloodshift Balance editor."),FConsoleCommandDelegate::CreateRaw(this,&FBloodshiftBalanceEditorModule::Open));
    }
    void RegisterMenus()
    {
        FToolMenuOwnerScoped Owner(this);
        for(const FName Menu:{FName(TEXT("LevelEditor.MainMenu.Tools")),FName(TEXT("LevelEditor.MainMenu.Window"))})
        {
            auto* M=UToolMenus::Get()->ExtendMenu(Menu);
            M->FindOrAddSection(TEXT("Bloodshift")).AddMenuEntry(TEXT("BloodshiftBalance"),FText::FromString(TEXT("Bloodshift Balance")),
                FText::FromString(TEXT("All abilities, upgrades and combat settings")),FSlateIcon(),FUIAction(FExecuteAction::CreateRaw(this,&FBloodshiftBalanceEditorModule::Open)));
            M->FindOrAddSection(TEXT("Bloodshift")).AddMenuEntry(TEXT("BloodshiftBalanceLab"),FText::FromString(TEXT("Balance Lab")),
                FText::FromString(TEXT("Open clickable combat test controls")),FSlateIcon(),FUIAction(FExecuteAction::CreateRaw(this,&FBloodshiftBalanceEditorModule::OpenLab)));
        }
    }
    virtual void ShutdownModule() override
    {
        if(auto* Property=FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor"))Property->UnregisterCustomClassLayout(TEXT("UpgradeDefinition"));
        if(Command)IConsoleManager::Get().UnregisterConsoleObject(Command);
        UToolMenus::UnRegisterStartupCallback(this); UToolMenus::UnregisterOwner(this);
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(BalanceTab);
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(LabTab);
    }
};
IMPLEMENT_MODULE(FBloodshiftBalanceEditorModule,BloodshiftBalanceEditor)
