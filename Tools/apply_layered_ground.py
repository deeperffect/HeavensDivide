"""Import the rock atlas and bake one static instanced slab actor into Level 1.

Run in Unreal Python with the interactive editor closed. Rebuild placement with
generate_layered_ground.py first. Replaces only the tagged pass-owned actor;
the map and every replaced material are backed up before editing.
"""
from datetime import datetime
from pathlib import Path
import hashlib
import json
import runpy
import shutil
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve()
OUT = ROOT / 'Saved/LayeredGround'
OUT.mkdir(parents=True, exist_ok=True)
BACKUP = OUT / 'Backups' / datetime.now().strftime('%Y%m%d_%H%M%S_%f')
FOLDER = '/Game/HeavensDivide/Materials/LayeredGround'
GROUND = FOLDER
MAP = '/Game/Maps/Lvl_B1_Lvl1'
TAG = 'GroundRockStamps'
LIB = u.MaterialEditingLibrary
ASSETS = u.AssetToolsHelpers.get_asset_tools()
ACTORS = u.get_editor_subsystem(u.EditorActorSubsystem)
LEVEL = u.get_editor_subsystem(u.LevelEditorSubsystem)


def disk(path, extension='.uasset'):
    return ROOT / 'Content' / (path.removeprefix('/Game/').split('.')[0] + extension)


def backup(path, extension='.uasset'):
    source = disk(path, extension)
    if source.exists():
        dest = BACKUP / source.relative_to(ROOT)
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, dest)


def put(obj, **values):
    obj.modify(True)
    for key, value in values.items():
        obj.set_editor_property(key, value)


def save(obj):
    assert u.EditorAssetLibrary.save_loaded_asset(obj, False), obj.get_path_name()


def scalar(material, name, value, y):
    node = LIB.create_material_expression(material, u.MaterialExpressionScalarParameter, -600, y)
    put(node, parameter_name=name, default_value=value)
    return node


def vector(material, name, value, y):
    node = LIB.create_material_expression(material, u.MaterialExpressionVectorParameter, -600, y)
    put(node, parameter_name=name, default_value=u.LinearColor(*value))
    return node


layout = json.loads((ROOT / 'Art/LayeredGround/RockScatterLayout.json').read_text())
config_path = ROOT / 'Tools/layered_ground.json'
config = json.loads(config_path.read_text())
assert hashlib.sha256(config_path.read_bytes()).hexdigest() == layout['config_sha256'], 'Rebuild the scatter after changing its configuration'
assert hashlib.sha256((ROOT / config['mask']).read_bytes()).hexdigest() == layout['mask_sha256'], 'Rebuild the scatter after changing the ground layout'
placements = layout['placements']
region_data = json.loads((ROOT / 'Art/LayeredGround/RockAtlasRegions.json').read_text())
assert hashlib.sha256((ROOT / 'Art/LayeredGround/RockAtlasRegions.json').read_bytes()).hexdigest() == layout['atlas_regions_sha256'], 'Rebuild the scatter after changing atlas crops'
for a in region_data['atlases']:
    assert hashlib.sha256((ROOT/a['source']).read_bytes()).hexdigest() == a['sha256'], 'Atlas metadata is stale'
regions = region_data['variants']
assert len(regions) == 8
assert placements and {int(p['variant']) for p in placements} == set(range(8))
assert all(abs(p['z'] - 1.3) < .001 for p in placements)
backup(MAP, '.umap')
for name in ['M_Lvl1_Ground', 'MI_Lvl1_Ground']:
    backup(GROUND + '/' + name)
protected = [p for p in (ROOT/'Content/Maps').glob('*.umap') if p.name != 'Lvl_B1_Lvl1.umap']
for folder in ['EnvironmentPolish','ReferenceGround','StylizedGround','TerracottaArena','ArenaVariations']:
    protected += list((ROOT/'Content/HeavensDivide/Materials'/folder).glob('*.uasset'))
for p in (ROOT/'Content/HeavensDivide/Materials/LayeredGround').glob('*.uasset'):
    backup('/Game/'+str(p.relative_to(ROOT/'Content').with_suffix('')).replace('\\','/'))
hashes = {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest() for path in protected}

# Build the owned material copy; the earlier restored assets remain unchanged.
runpy.run_path(str(ROOT / 'Tools/build_layered_ground_material.py'))

atlases=[]
for index,a in enumerate(region_data['atlases']):
    task=u.AssetImportTask()
    for key,value in dict(filename=str(ROOT/a['source']),destination_path=FOLDER,
                          destination_name='T_RockAtlas_'+str(index),automated=True,replace_existing=True,save=False).items():
        task.set_editor_property(key,value)
    ASSETS.import_asset_tasks([task])
    atlas=u.load_asset(FOLDER+'/T_RockAtlas_'+str(index))
    assert atlas
    put(atlas,srgb=True,compression_settings=u.TextureCompressionSettings.TC_DEFAULT,
        compression_no_alpha=False,address_x=u.TextureAddress.TA_CLAMP,address_y=u.TextureAddress.TA_CLAMP,
        lod_group=u.TextureGroup.TEXTUREGROUP_WORLD,power_of_two_mode=u.TexturePowerOfTwoSetting.STRETCH_TO_POWER_OF_TWO,
        max_texture_size=2048,mip_gen_settings=u.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP,never_stream=False)
    save(atlas)
    atlases.append(atlas)
atlas=atlases[0]

parent_path = FOLDER + '/M_RockStamp'
backup(parent_path)
parent = u.load_asset(parent_path) if u.EditorAssetLibrary.does_asset_exist(parent_path) else ASSETS.create_asset(
    'M_RockStamp', FOLDER, u.Material, u.MaterialFactoryNew())
assert parent
LIB.delete_all_material_expressions(parent)
put(parent, material_domain=u.MaterialDomain.MD_SURFACE, blend_mode=u.BlendMode.BLEND_MASKED,
    shading_model=u.MaterialShadingModel.MSM_DEFAULT_LIT, two_sided=False, opacity_mask_clip_value=.5)
uv = LIB.create_material_expression(parent, u.MaterialExpressionTextureCoordinate, -600, -200)
texture = LIB.create_material_expression(parent, u.MaterialExpressionTextureSampleParameter2D, -100, 0)
put(texture, parameter_name='RockAtlas', texture=atlas, sampler_type=u.MaterialSamplerType.SAMPLERTYPE_COLOR)
offset = vector(parent, 'AtlasUVRect', regions[0]['normalizedUVRect'], 180)
tint = vector(parent, 'StoneTint', (.88, .96, 1.08, 1), 360)
brightness = scalar(parent, 'Brightness', .45, 540)
saturation = scalar(parent, 'Saturation', .90, 720)
# Native sample/UV nodes expose atlas scale to Unreal's texture-streaming analysis.
# A Custom node with Texture2DSample rendered sharply after import, but the fresh
# editor loaded only coarse mips because that sampling was invisible to streaming.
origin = LIB.create_material_expression(parent, u.MaterialExpressionComponentMask, -400, 150)
span = LIB.create_material_expression(parent, u.MaterialExpressionComponentMask, -400, 300)
put(origin, r=True, g=True, b=False, a=False)
put(span, r=False, g=False, b=True, a=True)
for mask in (origin, span):
    assert LIB.connect_material_expressions(offset, 'RGBA', mask, '')
scaled_uv = LIB.create_material_expression(parent, u.MaterialExpressionMultiply, -250, -200)
atlas_uv = LIB.create_material_expression(parent, u.MaterialExpressionAdd, -100, -200)
assert LIB.connect_material_expressions(uv, '', scaled_uv, 'A')
assert LIB.connect_material_expressions(span, '', scaled_uv, 'B')
assert LIB.connect_material_expressions(scaled_uv, '', atlas_uv, 'A')
assert LIB.connect_material_expressions(origin, '', atlas_uv, 'B')
assert LIB.connect_material_expressions(atlas_uv, '', texture, '')
tinted = LIB.create_material_expression(parent, u.MaterialExpressionMultiply, 150, 0)
assert LIB.connect_material_expressions(texture, 'RGB', tinted, 'A')
assert LIB.connect_material_expressions(tint, 'RGB', tinted, 'B')
desaturation = LIB.create_material_expression(parent, u.MaterialExpressionDesaturation, 350, 0)
inverse_saturation = LIB.create_material_expression(parent, u.MaterialExpressionOneMinus, 150, 180)
put(desaturation, luminance_factors=u.LinearColor(.2126, .7152, .0722, 0.0))
assert LIB.connect_material_expressions(saturation, '', inverse_saturation, '')
assert LIB.connect_material_expressions(tinted, '', desaturation, '')
assert LIB.connect_material_expressions(inverse_saturation, '', desaturation, 'Fraction')
graded = LIB.create_material_expression(parent, u.MaterialExpressionMultiply, 550, 0)
assert LIB.connect_material_expressions(desaturation, '', graded, 'A')
assert LIB.connect_material_expressions(brightness, '', graded, 'B')
assert LIB.connect_material_property(graded, '', u.MaterialProperty.MP_BASE_COLOR)
assert LIB.connect_material_property(texture, 'A', u.MaterialProperty.MP_OPACITY_MASK)
for value, prop, y in [(1.0, u.MaterialProperty.MP_ROUGHNESS, 400), (.10, u.MaterialProperty.MP_SPECULAR, 550)]:
    node = LIB.create_material_expression(parent, u.MaterialExpressionConstant, 180, y)
    put(node, r=value)
    assert LIB.connect_material_property(node, '', prop)
LIB.set_base_material_usage(parent, u.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES, True)
LIB.recompile_material(parent)
save(parent)
materials = []
for index in range(8):
    name = 'MI_RockStamp_' + str(index)
    path = FOLDER + '/' + name
    backup(path)
    instance = u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else ASSETS.create_asset(
        name, FOLDER, u.MaterialInstanceConstant, u.MaterialInstanceConstantFactoryNew())
    LIB.set_material_instance_parent(instance, parent)
    LIB.set_material_instance_texture_parameter_value(instance,'RockAtlas',atlases[regions[index]['atlas']])
    LIB.set_material_instance_vector_parameter_value(instance, 'AtlasUVRect', u.LinearColor(*regions[index]['normalizedUVRect']))
    LIB.update_material_instance(instance)
    save(instance)
    materials.append(instance)

# Rebuild UV density for the eight crop overrides as well as the parent.
# This editor command analyzes loaded materials but does not save unrelated assets.
world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
u.SystemLibrary.execute_console_command(world, 'BuildMaterialTextureStreamingData')
for material in [parent] + materials:
    save(material)

assert LEVEL.load_level(MAP)
assert LEVEL.set_current_level_by_name('Lvl_B1_Lvl1')
scene_before = runpy.run_path(str(ROOT/'Tools/inspect_layered_ground.py'),init_globals={'SNAPSHOT_ONLY':True})['REPORT']
old_actors = [a for a in ACTORS.get_all_level_actors() if a.actor_has_tag(TAG)]
assert all(a.get_actor_label() == TAG for a in old_actors), 'Unexpected actor uses the rock pass tag'
unchanged_actors = {a.get_name() for a in ACTORS.get_all_level_actors() if a not in old_actors}
plane = u.load_asset('/Engine/BasicShapes/Plane.Plane')
assert plane
extent = plane.get_bounds().box_extent
assert abs(extent.x - 50) < .01 and abs(extent.y - 50) < .01, 'Expected a 100cm plane'
SDS = u.get_engine_subsystem(u.SubobjectDataSubsystem)
SD = u.SubobjectDataBlueprintFunctionLibrary
counts = []
with u.ScopedEditorTransaction('Scatter painted ground rock slabs'):
    for actor in old_actors:
        assert ACTORS.destroy_actor(actor)
    actor = ACTORS.spawn_actor_from_class(u.Actor, u.Vector(0, 0, 0), transient=False)
    assert actor
    actor.set_actor_label(TAG)
    actor.set_folder_path('Environment/Ground Details')
    actor.set_actor_tick_enabled(False)
    put(actor, tags=[u.Name(TAG)], can_be_damaged=False)
    handles = SDS.k2_gather_subobject_data_for_instance(actor)
    actor_handle = next(h for h in handles if SD.is_actor(SD.get_data(h)))
    root_handle, error = SDS.add_new_subobject(u.AddNewSubobjectParams(
        parent_handle=actor_handle, new_class=u.SceneComponent, blueprint_context=None))
    assert not str(error), str(error)
    scene_root = SD.get_associated_object(SD.get_data(root_handle))
    put(scene_root, mobility=u.ComponentMobility.STATIC)
    for variant in range(8):
        handle, error = SDS.add_new_subobject(u.AddNewSubobjectParams(
            parent_handle=root_handle, new_class=u.HierarchicalInstancedStaticMeshComponent, blueprint_context=None))
        assert not str(error), str(error)
        component = SD.get_associated_object(SD.get_data(handle))
        assert isinstance(component, u.HierarchicalInstancedStaticMeshComponent)
        component.set_static_mesh(plane)
        component.set_material(0, materials[variant])
        component.set_collision_profile_name('NoCollision')
        component.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
        component.set_editor_property('generate_overlap_events', False)
        component.set_component_tick_enabled(False)
        put(component, mobility=u.ComponentMobility.STATIC, cast_shadow=False,
            can_ever_affect_navigation=False, affect_distance_field_lighting=False,
            visible_in_ray_tracing=False)
        # The editor's fallback streaming data uses the 100cm source plane's
        # texel density; preserve detail on the much larger baked instances.
        variant_placements = [p for p in placements if int(p['variant']) == variant]
        put(component, streaming_distance_multiplier=max(max(p['width'], p['height']) for p in variant_placements) / 100.0)
        transforms = [u.Transform(location=u.Vector(p['x'], p['y'], p['z']),
                                   rotation=u.Rotator(pitch=0, yaw=p['yaw'], roll=0),
                                   scale=u.Vector(p['width'] / 100.0, p['height'] / 100.0, 1.0))
                      for p in variant_placements]
        indices = component.add_instances(transforms, True, True, False)
        assert len(indices) == len(transforms) == component.get_instance_count()
        counts.append(dict(variant=variant, count=len(indices), component=component.get_name(),
                           material=materials[variant].get_path_name()))
    assert len(actor.get_components_by_class(u.HierarchicalInstancedStaticMeshComponent)) == 8
    current_names = {a.get_name() for a in ACTORS.get_all_level_actors() if a != actor}
    assert unchanged_actors == current_names, 'An unrelated scene actor changed'
    ground = u.load_asset(GROUND + '/MI_Lvl1_Ground')
    LIB.set_material_instance_scalar_parameter_value(ground, 'PlacedRockStamps', 1.0)
    LIB.update_material_instance(ground)
    assert LIB.get_material_instance_scalar_parameter_value(ground, 'PlacedRockStamps') == 1.0
    save(ground)
    floor=next(a for a in ACTORS.get_all_level_actors() if a.get_actor_label()=='Plane')
    floor.modify(True)
    floor.static_mesh_component.modify(True)
    floor.static_mesh_component.set_material(0,ground)
    scene_after=runpy.run_path(str(ROOT/'Tools/inspect_layered_ground.py'),init_globals={'SNAPSHOT_ONLY':True})['REPORT']
    assert scene_before==scene_after, 'Unrelated scene presentation/transform/collision changed'
    assert LEVEL.save_current_level()

assert hashes == {relative: hashlib.sha256((ROOT / relative).read_bytes()).hexdigest() for relative in hashes}
report = dict(passed=True, backup=str(BACKUP), map=MAP, actor=actor.get_path_name(),
              instance_count=len(placements), components=counts, atlases=[a.get_path_name() for a in atlases],
              parent=parent.get_path_name(), height=1.3, collision='NoCollision', navigation=False,
              casts_shadows=False, seed=layout.get('seed'), source='Art/LayeredGround/RockScatterLayout.json',
              protected_assets=hashes, scene_snapshot=scene_before)
(OUT / 'Applied.json').write_text(json.dumps(report, indent=2))
u.log('LAYERED_GROUND_APPLY_OK ' + str(OUT / 'Applied.json'))
