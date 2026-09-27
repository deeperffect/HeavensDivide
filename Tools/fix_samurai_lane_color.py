"""Disable saved strike debug outlines and inspect authored lane color overrides."""
from pathlib import Path
import json
import shutil
import unreal as u

root = Path(u.Paths.project_dir()).resolve()
path = '/Game/HeavensDivide/Blueprints/Objectives/BP_SamuraiTechniqueTrial'
source = root / 'Content' / (path.removeprefix('/Game/') + '.uasset')
backup = root / 'Saved/Backups/SamuraiLaneColor' / source.name
backup.parent.mkdir(parents=True, exist_ok=True)
if not backup.exists():
    shutil.copy2(source, backup)
bp = u.load_asset(path)
defaults = u.get_default_object(bp.generated_class())
material = defaults.get_editor_property('lane_indicator_material')
report = {'assigned_material': material.get_path_name(),
          'debug_boxes_before': defaults.get_editor_property('draw_strike_damage_debug_boxes')}
if isinstance(material, u.MaterialInstanceConstant):
    report['vector_overrides'] = str(material.get_editor_property('vector_parameter_values'))
    report['resolved_fill_color'] = str(u.MaterialEditingLibrary.get_material_instance_vector_parameter_value(material, 'FillColor'))
parent = u.load_asset('/Game/HeavensDivide/Materials/M_SamuraiLaneIndicator')
report['parent_fill_color'] = str(u.MaterialEditingLibrary.get_material_default_vector_parameter_value(parent, 'FillColor'))
defaults.set_editor_property('draw_strike_damage_debug_boxes', False)
plane = u.load_asset('/Engine/BasicShapes/Plane')
assert plane
subsystem = u.get_engine_subsystem(u.SubobjectDataSubsystem)
lanes = []
for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
    data = u.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
    component = u.SubobjectDataBlueprintFunctionLibrary.get_object_for_blueprint(data, bp)
    if isinstance(component, u.StaticMeshComponent) and component.get_name() in ['LeftLane', 'CenterLane', 'RightLane']:
        component.set_static_mesh(plane)
        lanes.append(component.get_name())
assert len(lanes) == 3, lanes
report['flat_lane_surfaces'] = lanes
u.BlueprintEditorLibrary.compile_blueprint(bp)
assert u.EditorAssetLibrary.save_loaded_asset(bp, False)
assert not u.get_default_object(bp.generated_class()).get_editor_property('draw_strike_damage_debug_boxes')
out = root / 'Saved/SamuraiLaneIndicator/color_fix.json'
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(report, indent=2))
u.log('SAMURAI_LANE_COLOR_FIX ' + json.dumps(report))
