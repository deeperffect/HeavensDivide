"""Repair saved scenery in place, preserving authored actor transforms."""
from pathlib import Path
from datetime import datetime
import json,sys,shutil
import unreal as u
ROOT=Path(u.Paths.project_dir()).resolve();sys.path.insert(0,str(ROOT/'Tools'))
from old_village_districts import ground_actor
from setup_old_village_navigation_camera import prepare_current_level
E=u.EditorAssetLibrary;M=u.MaterialEditingLibrary
ACT=u.get_editor_subsystem(u.EditorActorSubsystem);L=u.get_editor_subsystem(u.LevelEditorSubsystem)
SM=u.get_editor_subsystem(u.StaticMeshEditorSubsystem) or u.get_default_object(u.StaticMeshEditorSubsystem)
DEST='/Game/HeavensDivide/Environment/OldVillage/Polish'
BUSH='/Game/Assets/Environment/Stylized_Village/Assets/Foliage/Bushes/SM_Bush_01'
ROSE='/Game/Assets/Environment/Stylized_Village/Assets/Foliage/Flowers/RosesAndDandelions/SM_Rose_Bush_Large_01'

def simple_mesh(mesh):
    path=DEST+'/Meshes/'+mesh.get_name()
    copy=E.load_asset(path) if E.does_asset_exist(path) else E.duplicate_asset(mesh.get_path_name(),path)
    assert copy
    if SM.get_simple_collision_count(copy)==0:
        assert SM.add_simple_collisions(copy,u.ScriptCollisionShapeType.BOX)>=0
    copy.get_editor_property('body_setup').set_editor_property('collision_trace_flag',u.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    assert E.save_loaded_asset(copy,False)
    return copy

def bamboo_leaves():
    path=DEST+'/Materials/M_BambooLeaves'
    if E.does_asset_exist(path):return E.load_asset(path)
    mat=u.AssetToolsHelpers.get_asset_tools().create_asset('M_BambooLeaves',DEST+'/Materials',u.Material,u.MaterialFactoryNew())
    mat.set_editor_property('blend_mode',u.BlendMode.BLEND_MASKED)
    mat.set_editor_property('two_sided',True)
    color=M.create_material_expression(mat,u.MaterialExpressionConstant3Vector)
    color.set_editor_property('constant',u.LinearColor(.12,.32,.045,1))
    M.connect_material_property(color,'',u.MaterialProperty.MP_BASE_COLOR)
    mask=M.create_material_expression(mat,u.MaterialExpressionTextureSample)
    texture=u.load_asset('/Game/Assets/Environment/JapaneseShrine/Textures/T_Leaf_Tree_Bamboo_01a_O')
    mask.set_editor_property('texture',texture)
    mask.set_editor_property('sampler_type',u.MaterialSamplerType.SAMPLERTYPE_COLOR if texture.get_editor_property('srgb') else u.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    M.connect_material_property(mask,'R',u.MaterialProperty.MP_OPACITY_MASK)
    for prop,value in [(u.MaterialProperty.MP_ROUGHNESS,.9),(u.MaterialProperty.MP_SPECULAR,.1)]:
        node=M.create_material_expression(mat,u.MaterialExpressionConstant);node.set_editor_property('r',value);M.connect_material_property(node,'',prop)
    for prop in ['BaseColor','OpacityMask','Roughness','Specular']:
        u.SwapVFXSetupLibrary.set_property_text(mat.get_editor_property('editor_only_data'),prop,'(UseConstant=False)')
    M.recompile_material(mat);assert E.save_loaded_asset(mat,False)
    return mat

def proxy(actor,radius,height):
    tag='VillageSolidProxy_'+actor.get_name()
    existing=next((a for a in ACT.get_all_level_actors() if a.actor_has_tag(tag)),None)
    if existing:return existing
    center,extent=actor.get_actor_bounds(False)
    a=ACT.spawn_actor_from_class(u.StaticMeshActor,u.Vector(center.x,center.y,center.z-extent.z+height/2))
    a.set_actor_label('Plant trunk collision - '+actor.get_actor_label())
    a.set_folder_path('70_Collision/Planting')
    a.set_editor_property('tags',[tag,'VillageSolidPlantProxy'])
    a.static_mesh_component.set_static_mesh(u.load_asset('/Engine/BasicShapes/Cylinder'))
    a.set_actor_scale3d(u.Vector(radius/50,radius/50,height/100))
    a.static_mesh_component.set_collision_profile_name('OldVillageSolid')
    a.set_actor_hidden_in_game(True);a.static_mesh_component.set_visibility(False)
    return a

def apply_current_level_polish():
    actors=list(ACT.get_all_level_actors());report={'solid_props':[],'bamboo':0,'bushes':0,'gates':0}
    leaves=bamboo_leaves();copies={}
    for a in actors:
        if a.actor_has_tag('OldVillageBackdrop'):continue
        if isinstance(a,u.BossToriiGate):
            a.align_portal_to_gate();report['gates']+=1
            visual=a.get_editor_property('gate_visual');mesh=visual.static_mesh
            if mesh:
                # Keep the arch opening clear; a single box would seal it shut.
                path=DEST+'/Meshes/'+mesh.get_name()
                frame=E.load_asset(path) if E.does_asset_exist(path) else E.duplicate_asset(mesh.get_path_name(),path)
                frame.get_editor_property('body_setup').set_editor_property('collision_trace_flag',u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
                assert E.save_loaded_asset(frame,False)
                visual.set_static_mesh(frame);visual.set_collision_profile_name('OldVillageSolid')
        if not isinstance(a,u.StaticMeshActor):continue
        c=a.static_mesh_component;mesh=c.static_mesh
        if not mesh or not c.is_visible() or a.get_editor_property('hidden'):continue
        name=mesh.get_name();label=a.get_actor_label();path=mesh.get_path_name()
        if name.startswith('SM_TreeBamboo'):
            c.set_material(0,mesh.get_material(0));c.set_material(1,leaves)
            c.set_editor_property('evaluate_world_position_offset',False)
            c.set_collision_profile_name('OldVillageCanopy')
            # Compact stems block movement without turning leaf cards into walls.
            proxy(a,12,180);report['bamboo']+=1;continue
        if any(token in path for token in ['/Foliage/','SM_Plant','SM_TreeBlackPine']):continue
        if name in ['SM_OV_Island','SM_OV_Water','SM_OV_CourtyardMedallion'] or label in ['Garden pond water','BLOCK courtyard medallion']:continue
        if any(token in name for token in ['PotUnSoiled','Cup','Vase']):continue
        center,extent=a.get_actor_bounds(False)
        if extent.z*2<45 and max(extent.x,extent.y)*2<80:continue
        profile=str(c.get_collision_profile_name())
        # Existing building/gateway triangle collision preserves their openings.
        if profile not in ['NoCollision','OldVillageCanopy'] and (name.startswith('SM_OV_') or name.startswith('BLOCK')):continue
        if name.startswith('SM_OV_'):
            c.set_collision_profile_name('OldVillageSolid');report['solid_props'].append(label);continue
        if profile in ['NoCollision','OldVillageCanopy'] or SM.get_simple_collision_count(mesh)==0:
            if path not in copies:copies[path]=simple_mesh(mesh)
            c.set_static_mesh(copies[path]);c.set_collision_profile_name('OldVillageSolid')
            report['solid_props'].append(label)
    # Place paired shrubs in existing planted beds, away from the combat lanes.
    for bed in [a for a in actors if a.get_actor_label()=='Garden bed edging']:
        for i,path in enumerate([BUSH,ROSE]):
            tag='VillageBush_'+bed.get_name()+'_'+str(i)
            if any(a.actor_has_tag(tag) for a in ACT.get_all_level_actors()):continue
            p=bed.get_actor_location();p.x+=(-170 if i==0 else 170);p.y+=70
            a=ACT.spawn_actor_from_class(u.StaticMeshActor,p,u.Rotator(pitch=0,yaw=35+i*70,roll=0))
            a.set_actor_label('Planted shrub' if i==0 else 'Planted rose bush');a.set_folder_path(str(bed.get_folder_path())+'/Shrubs')
            a.set_editor_property('tags',[tag,'VillagePlantedBush'])
            c=a.static_mesh_component;c.set_static_mesh(u.load_asset(path));a.set_actor_scale3d(u.Vector(.85,.85,.85) if i==0 else u.Vector(1.4,1.4,1.4))
            c.set_editor_property('evaluate_world_position_offset',False)
            c.set_collision_profile_name('OldVillageCanopy');c.set_cull_distance(6000)
            ground_actor(a);proxy(a,55 if i==0 else 42,110)
            report['bushes']+=1
    report['total_bushes']=sum(a.actor_has_tag('VillagePlantedBush') for a in ACT.get_all_level_actors())
    report['plant_colliders']=sum(a.actor_has_tag('VillageSolidPlantProxy') for a in ACT.get_all_level_actors())
    from fix_old_village_bamboo import repair_current_level
    report['bamboo_lod_repairs']=len(repair_current_level())
    return report

def placement(actor):
    p=actor.get_actor_location();r=actor.get_actor_rotation();s=actor.get_actor_scale3d()
    return (p.x,p.y,p.z,r.pitch,r.yaw,r.roll,s.x,s.y,s.z)

if __name__=='__main__':
    backup=ROOT/'Saved/OldVillage/Backups'/(datetime.now().strftime('%Y%m%d_%H%M%S')+'_Polish');backup.mkdir(parents=True,exist_ok=True)
    report={}
    for name in ['Lvl_OldVillage','Lvl_OldVillage_Blockout']:
        source=ROOT/'Content/Maps'/(name+'.umap');shutil.copy2(source,backup/source.name)
        assert L.load_level('/Game/Maps/'+name)
        before={a.get_name():placement(a) for a in ACT.get_all_level_actors() if isinstance(a,(u.StaticMeshActor,u.BossToriiGate,u.PlayerStart))}
        report[name]=apply_current_level_polish()
        report[name]['navigation']=prepare_current_level()
        changed=[a.get_actor_label() for a in ACT.get_all_level_actors() if a.get_name() in before and placement(a)!=before[a.get_name()]]
        assert not changed,changed
        assert L.save_current_level()
    (ROOT/'Saved/OldVillage/PolishReport.json').write_text(json.dumps(report,indent=2))
    u.log('OLD_VILLAGE_POLISH_COMPLETE')
