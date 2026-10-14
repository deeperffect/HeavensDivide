"""Read-only checks of all ten saved preset assets and Level1 scene."""
from pathlib import Path
import hashlib,json
import unreal as u
ROOT=Path(u.Paths.project_dir()).resolve()
CONFIG=json.loads((ROOT/'Art/ArenaVariations/Variants.json').read_text())
FOLDER='/Game/HeavensDivide/Materials/ArenaVariations'
LIB=u.MaterialEditingLibrary
ACTORS=u.get_editor_subsystem(u.EditorActorSubsystem)
actors=ACTORS.get_all_level_actors()
floor=next(a for a in actors if a.get_actor_label()=='Plane' and isinstance(a,u.StaticMeshActor))
assert abs(floor.get_actor_scale3d().x*100-15000)<.01
assert abs(floor.get_actor_scale3d().y*100-15000)<.01
records=[]
for v in CONFIG['variants']:
    ident=v['id'];mat=u.load_asset(FOLDER+'/MI_'+ident);rim=u.load_asset(FOLDER+'/MI_Rim_'+ident)
    texture=u.load_asset(FOLDER+'/T_'+ident)
    assert mat and rim and texture
    assert mat.get_base_material().get_name()=='M_ArenaSurface'
    assert rim.get_base_material().get_name()=='M_ArenaRim'
    assert LIB.get_material_instance_texture_parameter_value(mat,'GroundTexture')==texture
    assert LIB.get_material_instance_texture_parameter_value(rim,'GroundTexture')==texture
    tile=LIB.get_material_instance_scalar_parameter_value(mat,'TileSize')
    assert abs(tile-v['tile_cm'])<.01
    assert texture.get_editor_property('srgb') and not texture.get_editor_property('never_stream')
    assert texture.get_editor_property('address_x')==u.TextureAddress.TA_CLAMP
    assert texture.get_editor_property('address_y')==u.TextureAddress.TA_CLAMP
    assert texture.get_editor_property('mip_gen_settings')==u.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP
    assert mat.get_base_material().get_editor_property('shading_model')==u.MaterialShadingModel.MSM_DEFAULT_LIT
    records.append(dict(id=ident,floor=mat.get_path_name(),rim=rim.get_path_name(),texture=texture.get_path_name(),tile_cm=tile,platform_size=[texture.blueprint_get_size_x(),texture.blueprint_get_size_y()],streaming=True,mipmaps=True))
slabs=next(a for a in actors if a.get_actor_label()=='GroundRockStamps')
for comp in slabs.get_components_by_class(u.MeshComponent):
    assert not comp.get_editor_property('visible') and comp.get_editor_property('hidden_in_game')
rim=next(a for a in actors if a.get_actor_label()=='TerracottaArenaRim')
counts=[]
for comp in rim.get_components_by_class(u.HierarchicalInstancedStaticMeshComponent):
    assert comp.get_collision_enabled()==u.CollisionEnabled.NO_COLLISION
    assert not comp.get_editor_property('can_ever_affect_navigation')
    assert comp.get_material(0).get_name().startswith('MI_Rim_')
    counts.append(comp.get_instance_count())
assert len(counts)==3 and sum(counts)==276,counts
fences=[c for a in actors for c in a.get_components_by_class(u.StaticMeshComponent) if c.static_mesh and c.static_mesh.get_name()=='SM_ShrineFence03a']
assert len(fences)==262 and all(not c.get_editor_property('visible') for c in fences)
applied=json.loads((ROOT/'Saved/ArenaVariations/Applied.json').read_text())
assert all(hashlib.sha256((ROOT/p).read_bytes()).hexdigest()==value for p,value in applied['protected_assets'].items())
assert str(floor.static_mesh_component.get_collision_profile_name())==applied['floor_collision']
REPORT=dict(passed=True,variants=records,selected=floor.static_mesh_component.get_material(0).get_path_name(),rim_instance_counts=counts,oversized_stamps_hidden=True,fences_hidden=len(fences),floor_collision=applied['floor_collision'],protected_assets_unchanged=len(applied['protected_assets']))
