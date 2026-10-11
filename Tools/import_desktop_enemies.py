"""Import isolated fitted enemy assets. Activate only after visual/asset validation."""
import unreal as u,json,os
from pathlib import Path
ROOT=Path(u.Paths.project_dir()).resolve();OUT=ROOT/'Saved'/os.environ.get('BLOODSHIFT_ENEMY_WORKSPACE','EnemyModelReplacement');ART=ROOT/'Art/Enemies'
tools=u.AssetToolsHelpers.get_asset_tools();M=u.MaterialEditingLibrary;E=u.EditorAssetLibrary
u.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
def save(asset):assert E.save_loaded_asset(asset,False),asset.get_path_name()
def task(file,folder,name,options=None):
    t=u.AssetImportTask();t.filename=str(file);t.destination_path=folder;t.destination_name=name;t.automated=True;t.replace_existing=True;t.save=True
    if options:t.factory=u.FbxFactory();t.options=options
    tools.import_asset_tasks([t]);return [u.load_asset(p) for p in t.imported_object_paths]
def fbx():
    o=u.FbxImportUI();o.automated_import_should_detect_type=False;o.import_materials=False;o.import_textures=False;o.create_physics_asset=False
    o.skeletal_mesh_import_data.set_editor_property('update_skeleton_reference_pose',True)
    o.skeletal_mesh_import_data.set_editor_property('use_t0_as_ref_pose',False)
    o.skeletal_mesh_import_data.set_editor_property('normal_import_method',u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    o.skeletal_mesh_import_data.set_editor_property('convert_scene',True);o.skeletal_mesh_import_data.set_editor_property('convert_scene_unit',True)
    return o
report={}
for name in os.environ.get('BLOODSHIFT_ENEMY_NAMES','AshSeer,OgreWarden,HexSniper').split(','):
    dest='/Game/Assets/EnemyCharacters/'+name;art=ART/name
    o=fbx();o.mesh_type_to_import=o.original_import_type=u.FBXImportType.FBXIT_SKELETAL_MESH;o.import_as_skeletal=True;o.import_mesh=True;o.import_animations=False
    task(art/('SK_'+name+'.fbx'),dest,'SK_'+name,o)
    mesh=u.load_asset(dest+'/SK_'+name);assert isinstance(mesh,u.SkeletalMesh)
    assert 45<mesh.get_bounds().box_extent.z<55,(name,'Incorrect import scale',mesh.get_bounds())
    skeleton=mesh.get_editor_property('skeleton');assert skeleton and skeleton.get_path_name().startswith(dest+'/')
    mod=u.SkeletonModifier();assert mod.set_skeletal_mesh(mesh);assert len(mod.get_all_bone_names())>=22
    tex=task(art/(name+'_BaseColor.jpg'),dest+'/Textures','T_'+name+'_BaseColor')[0]
    tex.set_editor_property('srgb',True);tex.set_editor_property('max_texture_size',2048);save(tex)
    material=u.load_asset(dest+'/M_'+name) if E.does_asset_exist(dest+'/M_'+name) else tools.create_asset('M_'+name,dest,u.Material,u.MaterialFactoryNew())
    M.delete_all_material_expressions(material);M.set_material_usage(material,u.MaterialUsage.MATUSAGE_SKELETAL_MESH)
    material.set_editor_property('blend_mode',u.BlendMode.BLEND_MASKED);material.set_editor_property('opacity_mask_clip_value',.333)
    def node(cls,x,y):return M.create_material_expression(material,cls,x,y)
    def connect(a,out,b,input):assert M.connect_material_expressions(a,out,b,input)
    def prop(n,out,p):assert M.connect_material_property(n,out,p)
    color=node(u.MaterialExpressionTextureSampleParameter2D,-850,0);color.set_editor_property('parameter_name','BaseColor');color.set_editor_property('texture',tex)
    brightness=node(u.MaterialExpressionScalarParameter,-850,210);brightness.set_editor_property('parameter_name','ColorBrightness');brightness.set_editor_property('default_value',1.15)
    base=node(u.MaterialExpressionMultiply,-550,0);connect(color,'RGB',base,'A');connect(brightness,'',base,'B');prop(base,'',u.MaterialProperty.MP_BASE_COLOR)
    for i,(label,value,mp) in enumerate([('Roughness',.78,u.MaterialProperty.MP_ROUGHNESS),('Specular',.28,u.MaterialProperty.MP_SPECULAR)]):
        n=node(u.MaterialExpressionScalarParameter,-330,150+i*130);n.set_editor_property('parameter_name',label);n.set_editor_property('default_value',value);prop(n,'',mp)
    glow=node(u.MaterialExpressionMultiply,-340,440);glow.set_editor_property('const_b',.025);connect(color,'RGB',glow,'A');prop(glow,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
    noise_texture=u.load_asset('/Game/Assets/VFX/VerticalBeamsV1/Textures/Noise/T_NoiseValue01');assert noise_texture
    noise=node(u.MaterialExpressionTextureSample,-850,650);noise.set_editor_property('texture',noise_texture)
    scale=node(u.MaterialExpressionMultiply,-650,650);scale.set_editor_property('const_b',.98);connect(noise,'R',scale,'A')
    bias=node(u.MaterialExpressionAdd,-450,650);bias.set_editor_property('const_b',.01);connect(scale,'',bias,'A')
    dissolve=node(u.MaterialExpressionScalarParameter,-650,850);dissolve.set_editor_property('parameter_name','DissolveAmount');dissolve.set_editor_property('default_value',0)
    sub=node(u.MaterialExpressionSubtract,-240,650);connect(bias,'',sub,'A');connect(dissolve,'',sub,'B')
    mask=node(u.MaterialExpressionAdd,-40,650);mask.set_editor_property('const_b',.333);connect(sub,'',mask,'A');prop(mask,'',u.MaterialProperty.MP_OPACITY_MASK)
    data=material.get_editor_property('editor_only_data')
    for property_name in ['BaseColor','Roughness','Specular','EmissiveColor','OpacityMask']:
        assert u.SwapVFXSetupLibrary.set_property_text(data,property_name,'(UseConstant=False)')
    M.recompile_material(material);save(material)
    slots=list(mesh.get_editor_property('materials'));assert len(slots)==1
    slots[0].material_interface=material;mesh.set_editor_property('materials',slots)
    # Three geometric LODs; four influences and one material section at all distances.
    editor=u.get_editor_subsystem(u.SkeletalMeshEditorSubsystem)
    assert editor.regenerate_lod(mesh,3,False,False)
    save(mesh);save(skeleton)
    clips={}
    for label in ['Idle','Walk','Cast']:
        o=fbx();o.mesh_type_to_import=o.original_import_type=u.FBXImportType.FBXIT_ANIMATION;o.import_mesh=False;o.import_as_skeletal=True;o.import_animations=True;o.skeleton=skeleton
        o.override_animation_name='A_'+name+'_'+label
        imported=task(art/(name+'_'+label+'.fbx'),dest+'/Animations','A_'+name+'_'+label,o)
        anim=next(a for a in imported if isinstance(a,u.AnimSequence));assert anim.get_editor_property('skeleton')==skeleton
        ref_pose=mod.get_bone_transform('Hips',False)
        # Blender's Armature object is folded into the mesh reference skeleton,
        # but legacy FBX animation import leaves the root keys in object space.
        # Bake the same basis into the root track; child-local tracks are unchanged.
        model=anim.get_editor_property('data_model_interface')
        count=model.get_number_of_keys()
        poses=[u.AnimationLibrary.get_bone_pose_for_time(anim,'Hips',anim.get_play_length()*i/(count-1),False) for i in range(count)]
        origin=poses[0].translation
        controller=anim.get_editor_property('controller')
        controller.open_bracket('Bake enemy FBX root basis',False)
        assert controller.set_bone_track_keys('Hips',
            [ref_pose.translation+ref_pose.rotation.rotate_vector(p.translation-origin) for p in poses],
            [ref_pose.rotation*p.rotation for p in poses],
            [ref_pose.scale3d for p in poses],False)
        controller.close_bracket(False)
        root_pose=u.AnimationLibrary.get_bone_pose_for_time(anim,'Hips',0,False)
        assert root_pose.translation.distance(ref_pose.translation)<2,(name,label,'Animation root mismatch',root_pose,ref_pose)
        anim.set_editor_property('enable_root_motion',False);save(anim);clips[label]=anim
    path=dest+'/ABP_'+name
    bp=u.load_asset(path) if E.does_asset_exist(path) else E.duplicate_asset('/Game/HeavensDivide/Blueprints/EnemyCharacters/Mobs/ABP_EnemyDevilRanged',path)
    assert bp
    assert u.SwapVFXSetupLibrary.set_property_text(bp,'TargetSkeleton',"Skeleton'"+skeleton.get_path_name()+"'")
    for label,suffix in [('Walk','AnimGraph.AnimGraphNode_StateMachine_0.Locomotion.AnimStateNode_0.Walk.AnimGraphNode_SequencePlayer_0'),('Idle','AnimGraph.AnimGraphNode_StateMachine_0.Locomotion.AnimStateNode_1.Idle.AnimGraphNode_SequencePlayer_1')]:
        n=u.load_object(None,bp.get_path_name()+':'+suffix);assert n,suffix
        assert u.SwapVFXSetupLibrary.set_property_text(n,'Node','(Sequence="'+clips[label].get_path_name()+'")')
    u.BlueprintEditorLibrary.compile_blueprint(bp);assert 'ERROR' not in str(bp.get_editor_property('status')),bp.get_path_name();save(bp)
    montagepath=dest+'/Animations/AM_'+name+'_Cast'
    if E.does_asset_exist(montagepath):
        montage=u.load_asset(montagepath)
        assert u.SwapVFXSetupLibrary.set_property_text(montage,'Skeleton',"Skeleton'"+skeleton.get_path_name()+"'")
    else:
        factory=u.AnimMontageFactory();factory.target_skeleton=skeleton;factory.source_animation=clips['Cast']
        montage=tools.create_asset('AM_'+name+'_Cast',dest+'/Animations',u.AnimMontage,factory)
    assert montage;save(montage)
    report[name]={'mesh':mesh.get_path_name(),'skeleton':skeleton.get_path_name(),'anim_blueprint':bp.get_path_name(),'montage':montage.get_path_name(),'animations':{k:a.get_path_name() for k,a in clips.items()},'lod_vertices':[editor.get_num_verts(mesh,i) for i in range(3)],'lod_sections':[editor.get_num_sections(mesh,i) for i in range(3)]}
    (OUT/'ImportedAssets.json').write_text(json.dumps(report,indent=2))
    u.log('ENEMY_ASSET_IMPORT_PASS '+name)
u.log('ALL_ENEMY_ASSETS_IMPORTED')
