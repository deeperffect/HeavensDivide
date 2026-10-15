"""Import the inspected Tripo kit into an isolated, named Unreal content folder."""
import json
from pathlib import Path
import unreal as u
ROOT=Path(u.Paths.project_dir()).resolve()
DEST='/Game/HeavensDivide/Environment/OldVillage'
E=u.EditorAssetLibrary; M=u.MaterialEditingLibrary; A=u.AssetToolsHelpers.get_asset_tools()
u.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')

def save(a):
    assert E.save_loaded_asset(a,False),a.get_path_name()

def import_file(file,folder,name,mesh=False):
    t=u.AssetImportTask(); t.filename=str(file);t.destination_path=folder;t.destination_name=name
    t.automated=True;t.replace_existing=True;t.save=True
    if mesh:
        o=u.FbxImportUI();o.automated_import_should_detect_type=False
        o.mesh_type_to_import=o.original_import_type=u.FBXImportType.FBXIT_STATIC_MESH
        o.import_as_skeletal=False;o.import_mesh=True;o.import_materials=False;o.import_textures=False
        d=o.static_mesh_import_data;d.combine_meshes=True;d.auto_generate_collision=False
        d.convert_scene=True;d.convert_scene_unit=True
        d.normal_import_method=u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
        t.options=o;t.factory=u.FbxFactory()
    A.import_asset_tasks([t])
    obj=u.load_asset(folder+'/'+name);assert obj,(name,list(t.imported_object_paths))
    return obj

def painted_material(name,tex=None,color=None):
    path=DEST+'/Materials/'+name
    mat=u.load_asset(path) if E.does_asset_exist(path) else A.create_asset(name,DEST+'/Materials',u.Material,u.MaterialFactoryNew())
    M.delete_all_material_expressions(mat)
    if tex:
        node=M.create_material_expression(mat,u.MaterialExpressionTextureSample,-600,0);node.set_editor_property('texture',tex);output='RGB'
    else:
        node=M.create_material_expression(mat,u.MaterialExpressionConstant3Vector,-600,0);node.set_editor_property('constant',u.LinearColor(*color));output=''
    M.connect_material_property(node,output,u.MaterialProperty.MP_BASE_COLOR)
    glow=M.create_material_expression(mat,u.MaterialExpressionMultiply,-300,180);glow.set_editor_property('const_b',.09)
    M.connect_material_expressions(node,output,glow,'A');M.connect_material_property(glow,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
    for index,(value,prop) in enumerate([(.92,u.MaterialProperty.MP_ROUGHNESS),(.12,u.MaterialProperty.MP_SPECULAR)]):
        n=M.create_material_expression(mat,u.MaterialExpressionConstant,-300,350+index*100);n.set_editor_property('r',value);M.connect_material_property(n,'',prop)
    for p in ['BaseColor','Roughness','Specular','EmissiveColor']:
        u.SwapVFXSetupLibrary.set_property_text(mat.get_editor_property('editor_only_data'),p,'(UseConstant=False)')
    M.recompile_material(mat);save(mat);return mat

def run():
    report=[]
    manifest=json.loads((ROOT/'Art/Environment/OldVillage/SourceManifest.json').read_text())
    for a in manifest['assets']:
        name=a['name'];folder=DEST+'/Meshes/'+a['category']
        mesh=import_file(ROOT/a['prepared_fbx'],folder,'SM_OV_'+name,True)
        tex=import_file(ROOT/a['texture'],DEST+'/Textures','T_OV_'+name+'_BaseColor')
        tex.set_editor_property('srgb',True);tex.set_editor_property('max_texture_size',2048);save(tex)
        mat=painted_material('M_OV_'+name,tex)
        mesh.set_material(0,mat)
        body=mesh.get_editor_property('body_setup')
        body.set_editor_property('collision_trace_flag',u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        save(mesh)
        b=mesh.get_bounds()
        report.append(dict(name=name,path=mesh.get_path_name(),dimensions=[b.box_extent.x*2,b.box_extent.y*2,b.box_extent.z*2],origin=[b.origin.x,b.origin.y,b.origin.z]))
        u.log('OLD_VILLAGE_IMPORTED '+name)
    (ROOT/'Saved/OldVillage/ImportReport.json').write_text(json.dumps(report,indent=2))
    u.log('OLD_VILLAGE_IMPORT_COMPLETE')

if __name__=='__main__':run()
