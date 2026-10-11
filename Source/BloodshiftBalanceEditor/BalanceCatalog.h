#pragma once
#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"

class UBlueprint;

/** Every entry points at the authoritative asset or Blueprint component template. */
struct FBalanceEntry
{
    FString Name, Group, Search, Notes;
    TStrongObjectPtr<UObject> Object;
    TWeakObjectPtr<UBlueprint> Blueprint;
    bool bInactive = false;
};

class FBalanceCatalog
{
public:
    TArray<TSharedPtr<FBalanceEntry>> Entries;
    void Refresh();
    static void Changed(const FBalanceEntry& Entry);
    static bool Save(const TArray<TSharedPtr<FBalanceEntry>>& Entries, FString& Result);
    static bool CaptureBaseline(const TArray<TSharedPtr<FBalanceEntry>>& Entries);
    static bool ResetToBaseline(const FBalanceEntry& Entry);
    static bool HasBaseline(const FBalanceEntry& Entry);
};
