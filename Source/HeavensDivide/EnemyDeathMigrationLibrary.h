#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EnemyDeathMigrationLibrary.generated.h"

class UMaterial;
/** Editor migration support for Tools/migrate_enemy_death.py. */
UCLASS()
class UEnemyDeathMigrationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="Editor|Enemy Death", meta=(DevelopmentOnly))
	static void RemoveLegacyCollapseNodes(UMaterial* Material);
};
