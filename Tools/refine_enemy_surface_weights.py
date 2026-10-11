import bpy,sys,json
from pathlib import Path
from mathutils import Matrix
ROOT=Path(r'C:\Users\deepe\OneDrive\Documents\Unreal Projects\HeavensDivide');sys.path.insert(0,str(ROOT/'Tools'))
from enemy_geodesic_skin import skin
for name in ['OgreWarden','HexSniper']:
    dest=ROOT/'Art/Enemies'/name;bpy.ops.wm.open_mainfile(filepath=str(dest/('Bloodshift_'+name+'_Rigged.blend')))
    arm=bpy.data.objects['Armature'];obj=bpy.data.objects['SK_'+name];arm.animation_data.action=None
    scale=obj.matrix_world.to_scale().x
    if scale>2:arm.matrix_world=Matrix.Diagonal((1/scale,1/scale,1/scale,1))@arm.matrix_world
    for b in arm.pose.bones:b.matrix_basis=Matrix.Identity(4)
    bpy.context.view_layer.update();print(name,skin(obj,arm,name),flush=True)
    arm.animation_data.action=bpy.data.actions[name+'_Cast'];bpy.context.scene.frame_set(round(arm.animation_data.action.frame_range[1]*.65))
    bpy.context.scene.render.filepath=str(ROOT/'Saved/EnemyModelReplacement'/(name+'_Cast_Rigged.png'));bpy.ops.render.render(write_still=True)
    arm.animation_data.action=None
    for b in arm.pose.bones:b.matrix_basis=Matrix.Identity(4)
    bpy.context.view_layer.update();bpy.ops.object.select_all(action='DESELECT');arm.select_set(True);obj.select_set(True);bpy.context.view_layer.objects.active=arm
    bpy.ops.wm.save_as_mainfile(filepath=str(dest/('Bloodshift_'+name+'_Rigged.blend')))
    bpy.ops.export_scene.fbx(filepath=str(dest/('SK_'+name+'.fbx')),use_selection=True,object_types={'MESH','ARMATURE'},add_leaf_bones=False,bake_anim=False,mesh_smooth_type='FACE',axis_forward='-Z',axis_up='Y',primary_bone_axis='Y',secondary_bone_axis='X',path_mode='COPY',embed_textures=False)
