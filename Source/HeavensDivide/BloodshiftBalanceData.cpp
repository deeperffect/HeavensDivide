#include "BloodshiftBalanceData.h"
#include "UObject/StrongObjectPtr.h"
UBloodshiftBalanceData::UBloodshiftBalanceData() { Skills=MetaSkillTree::DefaultNodes(); }
const UBloodshiftBalanceData* UBloodshiftBalanceData::Get()
{
    static TStrongObjectPtr<UBloodshiftBalanceData> Asset(LoadObject<UBloodshiftBalanceData>(nullptr,
        TEXT("/Game/HeavensDivide/Balance/DA_GlobalBalance.DA_GlobalBalance")));
    return Asset.IsValid()?Asset.Get():GetDefault<UBloodshiftBalanceData>();
}
