#include "CameraOcclusionMaterialLibrary.h"
#include "CameraOcclusionComponent.h"
#if WITH_EDITOR
#include "Materials/Material.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionGetMaterialAttributes.h"
#include "Materials/MaterialExpressionSetMaterialAttributes.h"
#include "Materials/MaterialFunction.h"
#endif

bool UCameraOcclusionMaterialLibrary::PrepareMaterial(UMaterial* Material)
{
#if WITH_EDITOR
    if (!Material || !Material->GetPathName().StartsWith(TEXT("/Game/HeavensDivide/Environment/OldVillage/CameraFade/"))) return false;
    if (Material->BlendMode != BLEND_Opaque && Material->BlendMode != BLEND_Masked) return false;
    for (UMaterialExpression* Expression : Material->GetExpressions())
        if (auto* Parameter = Cast<UMaterialExpressionScalarParameter>(Expression))
        {
            if (Parameter->ParameterName == TEXT("HD_CameraOcclusion")) return true;
            if (Parameter->bUseCustomPrimitiveData && Parameter->PrimitiveDataIndex == UCameraOcclusionComponent::FadeDataIndex) return false;
        }
    auto* Function = LoadObject<UMaterialFunction>(nullptr, TEXT("/Engine/Functions/Engine_MaterialFunctions02/Utility/DitherTemporalAA.DitherTemporalAA"));
    if (!Function) return false;
    Material->Modify();
    auto Add = [Material](UMaterialExpression* Expression)
    {
        Material->GetExpressionCollection().AddExpression(Expression);
        Expression->Material = Material;
        Expression->UpdateMaterialExpressionGuid(true, false);
    };
    auto* Fade = NewObject<UMaterialExpressionScalarParameter>(Material);
    Fade->ParameterName = TEXT("HD_CameraOcclusion");
    Fade->DefaultValue = 0.f;
    Fade->bUseCustomPrimitiveData = true;
    Fade->PrimitiveDataIndex = UCameraOcclusionComponent::FadeDataIndex;
    Add(Fade);
    auto* Strength = NewObject<UMaterialExpressionMultiply>(Material);
    Strength->A.Connect(0, Fade); Strength->ConstB = 0.92f; Add(Strength);
    auto* Opacity = NewObject<UMaterialExpressionOneMinus>(Material);
    Opacity->Input.Connect(0, Strength); Add(Opacity);
    auto* Dither = NewObject<UMaterialExpressionMaterialFunctionCall>(Material);
    Dither->SetMaterialFunction(Function);
    if (Dither->FunctionInputs.IsEmpty()) return false;
    Dither->FunctionInputs[0].Input.Connect(0, Opacity); Add(Dither);
    auto* Mask = NewObject<UMaterialExpressionMultiply>(Material);
    Mask->A.Connect(0, Dither); Mask->ConstB = 1.f; Add(Mask);
    auto* Data = Material->GetEditorOnlyData();
    if (Material->bUseMaterialAttributes)
    {
        auto* Get = NewObject<UMaterialExpressionGetMaterialAttributes>(Material);
        Get->MaterialAttributes = Data->MaterialAttributes;
        const int32 MaskOutput = Get->CreateOrGetOutputAttribute(MP_OpacityMask); Add(Get);
        Mask->B.Connect(MaskOutput, Get);
        auto* Set = NewObject<UMaterialExpressionSetMaterialAttributes>(Material);
        Set->Inputs[0] = Data->MaterialAttributes;
        Set->ConnectInputAttribute(MP_OpacityMask, Mask); Add(Set);
        Data->MaterialAttributes.Connect(0, Set);
    }
    else
    {
        if (Material->BlendMode == BLEND_Masked && Data->OpacityMask.Expression)
            Mask->B = Data->OpacityMask;
        else if (Material->BlendMode == BLEND_Masked && Data->OpacityMask.UseConstant)
            Mask->ConstB = Data->OpacityMask.Constant;
        Data->OpacityMask.Connect(0, Mask);
        Data->OpacityMask.UseConstant = false;
    }
    Material->BlendMode = BLEND_Masked;
    Material->PostEditChange();
    Material->MarkPackageDirty();
    return true;
#else
    return false;
#endif
}
