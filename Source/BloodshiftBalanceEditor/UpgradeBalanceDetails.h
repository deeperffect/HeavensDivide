#pragma once
#include "IDetailCustomization.h"
class FUpgradeBalanceDetails : public IDetailCustomization
{
public:
    static TSharedRef<IDetailCustomization> Make() { return MakeShared<FUpgradeBalanceDetails>(); }
    virtual void CustomizeDetails(IDetailLayoutBuilder& Builder) override;
};
