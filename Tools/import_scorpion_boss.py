"""Import the fitted scorpion into isolated assets before activating the boss."""
import json
import math
from pathlib import Path
import unreal as u

ROOT=Path(u.Paths.project_dir()).resolve();ART=ROOT/'Art/Enemies/ScorpionBoss';OUT=ROOT/'Saved/ScorpionBoss'
DEST='/Game/Assets/EnemyCharacters/PaleStinger'
E=u.EditorAssetLibrary;M=u.MaterialEditingLibrary;tools=u.AssetToolsHelpers.get_asset_tools()
u.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
rig=json.loads((OUT/'RiggingReport.json').read_text())
assert rig['unweighted']==0 and rig['max_influences']<=4 and rig['anatomy']==dict(legs=6,claws=2,tail_bones=3)
assert all(v['max_edge_stretch']<4.5 for v in rig['clips'].values())
def save(a):assert E.save_loaded_asset(a,False),a.get_path_name()
def task(file,folder,name,options=None):
    t=u.AssetImportTask();t.filename=str(file);t.destination_path=folder;t.destination_name=name;t.automated=True;t.replace_existing=True;t.save=True
    if options:t.factory=u.FbxFactory();t.options=options
    tools.import_asset_tasks([t]);return [u.load_asset(p) for p in t.imported_object_paths]
def fbx():
    o=u.FbxImportUI();o.automated_import_should_detect_type=False;o.import_materials=False;o.import_textures=False;o.create_physics_asset=False
    for data in [o.skeletal_mesh_import_data,o.anim_sequence_import_data]:
        data.set_editor_property('convert_scene',True);data.set_editor_property('convert_scene_unit',True)
    o.skeletal_mesh_import_data.set_editor_property('update_skeleton_reference_pose',True)
    o.skeletal_mesh_import_data.set_editor_property('use_t0_as_ref_pose',False)
    o.skeletal_mesh_import_data.set_editor_property('normal_import_method',u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    return o
o=fbx();o.mesh_type_to_import=o.original_import_type=u.FBXImportType.FBXIT_SKELETAL_MESH;o.import_as_skeletal=True;o.import_mesh=True;o.import_animations=False
task(ART/'SK_PaleStinger.fbx',DEST,'SK_PaleStinger',o)
mesh=u.load_asset(DEST+'/SK_PaleStinger');assert isinstance(mesh,u.SkeletalMesh)
skeleton=mesh.get_editor_property('skeleton');assert skeleton
mod=u.SkeletonModifier();assert mod.set_skeletal_mesh(mesh)
bones=list(mod.get_all_bone_names());assert len(bones)==rig['bones'],(bones,rig['bones'])
mesh.set_editor_property('positive_bounds_extension',u.Vector())
mesh.set_editor_property('negative_bounds_extension',u.Vector())
extent=mesh.get_bounds().box_extent;assert 280<extent.z<320,('Incorrect meter-to-cm import',extent)
texture=task(ART/'Enemy_BossScorpion_base.jpg',DEST+'/Textures','T_PaleStinger_BaseColor')[0]
texture.set_editor_property('srgb',True);texture.set_editor_property('max_texture_size',2048);save(texture)
material=u.load_asset(DEST+'/M_PaleStinger') or tools.create_asset('M_PaleStinger',DEST,u.Material,u.MaterialFactoryNew())
M.delete_all_material_expressions(material);M.set_material_usage(material,u.MaterialUsage.MATUSAGE_SKELETAL_MESH)
color=M.create_material_expression(material,u.MaterialExpressionTextureSampleParameter2D,-650,0)
color.set_editor_property('parameter_name','BaseColor');color.set_editor_property('texture',texture)
assert M.connect_material_property(color,'RGB',u.MaterialProperty.MP_BASE_COLOR)
for y,name,value,prop in [(200,'Roughness',.74,u.MaterialProperty.MP_ROUGHNESS),(350,'Specular',.24,u.MaterialProperty.MP_SPECULAR)]:
    n=M.create_material_expression(material,u.MaterialExpressionScalarParameter,-450,y);n.set_editor_property('parameter_name',name);n.set_editor_property('default_value',value)
    assert M.connect_material_property(n,'',prop)
data=material.get_editor_property('editor_only_data')
for name in ['BaseColor','Roughness','Specular']:assert u.SwapVFXSetupLibrary.set_property_text(data,name,'(UseConstant=False)')
M.recompile_material(material);save(material)
slots=list(mesh.get_editor_property('materials'));assert len(slots)==1
slots[0].material_interface=material;mesh.set_editor_property('materials',slots)
mesh.set_editor_property('positive_bounds_extension',u.Vector(80,80,50));mesh.set_editor_property('negative_bounds_extension',u.Vector(80,80,30))
editor=u.get_editor_subsystem(u.SkeletalMeshEditorSubsystem);assert editor.regenerate_lod(mesh,3,False,False)
save(mesh);save(skeleton)
clips={};raw_bind=None
for label in ['Idle','Walk','Cast','Death']:
    o=fbx();o.mesh_type_to_import=o.original_import_type=u.FBXImportType.FBXIT_ANIMATION;o.import_mesh=False;o.import_as_skeletal=True;o.import_animations=True;o.skeleton=skeleton
    o.override_animation_name='A_PaleStinger_'+label
    imported=task(ART/('PaleStinger_'+label+'.fbx'),DEST+'/Animations','A_PaleStinger_'+label,o)
    animation=next(a for a in imported if isinstance(a,u.AnimSequence))
    assert animation.get_editor_property('skeleton')==skeleton
    model=animation.get_editor_property('data_model_interface');count=model.get_number_of_keys()
    poses=[u.AnimationLibrary.get_bone_pose_for_time(animation,'root',animation.get_play_length()*i/(count-1),False) for i in range(count)]
    reference=mod.get_bone_transform('root',False)
    if raw_bind is None:raw_bind=poses[0]
    # Apply the mesh's imported FBX root basis to every clip, including its
    # small authored floor corrections. Keep all child-local animation keys.
    q=raw_bind.rotation;inverse=u.Quat(-q.x,-q.y,-q.z,q.w);basis=reference.rotation*inverse
    def root_position(p):
        local=inverse.rotate_vector(p.translation-raw_bind.translation)
        source,target=raw_bind.scale3d,reference.scale3d
        scaled=u.Vector(local.x*target.x/source.x,local.y*target.y/source.y,local.z*target.z/source.z)
        return reference.translation+reference.rotation.rotate_vector(scaled)
    controller=animation.get_editor_property('controller');controller.open_bracket('Fit scorpion FBX root basis',False)
    assert controller.set_bone_track_keys('root',[root_position(p) for p in poses],
        [basis*p.rotation for p in poses],[reference.scale3d for p in poses],False)
    controller.close_bracket(False);animation.set_editor_property('enable_root_motion',False);save(animation);clips[label]=animation
bind_errors=[]
for bone in bones:
    actual=u.AnimationLibrary.get_bone_pose_for_time(clips['Idle'],bone,0,False);reference=mod.get_bone_transform(bone,False)
    q,r=actual.rotation,reference.rotation;dot=abs(q.x*r.x+q.y*r.y+q.z*r.z+q.w*r.w)
    if actual.translation.distance(reference.translation)>.1 or dot<.999:bind_errors.append((str(bone),actual.translation.distance(reference.translation),dot))
assert not bind_errors,('Idle bind pose mismatch',bind_errors)
abp=E.duplicate_asset('/Game/HeavensDivide/Blueprints/EnemyCharacters/Mobs/ABP_EnemyDevilRanged',DEST+'/ABP_PaleStinger') if not E.does_asset_exist(DEST+'/ABP_PaleStinger') else u.load_asset(DEST+'/ABP_PaleStinger')
assert abp and u.SwapVFXSetupLibrary.set_property_text(abp,'TargetSkeleton',"Skeleton'"+skeleton.get_path_name()+"'")
for label,suffix in [('Walk','AnimGraph.AnimGraphNode_StateMachine_0.Locomotion.AnimStateNode_0.Walk.AnimGraphNode_SequencePlayer_0'),('Idle','AnimGraph.AnimGraphNode_StateMachine_0.Locomotion.AnimStateNode_1.Idle.AnimGraphNode_SequencePlayer_1')]:
    n=u.load_object(None,abp.get_path_name()+':'+suffix);assert n
    assert u.SwapVFXSetupLibrary.set_property_text(n,'Node','(Sequence="'+clips[label].get_path_name()+'",bLoopAnimation=True)')
u.BlueprintEditorLibrary.compile_blueprint(abp);assert 'ERROR' not in str(abp.get_editor_property('status'));save(abp)
montages={}
for label in ['Cast','Death']:
    factory=u.AnimMontageFactory();factory.target_skeleton=skeleton;factory.source_animation=clips[label]
    montage=u.load_asset(DEST+'/Animations/AM_PaleStinger_'+label) or tools.create_asset('AM_PaleStinger_'+label,DEST+'/Animations',u.AnimMontage,factory)
    assert montage;save(montage);montages[label]=montage
head=mod.get_bone_transform('Head',True).translation;body=mod.get_bone_transform('Thorax',True).translation
yaw=-math.degrees(math.atan2(head.y-body.y,head.x-body.x))
report=dict(mesh=mesh.get_path_name(),skeleton=skeleton.get_path_name(),anim_blueprint=abp.get_path_name(),material=material.get_path_name(),texture=texture.get_path_name(),
    bones=[str(b) for b in bones],component_yaw=yaw,animations={k:a.get_path_name() for k,a in clips.items()},montages={k:a.get_path_name() for k,a in montages.items()},
    durations={k:a.get_play_length() for k,a in clips.items()},lod_vertices=[editor.get_num_verts(mesh,i) for i in range(3)],lod_sections=[editor.get_num_sections(mesh,i) for i in range(3)],root_reference=str(mod.get_bone_transform('root',False)),raw_root_bind=str(raw_bind))
(OUT/'ImportedAssets.json').write_text(json.dumps(report,indent=2))
u.log('SCORPION_IMPORTED '+json.dumps(report))
