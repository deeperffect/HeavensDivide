#include "SBloodshiftBalance.h"
#include "Framework/Docking/TabManager.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "DetailLayoutBuilder.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "UpgradeDefinition.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Misc/MessageDialog.h"
#include "Styling/AppStyle.h"

void SBloodshiftBalance::Construct(const FArguments& Args)
{
    Catalog.Refresh();
    Groups.Add(MakeShared<FString>(TEXT("All categories")));
    TSet<FString> Seen;
    for(const auto& E:Catalog.Entries)
    {
        const FString Main=E->Group.Left(E->Group.Find(TEXT(" / "))==INDEX_NONE?E->Group.Len():E->Group.Find(TEXT(" / ")));
        if(!Seen.Contains(Main)) { Seen.Add(Main); Groups.Add(MakeShared<FString>(Main)); }
    }
    FDetailsViewArgs D; D.bAllowSearch=true; D.bHideSelectionTip=true; D.bLockable=false;
    D.NameAreaSettings=FDetailsViewArgs::HideNameArea; D.bShowOptions=true;
    Details=FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor").CreateDetailView(D);
    Details->SetIsPropertyEditingEnabledDelegate(FIsPropertyEditingEnabled::CreateLambda([]{return !GEditor || !GEditor->PlayWorld;}));
    Details->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateLambda([](const FPropertyAndParent& P)
    { return !P.Property.HasMetaData(TEXT("DeprecatedProperty")) && !P.Property.HasAnyPropertyFlags(CPF_Deprecated); }));
    Details->OnFinishedChangingProperties().AddLambda([this](const FPropertyChangedEvent&)
    {
        if(Selected) { FBalanceCatalog::Changed(*Selected); Status=TEXT("Unsaved changes. Save, then start a fresh run to test the new balance."); }
    });
    if(GEditor) GEditor->RegisterForUndo(this);
    ChildSlot
    [
        SNew(SBorder).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder")).Padding(0)
        [
        SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight().Padding(14,10)
        [ SNew(SHorizontalBox)
            +SHorizontalBox::Slot().FillWidth(1)
            [ SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(TEXT("BLOODSHIFT  /  BALANCE"))).Font(FAppStyle::GetFontStyle("HeadingExtraSmall"))]
                +SVerticalBox::Slot().AutoHeight().Padding(0,5)[SNew(STextBlock).Text(FText::FromString(TEXT("Stances, attacks, upgrades and encounters — one searchable workspace.")))] ]
            +SHorizontalBox::Slot().AutoWidth().Padding(5,0)[SNew(SButton).Text(FText::FromString(TEXT("Balance Lab"))).ToolTipText(FText::FromString(TEXT("Open clickable controls for combat tests"))).OnClicked_Lambda([]{FGlobalTabmanager::Get()->TryInvokeTab(FName(TEXT("BloodshiftBalanceLab")));return FReply::Handled();})]
            +SHorizontalBox::Slot().AutoWidth().Padding(5,0)[SNew(SButton).Text(FText::FromString(TEXT("Save changes"))).OnClicked(this,&SBloodshiftBalance::Save)]
            +SHorizontalBox::Slot().AutoWidth().Padding(5,0)[SNew(SButton).Text(FText::FromString(TEXT("Refresh"))).OnClicked_Lambda([this]{ Reload();return FReply::Handled(); })]
        ]
        +SVerticalBox::Slot().FillHeight(1)
        [ SNew(SSplitter)
            +SSplitter::Slot().Value(.32f)
            [ SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight().Padding(10,4)[SNew(SSearchBox).HintText(FText::FromString(TEXT("Find an ability, upgrade or stat..."))).OnTextChanged_Lambda([this](const FText& T){Query=T.ToString();Filter();})]
                +SVerticalBox::Slot().AutoHeight().Padding(10,4)
                [SNew(SComboBox<TSharedPtr<FString>>).OptionsSource(&Groups).InitiallySelectedItem(Groups[0])
                    .OnGenerateWidget_Lambda([](TSharedPtr<FString> S){return SNew(STextBlock).Text(FText::FromString(*S));})
                    .OnSelectionChanged_Lambda([this](TSharedPtr<FString> S,ESelectInfo::Type){if(S){Group=*S;Filter();}})
                    [SNew(STextBlock).Text_Lambda([this]{return FText::FromString(Group);})]]
                +SVerticalBox::Slot().AutoHeight().Padding(10,4)
                [SNew(SCheckBox).OnCheckStateChanged_Lambda([this](ECheckBoxState S){bShowInactive=S==ECheckBoxState::Checked;Filter();})
                    [SNew(STextBlock).Text(FText::FromString(TEXT("Include inactive / retired upgrade assets")))]]
                +SVerticalBox::Slot().AutoHeight().Padding(10,4)[SNew(STextBlock).Text_Lambda([this]{return FText::FromString(FString::Printf(TEXT("%d entries"),Filtered.Num()));})]
                +SVerticalBox::Slot().FillHeight(1).Padding(6)
                [SAssignNew(List,SListView<TSharedPtr<FBalanceEntry>>).ListItemsSource(&Filtered).SelectionMode(ESelectionMode::Single)
                    .OnSelectionChanged(this,&SBloodshiftBalance::Select)
                    .OnGenerateRow_Lambda([](TSharedPtr<FBalanceEntry> E,const TSharedRef<STableViewBase>& Owner)
                    { return SNew(STableRow<TSharedPtr<FBalanceEntry>>,Owner).Padding(FMargin(8,6))
                        [SNew(SVerticalBox)
                            +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(E->Name+(E->bInactive?TEXT(" [inactive]"):TEXT("")))).Font(FAppStyle::GetFontStyle("NormalFontBold"))]
                            +SVerticalBox::Slot().AutoHeight().Padding(0,3)[SNew(STextBlock).Text(FText::FromString(E->Group)).ColorAndOpacity(FLinearColor(.57f,.63f,.65f))]]; })]
            ]
            +SSplitter::Slot().Value(.68f)
            [SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight().Padding(12,8)[SNew(STextBlock).Text_Lambda([this]{return FText::FromString(Selected?Selected->Name:TEXT("Select a balance entry"));}).Font(FAppStyle::GetFontStyle("HeadingExtraSmall"))]
                +SVerticalBox::Slot().AutoHeight().Padding(12,4)[SNew(STextBlock).Text_Lambda([this]{return FText::FromString(Selected?Selected->Notes:TEXT("Choose a category or search the list. Each entry edits the settings the game actually uses."));}).AutoWrapText(true)]
                +SVerticalBox::Slot().AutoHeight().Padding(12,4)[SNew(STextBlock).Text_Lambda([this]{const auto* Card=Selected?Cast<UUpgradeDefinition>(Selected->Object.Get()):nullptr;return Card?Card->GetBalanceDescription():FText::GetEmpty();}).AutoWrapText(true).ColorAndOpacity(FLinearColor(.85f,.76f,.53f))]
                +SVerticalBox::Slot().AutoHeight().Padding(12,4)[SNew(SHorizontalBox)
                    +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(FText::FromString(TEXT("Browse source asset"))).IsEnabled_Lambda([this]{return Selected.IsValid();}).OnClicked_Lambda([this]
                        {if(Selected){TArray<UObject*> Objects;Objects.Add(Selected->Blueprint.IsValid()?static_cast<UObject*>(Selected->Blueprint.Get()):Selected->Object.Get());FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get().SyncBrowserToAssets(Objects);}return FReply::Handled();})]
                    +SHorizontalBox::Slot().AutoWidth().Padding(8,0)[SNew(SButton).Text(FText::FromString(TEXT("Reset entry to baseline"))).IsEnabled_Lambda([this]{return Selected.IsValid();}).OnClicked(this,&SBloodshiftBalance::Reset)]]
                +SVerticalBox::Slot().AutoHeight().Padding(12,4)[SNew(STextBlock).Text_Lambda([this]{return FText::FromString(Selected?Selected->Object->GetPathName():FString());}).AutoWrapText(true).ColorAndOpacity(FLinearColor(.55f,.6f,.65f))]
                +SVerticalBox::Slot().FillHeight(1).Padding(6)[Details.ToSharedRef()]
            ]
        ]
        +SVerticalBox::Slot().AutoHeight().Padding(12,8)[SNew(STextBlock).Text_Lambda([this]{return FText::FromString(Status);}).AutoWrapText(true)]
        ]
    ];
    Filter(); if(!Filtered.IsEmpty()) List->SetSelection(Filtered[0]);
}
SBloodshiftBalance::~SBloodshiftBalance() { if(GEditor) GEditor->UnregisterForUndo(this); }
void SBloodshiftBalance::PostUndo(bool)
{
    if(Details) Details->ForceRefresh();
}
void SBloodshiftBalance::Filter()
{
    Filtered.Reset(); TArray<FString> Terms; Query.ParseIntoArrayWS(Terms);
    for(const auto& E:Catalog.Entries)
    {
        if(E->bInactive&&!bShowInactive) continue;
        if(Group!=TEXT("All categories")&&!E->Group.StartsWith(Group))continue;
        bool Match=true;for(const auto& Term:Terms) if(!E->Search.Contains(Term)) { Match=false;break; }
        if(Match) Filtered.Add(E);
    }
    if(List) List->RequestListRefresh();
}
void SBloodshiftBalance::Select(TSharedPtr<FBalanceEntry> E,ESelectInfo::Type)
{ Selected=E; Details->SetObject(E?E->Object.Get():nullptr); }
void SBloodshiftBalance::SelectByName(const FString& Name)
{for(const auto& E:Filtered)if(E->Object->GetPathName()==Name||E->Name.Contains(Name)){List->SetSelection(E);List->RequestScrollIntoView(E);break;}}
void SBloodshiftBalance::Reload()
{
    const FString Path=Selected?Selected->Object->GetPathName():FString();
    Details->SetObject(nullptr);Selected.Reset();Catalog.Refresh();Filter();
    for(const auto& E:Filtered)if(E->Object->GetPathName()==Path){List->SetSelection(E);return;}
}
FReply SBloodshiftBalance::Save()
{
    if(GEditor&&GEditor->PlayWorld){Status=TEXT("Stop Play in Editor before saving balance changes.");return FReply::Handled();}
    const FString SelectionName=Selected?Selected->Name:FString();
    const FString SelectionGroup=Selected?Selected->Group:FString();
    Details->SetObject(nullptr);
    FBalanceCatalog::Save(Catalog.Entries,Status); Reload();
    for(const auto& E:Filtered)if(E->Name==SelectionName&&E->Group==SelectionGroup){List->SetSelection(E);List->RequestScrollIntoView(E);break;}
    return FReply::Handled();
}
FReply SBloodshiftBalance::Reset()
{
    if(GEditor&&GEditor->PlayWorld){Status=TEXT("Stop Play in Editor before resetting balance changes.");return FReply::Handled();}
    if(!Selected)return FReply::Handled();
    if(!FBalanceCatalog::HasBaseline(*Selected)){Status=TEXT("No installed baseline for this new entry. Use Unreal's per-property reset arrows or Undo.");return FReply::Handled();}
    if(FMessageDialog::Open(EAppMsgType::YesNo,FText::FromString(TEXT("Restore this entry to the balance captured when this tool was installed? This is undoable and is not saved until Save changes.")))==EAppReturnType::Yes)
    {FBalanceCatalog::ResetToBaseline(*Selected);Details->ForceRefresh();Status=TEXT("Restored selected entry to baseline. Unsaved; Ctrl+Z to undo.");}
    return FReply::Handled();
}
