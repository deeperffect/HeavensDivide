import unreal as u
from pathlib import Path
import json,math
ROOT=Path(u.Paths.project_dir()).resolve();OUT=ROOT/'Saved/NinjaV6Replacement'
ART=ROOT/'Art/Characters/Ninja/NinjaV6_Textured'
DEST='/Game/Assets/PlayerCharacters/Ninja/V6'
tools=u.AssetToolsHelpers.get_asset_tools()
u.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')
task=u.AssetImportTask();task.filename=str(ART/'Bloodshift_NinjaV6_UE5.fbx')
task.destination_path=DEST;task.destination_name='SK_NinjaV6';task.automated=True
task.replace_existing=True;task.save=True;task.factory=u.FbxFactory()
options=u.FbxImportUI();options.automated_import_should_detect_type=False
options.mesh_type_to_import=u.FBXImportType.FBXIT_SKELETAL_MESH
options.original_import_type=u.FBXImportType.FBXIT_SKELETAL_MESH
options.import_as_skeletal=True;options.import_mesh=True;options.import_animations=False
options.import_materials=False;options.import_textures=False;options.create_physics_asset=False
data=options.skeletal_mesh_import_data
data.set_editor_property('update_skeleton_reference_pose',False)
data.set_editor_property('use_t0_as_ref_pose',False)
data.set_editor_property('normal_import_method',u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
data.set_editor_property('convert_scene',True)
data.set_editor_property('convert_scene_unit',True)
task.options=options;tools.import_asset_tasks([task])
mesh=u.load_asset(DEST+'/SK_NinjaV6');assert isinstance(mesh,u.SkeletalMesh),task.imported_object_paths
modifier=u.SkeletonModifier();assert modifier.set_skeletal_mesh(mesh)
reference=json.loads((OUT/'CurrentNinja.json').read_text())
actual_names=list(map(str,modifier.get_all_bone_names()))
diff=[]
for b in reference['bones']:
    if b['name'] not in actual_names:
        diff.append({'bone':b['name'],'missing':True});continue
    t=modifier.get_bone_transform(b['name'],False);r=b['local']
    q=[t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w]
    diff.append({'bone':b['name'],'parent':str(modifier.get_parent_name(b['name'])),
        'translation_error':max(abs(a-c) for a,c in zip([t.translation.x,t.translation.y,t.translation.z],r['translation'])),
        'rotation_dot':abs(sum(a*c for a,c in zip(q,r['rotation']))),
        'scale': [t.scale3d.x,t.scale3d.y,t.scale3d.z]})
(OUT/'ImportedRigComparison.json').write_text(json.dumps({'bones':actual_names,'comparison':diff},indent=2))
assert set(actual_names)=={b['name'] for b in reference['bones']},actual_names
for b,d in zip(reference['bones'],diff):
    assert b['parent']==d['parent'],d
    assert d['translation_error']<.002,d
    assert d['rotation_dot']>.99999,d
    assert max(abs(s-1) for s in d['scale'])<.001,d
original_skeleton=u.load_asset(reference['skeleton'])
assert u.SwapVFXSetupLibrary.set_property_text(mesh,'Skeleton',"Skeleton'"+original_skeleton.get_path_name()+"'")
mesh.set_editor_property('physics_asset',u.load_asset('/Game/Assets/PlayerCharacters/Ninja/NinjaCharacterV3_PhysicsAsset'))

textures={}
for name in ['BaseColor','ORM','Specular']:
    t=u.AssetImportTask();t.filename=str(ART/'Textures'/('T_NinjaV6_'+name+'.png'))
    t.destination_path=DEST+'/Textures';t.destination_name='T_NinjaV6_'+name
    t.automated=True;t.replace_existing=True;t.save=True;tools.import_asset_tasks([t])
    tex=u.load_asset(t.imported_object_paths[0]);tex.set_editor_property('srgb',name=='BaseColor')
    if name!='BaseColor':tex.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_MASKS)
    u.EditorAssetLibrary.save_loaded_asset(tex,False);textures[name]=tex
materials=[]
for name in ['M_NinjaV6_Body','M_NinjaV6_ScarfRibbons']:
    material=u.load_asset(DEST+'/'+name)
    if not material:material=tools.create_asset(name,DEST,u.Material,u.MaterialFactoryNew())
    u.MaterialEditingLibrary.delete_all_material_expressions(material)
    material.set_editor_property('two_sided',True)
    u.MaterialEditingLibrary.set_material_usage(material,u.MaterialUsage.MATUSAGE_SKELETAL_MESH)
    if 'Scarf' in name:u.MaterialEditingLibrary.set_material_usage(material,u.MaterialUsage.MATUSAGE_CLOTHING)
    for index,(key,tex) in enumerate(textures.items()):
        n=u.MaterialEditingLibrary.create_material_expression(material,u.MaterialExpressionTextureSampleParameter2D,-400,index*220)
        n.set_editor_property('parameter_name',key);n.set_editor_property('texture',tex)
        n.set_editor_property('sampler_type',u.MaterialSamplerType.SAMPLERTYPE_COLOR if key=='BaseColor' else u.MaterialSamplerType.SAMPLERTYPE_MASKS)
        if key=='BaseColor':u.MaterialEditingLibrary.connect_material_property(n,'RGB',u.MaterialProperty.MP_BASE_COLOR)
        elif key=='Specular':u.MaterialEditingLibrary.connect_material_property(n,'R',u.MaterialProperty.MP_SPECULAR)
        else:
            for channel,prop in [('R',u.MaterialProperty.MP_AMBIENT_OCCLUSION),('G',u.MaterialProperty.MP_ROUGHNESS),('B',u.MaterialProperty.MP_METALLIC)]:
                u.MaterialEditingLibrary.connect_material_property(n,channel,prop)
    u.MaterialEditingLibrary.recompile_material(material)
    u.EditorAssetLibrary.save_loaded_asset(material,False);materials.append(material)
slots=list(mesh.get_editor_property('materials'));assert len(slots)==2
for slot in slots:
    index=1 if 'Scarf' in str(slot.material_slot_name) else 0
    slot.material_interface=materials[index]
mesh.set_editor_property('materials',slots)
assert u.EditorAssetLibrary.save_loaded_asset(mesh,False)
u.log('NINJA_V6_IMPORT_PASS '+mesh.get_path_name())
