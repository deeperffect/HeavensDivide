#include "EnemyDeathMigrationLibrary.h"
#if WITH_EDITOR
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialFunctionInterface.h"
#endif

void UEnemyDeathMigrationLibrary::RemoveLegacyCollapseNodes(UMaterial* Material)
{
#if WITH_EDITOR
	if (!Material || !Material->GetPathName().StartsWith(TEXT("/Game/Assets/EnemyCharacters/"))) return;
	const TArray<UMaterialExpression*> Expressions = UMaterialEditingLibrary::GetMaterialExpressions(Material);
	for (UMaterialExpression* Expression : Expressions)
	{
		bool bRemove = false;
		if (const auto* Scalar = Cast<UMaterialExpressionScalarParameter>(Expression))
			bRemove = Scalar->ParameterName.ToString().StartsWith(TEXT("Collapse"));
		if (const auto* Vector = Cast<UMaterialExpressionVectorParameter>(Expression))
			bRemove = Vector->ParameterName.ToString().StartsWith(TEXT("Collapse"));
		if (const auto* Function = Cast<UMaterialExpressionMaterialFunctionCall>(Expression))
			bRemove = Function->MaterialFunction && Function->MaterialFunction->GetPathName().Contains(TEXT("Vertex_Collapse"));
		if (bRemove && Expression->GetOuter() == Material)
		{
			// Startup-loaded expressions can be rooted. The material and local array keep
			// them alive until DeleteMaterialExpression unlinks and marks them as garbage.
			if (Expression->IsRooted()) Expression->RemoveFromRoot();
			UMaterialEditingLibrary::DeleteMaterialExpression(Material, Expression);
		}
	}
#endif
}
