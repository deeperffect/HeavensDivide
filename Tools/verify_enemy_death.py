"""Validate migrated enemy material shaders and inherited component defaults in Unreal."""
from pathlib import Path
import unreal
M = unreal.MaterialEditingLibrary
manifest = Path(unreal.Paths.project_saved_dir(), 'enemy_death_migration.txt').read_text().splitlines()
count = 0
for path in manifest:
    if not path.startswith('/Game/Assets/EnemyCharacters/'): continue
    material = unreal.load_asset(path)
    assert material, path
    expressions = M.get_material_expressions(material)
    assert any(isinstance(e, unreal.MaterialExpressionScalarParameter) and str(e.get_editor_property('parameter_name')) == 'DissolveAmount' for e in expressions), path
    for expression in expressions:
        if isinstance(expression, unreal.MaterialExpressionMaterialFunctionCall):
            function = expression.get_editor_property('material_function')
            assert not function or 'Vertex_Collapse' not in function.get_path_name(), path
    errors = M.recompile_material(material)
    assert not errors, (path, errors)
    count += 1
component = unreal.get_default_object(unreal.EnemyDeathComponent)
assert component.get_editor_property('death_niagara_system')
assert component.get_editor_property('death_sound')
assert abs(component.get_editor_property('dissolve_duration') - 0.25) < 0.001
boss = unreal.load_asset('/Game/HeavensDivide/Blueprints/EnemyCharacters/Bosses/SamuraiBoss/BP_SamuraiBoss')
assert unreal.get_default_object(boss.generated_class()).get_editor_property('death_montage')
assert count == 12, count
unreal.log('ENEMY_DEATH_VALIDATION_OK: 12 materials compiled; component defaults and boss montage preserved')
