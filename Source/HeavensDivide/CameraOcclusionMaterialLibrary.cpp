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
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExpressionObjectPositionWS.h"
#include "Materials/MaterialExpressionObjectBounds.h"
#endif

bool UCameraOcclusionMaterialLibrary::PrepareMaterial(UMaterial* Material)
{
#if WITH_EDITOR
    if (!Material || !Material->GetPathName().StartsWith(TEXT("/Game/HeavensDivide/Environment/OldVillage/CameraFade/"))) return false;
    if (Material->BlendMode != BLEND_Opaque && Material->BlendMode != BLEND_Masked) return false;
    UMaterialExpressionScalarParameter* ExistingFade = nullptr;
    for (UMaterialExpression* Expression : Material->GetExpressions())
        if (auto* Parameter = Cast<UMaterialExpressionScalarParameter>(Expression))
        {
            if (Parameter->ParameterName == TEXT("HD_CameraOcclusion")) ExistingFade = Parameter;
            else if (Parameter->bUseCustomPrimitiveData && Parameter->PrimitiveDataIndex == UCameraOcclusionComponent::FadeDataIndex) return false;
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
    auto ApplyHalfMask = [&](UMaterialExpressionMultiply* Strength)
    {
        auto* Position = NewObject<UMaterialExpressionWorldPosition>(Material); Add(Position);
        Position->WorldPositionShaderOffset = WPT_ExcludeAllShaderOffsets;
        auto* Center = NewObject<UMaterialExpressionObjectPositionWS>(Material); Add(Center);
        auto* Bounds = NewObject<UMaterialExpressionObjectBounds>(Material); Add(Bounds);
        auto* Half = NewObject<UMaterialExpressionCustom>(Material);
        Half->Description = TEXT("HD_UpperHalfOcclusion");
        Half->OutputType = CMOT_Float1;
        Half->Code = TEXT("return 0.96 * saturate((Position.z - Center.z) / max(Bounds.z * 0.12, 1.0));");
        Half->Inputs.SetNum(3);
        Half->Inputs[0].InputName=TEXT("Position"); Half->Inputs[0].Input.Connect(0,Position);
        Half->Inputs[1].InputName=TEXT("Center"); Half->Inputs[1].Input.Connect(0,Center);
        Half->Inputs[2].InputName=TEXT("Bounds"); Half->Inputs[2].Input.Connect(0,Bounds);
        Add(Half); Strength->B.Connect(0,Half);
    };
    if (ExistingFade)
    {
        // Migrate the existing mask in place; keep the vendor leaf alpha and
        // material-attributes wiring exactly as authored.
        for (UMaterialExpression* Expression : Material->GetExpressions())
            if (auto* Strength = Cast<UMaterialExpressionMultiply>(Expression))
                if (Strength->A.Expression == ExistingFade)
                {
                    if (auto* Mask = Cast<UMaterialExpressionCustom>(Strength->B.Expression))
                        if (Mask->Description == TEXT("HD_UpperHalfOcclusion")) return true;
                    ApplyHalfMask(Strength);
                    Material->PostEditChange(); Material->MarkPackageDirty(); return true;
                }
        return false;
    }
    auto* Fade = NewObject<UMaterialExpressionScalarParameter>(Material);
    Fade->ParameterName = TEXT("HD_CameraOcclusion");
    Fade->DefaultValue = 0.f;
    Fade->bUseCustomPrimitiveData = true;
    Fade->PrimitiveDataIndex = UCameraOcclusionComponent::FadeDataIndex;
    Add(Fade);
    auto* Strength = NewObject<UMaterialExpressionMultiply>(Material);
    Strength->A.Connect(0, Fade); Add(Strength); ApplyHalfMask(Strength);
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
