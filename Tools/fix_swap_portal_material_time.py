"""Make assigned swap portals animate through swap slowdown; preserve their tuning."""
import unreal
from pathlib import Path
import json

registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
pending = []
systems = set()
for character in ['Ninja', 'Samurai']:
    bp = unreal.load_asset('/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_' + character)
    component = unreal.get_default_object(bp.generated_class()).get_editor_property('swap_presentation')
    for slot in ['arrival_portal', 'departure_portal']:
        system = component.get_editor_property(slot)
        if system:
            pending.append(system.get_path_name().split('.')[0])
            systems.add(system.get_path_name())

visited = set()
report = []
for path in sorted(systems):
    system = unreal.load_asset(path)
    changed = unreal.SwapVFXSetupLibrary.prepare_portal_local_space(system)
    unreal.log('PORTAL_LOCAL_SPACE ' + path + ': converted ' + str(changed) + ' emitters')
    if changed:
        assert unreal.EditorAssetLibrary.save_loaded_asset(system, False)
        report.append({'asset': path, 'local_space_emitters': changed})
while pending:
    package = str(pending.pop())
    if package in visited or not package.startswith('/Game/'):
        continue
    visited.add(package)
    pending.extend(registry.get_dependencies(package, unreal.AssetRegistryDependencyOptions()))
    asset = unreal.load_asset(package)
    lib = unreal.MaterialEditingLibrary
    if isinstance(asset, unreal.Material):
        expressions = lib.get_material_expressions(asset)
        create = lambda: lib.create_material_expression(asset, unreal.MaterialExpressionTime)
    elif isinstance(asset, unreal.MaterialFunction):
        expressions = lib.get_material_function_expressions(asset)
        create = lambda: lib.create_material_expression_in_function(asset, unreal.MaterialExpressionTime)
    else:
        continue
    changes = []
    for expression in expressions:
        if isinstance(expression, unreal.MaterialExpressionTime):
            if not expression.get_editor_property('ignore_pause'):
                expression.set_editor_property('ignore_pause', True)
                changes.append('Time: game clock -> real clock')
        elif isinstance(expression, (unreal.MaterialExpressionPanner, unreal.MaterialExpressionRotator)):
            if not unreal.SwapVFXSetupLibrary.is_material_input_connected(expression, 'Time'):
                clock = create()
                clock.set_editor_property('ignore_pause', True)
                assert lib.connect_material_expressions(clock, '', expression, 'Time')
                changes.append(expression.get_class().get_name() + ': implicit game clock -> real clock')
    if changes:
        if isinstance(asset, unreal.Material):
            lib.recompile_material(asset)
        else:
            lib.update_material_function(asset)
        assert unreal.EditorAssetLibrary.save_loaded_asset(asset, False), package
        report.append({'asset': package, 'changes': changes})
        unreal.log('PORTAL_REAL_TIME ' + package + ' ' + str(changes))
out = Path(unreal.Paths.project_saved_dir()) / 'SwapPortalMaterialTime.json'
out.write_text(json.dumps(report, indent=2))
unreal.log('PORTAL_REAL_TIME_COMPLETE ' + str(len(report)))
