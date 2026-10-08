import bpy,json,math
from pathlib import Path
from mathutils import Vector,Matrix
ROOT=Path(r'C:\Users\deepe\OneDrive\Documents\Unreal Projects\HeavensDivide')
ART=ROOT/'Art/Characters/Ninja/NinjaV6_Textured';OUT=ROOT/'Saved/NinjaV6Replacement'
bpy.ops.wm.open_mainfile(filepath=str(ART/'Bloodshift_NinjaV6_ScarfSeparated.blend'))
obj=bpy.data.objects['NinjaV6_ReferenceTextured'];obj.name='SK_NinjaV6'
with bpy.data.libraries.load(str(OUT/'CurrentNinja_Rig.blend'),link=False) as (src,dst):
    dst.objects=src.objects
for o in dst.objects:
    if o:bpy.context.scene.collection.objects.link(o)
arm=next(o for o in dst.objects if o.type=='ARMATURE')
source=next(o for o in dst.objects if o.type=='MESH')
# Unreal's FBX root is represented as Blender's armature object on import.
# Make it an explicit deform bone, retaining the imported children's matrices.
arm.name='Armature'
bpy.ops.object.select_all(action='DESELECT');arm.select_set(True);bpy.context.view_layer.objects.active=arm
bpy.ops.object.mode_set(mode='EDIT')
roots=[b for b in arm.data.edit_bones if not b.parent]
hip=arm.data.edit_bones.new('Hips');hip.head=(0,0,0);hip.tail=(0,5,0)
for b in roots:b.parent=hip
bpy.ops.object.mode_set(mode='OBJECT')
hipgroup=source.vertex_groups.new(name='Hips')
for v in source.data.vertices:
    missing=max(0,1-sum(g.weight for g in v.groups))
    if missing>.00001:hipgroup.add([v.index],missing,'REPLACE')
original=[v.co.copy() for v in obj.data.vertices]
for v in obj.data.vertices:v.co+=Vector((-.007,.10,0))
# Fit the old T-pose to the new A-pose for weight projection, without changing
# the skeleton's stored rest matrices or any existing animation asset.
for side,angle in [('Left',65),('Right',-65)]:
    b=arm.pose.bones[side+'Arm'];world=arm.matrix_world@b.matrix
    rotation=Matrix.Rotation(math.radians(angle),4,'Y')
    pivot=world.translation.copy()
    b.matrix=arm.matrix_world.inverted()@Matrix.Translation(pivot)@rotation@Matrix.Translation(-pivot)@world
    bpy.context.view_layer.update()
for b in arm.data.bones:obj.vertex_groups.new(name=b.name)
bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
transfer=obj.modifiers.new('Project existing animation weights','DATA_TRANSFER')
transfer.object=source;transfer.use_vert_data=True;transfer.data_types_verts={'VGROUP_WEIGHTS'}
transfer.vert_mapping='POLYINTERP_NEAREST'
bpy.ops.object.modifier_apply(modifier=transfer.name)
groups=json.loads((OUT/'V6_Components.json').read_text())
component={v:g['component'] for g in groups for v in g['vertex_indices']}
def weights(index,items):
    v=obj.data.vertices[index]
    for entry in list(v.groups):obj.vertex_groups[entry.group].remove([index])
    for name,w in items:
        if w>1e-6:obj.vertex_groups[name].add([index],w,'REPLACE')
body_components={1,2,5,6,7,8,10,12,15,22,24,28,35,36,37}
for v in obj.data.vertices:
    c=component[v.index];p=original[v.index]
    if c in {15,24}:weights(v.index,[('Spine2',1)])
    elif c not in body_components:
        # Keep the face, bangs and ornament rigid; the trailing ponytail uses
        # the same five-bone dynamics chain as the original playable ninja.
        if c not in {3,4,13,38,39,41,42,43,44,45,46} and p.y>-.025 and p.z<.962:
            t=max(0,min(4,(.96-p.z)/.34*4))
            a=min(3,int(t));f=t-a
            weights(v.index,[(f'Ponytail{a+1}',1-f),(f'Ponytail{a+2}',f)])
        else:weights(v.index,[('Head',1)])
    elif c in {6,35}:weights(v.index,[('Hips',.8),('Spine',.2)])
    elif c in {10,28}:weights(v.index,[('RightArm',.8),('RightShoulder',.2)])
    # Remove vanishing influences and normalize every vertex (maximum four).
    entries=sorted([(obj.vertex_groups[w.group].name,w.weight) for w in v.groups if w.weight>.002],key=lambda x:-x[1])[:4]
    if not entries:entries=[('Hips',1)]
    total=sum(w for n,w in entries);weights(v.index,[(n,w/total) for n,w in entries])
bpy.context.view_layer.update()
posed_points=[v.co.copy() for v in obj.data.vertices]
matrices={b.name:arm.matrix_world@b.matrix@b.bone.matrix_local.inverted()@arm.matrix_world.inverted() for b in arm.pose.bones}
# Invert the exact weighted skin matrix, returning the new geometry to the
# original animation bind pose. Reapplying the A-pose must recover every point.
max_error=0
for v in obj.data.vertices:
    m=Matrix(((0,0,0,0),)*4)
    for g in v.groups:m+=matrices[obj.vertex_groups[g.group].name]*g.weight
    target=v.co.copy();v.co=m.inverted()@target
    max_error=max(max_error,(m@v.co-target).length)
assert max_error<1e-5,max_error
mod=obj.modifiers.new('Ninja animation rig','ARMATURE');mod.object=arm
obj.parent=arm;obj.matrix_parent_inverse=arm.matrix_world.inverted()
source.hide_render=True;source.hide_set(True)
for o in dst.objects:
    if o!=arm and o!=source and o.type=='EMPTY':o.hide_render=True
scene=bpy.context.scene;scene.render.resolution_x=768;scene.render.resolution_y=1024;scene.cycles.samples=12
cam=scene.camera;target=Vector((0,.08,.50));cam.location=(0,-3,.5);cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.ortho_scale=1.10
scene.render.filepath=str(OUT/'Rigged_APose_Front.png');bpy.ops.render.render(write_still=True)
for b in arm.pose.bones:b.matrix_basis=Matrix.Identity(4)
bpy.context.view_layer.update()
scene.render.filepath=str(OUT/'Rigged_BindPose_Front.png');bpy.ops.render.render(write_still=True)
# Exclude the transfer donor from the production .blend.
bpy.data.objects.remove(source,do_unlink=True)
arm.show_in_front=True
bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);arm.select_set(True);bpy.context.view_layer.objects.active=obj
for image in bpy.data.images:
    if image.source=='FILE' and image.has_data:image.pack()
bpy.ops.wm.save_as_mainfile(filepath=str(ART/'Bloodshift_NinjaV6_UE5.blend'))
bpy.ops.export_scene.fbx(filepath=str(ART/'Bloodshift_NinjaV6_UE5.fbx'),use_selection=True,object_types={'MESH','ARMATURE'},
    add_leaf_bones=False,bake_anim=False,use_armature_deform_only=False,axis_forward='-Z',axis_up='Y',
    primary_bone_axis='Y',secondary_bone_axis='X',path_mode='COPY',embed_textures=False)
report={'vertices':len(obj.data.vertices),'polygons':len(obj.data.polygons),'bones':len(arm.data.bones),
        'materials':[m.name for m in obj.data.materials],'a_pose_inverse_skin_max_error_m':max_error,
        'unweighted_vertices':sum(not v.groups for v in obj.data.vertices),
        'max_influences':max(len(v.groups) for v in obj.data.vertices)}
(OUT/'Rigging.json').write_text(json.dumps(report,indent=2));print('NINJA_V6_RIG_PASS',report,flush=True)
