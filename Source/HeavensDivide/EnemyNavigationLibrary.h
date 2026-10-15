#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EnemyNavigationLibrary.generated.h"

/** Editor-only baking implementation; no navigation generation at runtime. */
UCLASS()
class HEAVENSDIVIDE_API UEnemyNavigationLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Editor|Enemy Navigation", meta=(WorldContext="WorldContext"))
    static bool BakeOldVillageNavigation(UObject* WorldContext);
};
