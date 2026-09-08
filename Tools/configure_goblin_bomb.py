"""Configure Goblin Bomb using the Floating Skeleton's existing AoE telegraph."""
from pathlib import Path
import shutil
import unreal

path = '/Game/HeavensDivide/Blueprints/EnemyCharacters/Mobs/BP_EnemyGoblinBomb'
bp = unreal.load_asset(path)
skeleton = unreal.load_asset('/Game/HeavensDivide/Blueprints/EnemyCharacters/Mobs/BP_EnemyFloatingSkeleton')
assert bp and skeleton
source = unreal.get_default_object(skeleton.generated_class())
sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
def bomb_meshes():
    result = {}
    for h in sub.k2_gather_subobject_data_for_blueprint(bp):
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(h)
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object_for_blueprint(data, bp)
        if isinstance(obj, unreal.StaticMeshComponent):
            result[obj.get_name()] = tuple(str(obj.get_editor_property(p)) for p in
                ['static_mesh', 'relative_location', 'relative_rotation', 'relative_scale3d'])
    return result
before = bomb_meshes()
assert any('Bomb' in key for key in before), before
content = Path(unreal.Paths.project_content_dir()).resolve()
rel = Path(path.removeprefix('/Game/') + '.uasset')
backup = Path(unreal.Paths.project_saved_dir()).resolve() / 'Backups/GoblinBombAttack' / rel
backup.parent.mkdir(parents=True, exist_ok=True)
if not backup.exists(): shutil.copy2(content / rel, backup)
unreal.BlueprintEditorLibrary.reparent_blueprint(bp, unreal.GoblinBombEnemy)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
cdo = unreal.get_default_object(bp.generated_class())
assert isinstance(cdo, unreal.GoblinBombEnemy)
cdo.set_editor_property('attack_montage', None)
cdo.set_editor_property('attack_shape', unreal.TankSlamAttackShape.CIRCLE)
cdo.set_editor_property('attack_telegraph_material', source.get_editor_property('attack_telegraph_material'))
cdo.set_editor_property('attack_ao_e_radius', source.get_editor_property('attack_ao_e_radius'))
cdo.set_editor_property('telegraph_windup_duration', source.get_editor_property('telegraph_windup_duration'))
cdo.set_editor_property('attack_range', 250.0)
cdo.set_editor_property('windup_tracking_rotation_speed', 0.0)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert bomb_meshes() == before, 'Attached Bomb mesh/transform changed during reparenting'
assert unreal.EditorAssetLibrary.save_loaded_asset(bp, False)
unreal.log('GOBLIN_BOMB_SETUP_OK: bomb mesh preserved; trigger=250; shared circle radius=' + str(cdo.get_editor_property('attack_ao_e_radius')))
