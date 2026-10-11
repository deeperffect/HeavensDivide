#pragma once
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "EditorUndoClient.h"
#include "BalanceCatalog.h"

class IDetailsView;
class SBloodshiftBalance : public SCompoundWidget, public FEditorUndoClient
{
public:
    SLATE_BEGIN_ARGS(SBloodshiftBalance) {} SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    virtual ~SBloodshiftBalance();
    virtual void PostUndo(bool bSuccess) override;
    virtual void PostRedo(bool bSuccess) override { PostUndo(bSuccess); }
    const FBalanceCatalog& GetCatalog() const { return Catalog; }
    void SelectByName(const FString& Name);
private:
    FBalanceCatalog Catalog;
    TArray<TSharedPtr<FBalanceEntry>> Filtered;
    TArray<TSharedPtr<FString>> Groups;
    TSharedPtr<SListView<TSharedPtr<FBalanceEntry>>> List;
    TSharedPtr<IDetailsView> Details;
    TSharedPtr<FBalanceEntry> Selected;
    FString Query, Group=TEXT("All categories"), Status=TEXT("Select an entry. Save changes before starting a fresh run. Ctrl+Z / Ctrl+Y undo and redo edits.");
    bool bShowInactive=false;
    void Filter();
    void Select(TSharedPtr<FBalanceEntry> Entry,ESelectInfo::Type Type);
    void Reload();
    FReply Save();
    FReply Reset();
};
