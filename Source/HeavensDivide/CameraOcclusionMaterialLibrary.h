#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CameraOcclusionMaterialLibrary.generated.h"
class UMaterial;

UCLASS()
class HEAVENSDIVIDE_API UCameraOcclusionMaterialLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Only accepts project-owned fade copies; never edits source/vendor materials. */
    UFUNCTION(BlueprintCallable, Category="Editor|Camera Occlusion")
    static bool PrepareMaterial(UMaterial* Material);
};
