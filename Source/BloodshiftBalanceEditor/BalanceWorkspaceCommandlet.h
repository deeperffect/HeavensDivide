#pragma once
#include "Commandlets/Commandlet.h"
#include "BalanceWorkspaceCommandlet.generated.h"
UCLASS()
class UBalanceWorkspaceCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UBalanceWorkspaceCommandlet() { IsClient=false;IsServer=false;IsEditor=true;LogToConsole=true; }
    virtual int32 Main(const FString& Params) override;
};
