"""Export mesh and animation with identical FBX root hierarchy and unit transforms."""
import bpy,json,os
from pathlib import Path
from mathutils import Matrix
ROOT=Path(r'C:\Users\deepe\OneDrive\Documents\Unreal Projects\HeavensDivide')
for name in os.environ.get('BLOODSHIFT_ENEMY_NAMES','AshSeer,OgreWarden,HexSniper').split(','):
    dest=ROOT/'Art/Enemies'/name;bpy.ops.wm.open_mainfile(filepath=str(dest/('Bloodshift_'+name+'_Rigged.blend')))
    arm=bpy.data.objects['Armature'];obj=bpy.data.objects['SK_'+name];arm.animation_data.action=None
    for b in arm.pose.bones:b.matrix_basis=Matrix.Identity(4)
    scale=obj.matrix_world.to_scale().x
    if scale>2:arm.matrix_world=Matrix.Diagonal((1/scale,1/scale,1/scale,1))@arm.matrix_world
    bpy.context.view_layer.update();aw=arm.matrix_world.copy();om=obj.matrix_basis.copy()
    bpy.ops.object.select_all(action='DESELECT');arm.select_set(True);obj.select_set(True);bpy.context.view_layer.objects.active=arm
    bpy.ops.wm.save_as_mainfile(filepath=str(dest/('Bloodshift_'+name+'_Rigged.blend')))
    settings=dict(use_selection=True,object_types={'MESH','ARMATURE'},add_leaf_bones=False,mesh_smooth_type='FACE',axis_forward='-Z',axis_up='Y',primary_bone_axis='Y',secondary_bone_axis='X',path_mode='COPY',embed_textures=False)
    bpy.ops.export_scene.fbx(filepath=str(dest/('SK_'+name+'.fbx')),bake_anim=False,**settings)
    arm.matrix_world=aw;obj.matrix_basis=om
    for label in ['Idle','Walk','Cast']:
        action=bpy.data.actions[name+'_'+label];arm.animation_data.action=action
        bpy.context.scene.frame_start=1;bpy.context.scene.frame_end=round(action.frame_range[1]);bpy.context.scene.frame_set(1)
        bpy.ops.export_scene.fbx(filepath=str(dest/(name+'_'+label+'.fbx')),bake_anim=True,bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,bake_anim_force_startend_keying=True,bake_anim_simplify_factor=0,**settings)
        arm.matrix_world=aw;obj.matrix_basis=om
    print('FINAL_ENEMY_FBX_EXPORT',name,flush=True)
