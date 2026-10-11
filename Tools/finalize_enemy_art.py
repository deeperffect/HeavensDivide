import bpy
from pathlib import Path
from mathutils import Matrix
ROOT=Path(r'C:\Users\deepe\OneDrive\Documents\Unreal Projects\HeavensDivide')
for name in ['AshSeer','OgreWarden','HexSniper']:
    dest=ROOT/'Art/Enemies'/name;bpy.ops.wm.open_mainfile(filepath=str(dest/('Bloodshift_'+name+'_Rigged.blend')))
    arm=bpy.data.objects['Armature'];obj=bpy.data.objects['SK_'+name];arm.animation_data.action=None
    for b in arm.pose.bones:b.matrix_basis=Matrix.Identity(4)
    scale=obj.matrix_world.to_scale().x
    if scale>2:arm.matrix_world=Matrix.Diagonal((1/scale,1/scale,1/scale,1))@arm.matrix_world
    bpy.context.view_layer.update();bpy.ops.object.select_all(action='DESELECT');arm.select_set(True);obj.select_set(True);bpy.context.view_layer.objects.active=arm
    bpy.ops.wm.save_as_mainfile(filepath=str(dest/('Bloodshift_'+name+'_Rigged.blend')))
    bpy.ops.export_scene.fbx(filepath=str(dest/('SK_'+name+'.fbx')),use_selection=True,object_types={'MESH','ARMATURE'},add_leaf_bones=False,bake_anim=False,mesh_smooth_type='FACE',axis_forward='-Z',axis_up='Y',primary_bone_axis='Y',secondary_bone_axis='X',path_mode='COPY',embed_textures=False)
