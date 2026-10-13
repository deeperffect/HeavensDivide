"""Anatomical scorpion skinning and editable, baked creature animations."""
import bpy
import json
import math
from pathlib import Path
from mathutils import Vector, Matrix, Quaternion

ROOT=Path(r'C:\Users\deepe\OneDrive\Documents\Unreal Projects\HeavensDivide')
ART=ROOT/'Art/Enemies/ScorpionBoss';OUT=ROOT/'Saved/ScorpionBoss'
bpy.ops.wm.open_mainfile(filepath=str(ART/'Scorpion_Source.blend'))
scene=bpy.context.scene;obj=next(o for o in scene.objects if o.type=='MESH');obj.name='SK_PaleStinger'
components=json.loads((OUT/'SourceInspection.json').read_text())['meshes'][0]['components']
arm=bpy.data.objects.new('Armature',bpy.data.armatures.new('ScorpionSkeleton'));scene.collection.objects.link(arm)
arm.show_in_front=True
bpy.ops.object.select_all(action='DESELECT');arm.select_set(True);bpy.context.view_layer.objects.active=arm
bpy.ops.object.mode_set(mode='EDIT')
landmarks={};parents={};legs={};claws={}
def bone(name,head,tail,parent=None,deform=True):
    b=arm.data.edit_bones.new(name);b.head=head;b.tail=tail;b.use_deform=deform
    if parent:b.parent=arm.data.edit_bones[parent]
    landmarks[name]=(Vector(head),Vector(tail));parents[name]=parent
bone('root',(0,0,0),(0,0,.3),deform=False)
bone('Thorax',(0,0,2.3),(0,0,3.05),'root')
bone('Head',(0,-1.25,2.5),(0,-2.7,2.03),'Thorax')
bone('Tail_Base',(0,.12,2.9),(0,.85,3.75),'Thorax')
bone('Tail_Mid',(0,.85,3.75),(0,1.8,4.8),'Tail_Base')
bone('Tail_Tip',(0,1.8,4.8),(0,2.6,5.75),'Tail_Mid')
for side,sign in [('L',1),('R',-1)]:
    for index,points in enumerate([
        [(.55,-.62,2.48),(1.35,-.43,2.3),(2.59,-.22,1.6),(2.91,-.06,.055)],
        [(.55,.28,2.45),(1.52,.68,2.28),(2.77,1.03,1.53),(2.88,1.6,.055)],
        [(.43,.87,2.3),(.84,1.54,2.14),(1.08,2.32,1.35),(1.09,2.84,.04)]
    ]):
        points=[Vector((p[0]*sign,p[1],p[2])) for p in points]
        names=[f'Leg_{side}{index+1}_{segment}' for segment in ('Coxa','Upper','Lower')]
        for i,name in enumerate(names):bone(name,points[i],points[i+1],'Thorax' if i==0 else names[i-1])
        toe=f'Foot_{side}{index+1}';bone(toe,points[-1],points[-1]+Vector((0,-.15,0)),names[-1],False)
        legs[(side,index)]={'names':names,'points':points,'toe':toe}
    points=[Vector((x*sign,y,z)) for x,y,z in [(.70,-1.10,2.45),(1.22,-1.35,2.22),(1.97,-1.79,1.76),(1.42,-2.91,.06),(2.44,-2.12,.045)]]
    names=[f'Claw_{side}_{part}' for part in ('Upper','Forearm','Outer','Inner')]
    bone(names[0],points[0],points[1],'Thorax');bone(names[1],points[1],points[2],names[0])
    bone(names[2],points[2],points[3],names[1]);bone(names[3],points[2],points[4],names[1])
    claws[side]={'names':names,'points':points}
bpy.ops.object.mode_set(mode='OBJECT');bpy.context.view_layer.update()
rest={b.name:b.matrix_local.copy() for b in arm.data.bones}
groups={n:obj.vertex_groups.new(name=n) for n in landmarks if arm.data.bones[n].use_deform}
component_assignments={3:legs[('L',0)]['names'],4:legs[('R',0)]['names'],6:legs[('L',1)]['names'],5:legs[('R',1)]['names'],8:legs[('L',2)]['names'],7:legs[('R',2)]['names'],2:claws['L']['names'],1:claws['R']['names']}
def distance(point,a,b):
    direction=b-a;u=max(0,min(1,(point-a).dot(direction)/direction.length_squared))
    return (point-a-u*direction).length
def assign(index,weights):
    for name,w in weights:
        if w>1e-6:groups[name].add([index],w,'REPLACE')
def joint_weights(p,names):
    # Continuous falloff avoids weight seams on the connected limb skins.
    # Six-power distance keeps long chitin segments almost rigid while the
    # knuckles blend, without crossing to other legs or the opposite claw.
    weights=[(n,(distance(p,*landmarks[n])+.12)**-6) for n in names]
    total=sum(w for n,w in weights)
    return [(n,w/total) for n,w in weights]
rigid={9:'Thorax',10:'Thorax',11:'Tail_Mid',12:'Tail_Mid',13:'Tail_Mid',14:'Tail_Base',15:'Tail_Base',16:'Thorax',17:'Tail_Mid',18:'Head',19:'Tail_Mid',20:'Tail_Tip',21:'Head',22:'Head',23:'Head',24:'Head',25:'Head',26:'Thorax',27:'Thorax',28:'Head',29:'Head',30:'Head',31:'Head',32:'Head',33:'Head',34:'Claw_R_Outer',35:'Claw_L_Outer'}
for component,c in enumerate(components):
    for i in c['indices']:
        p=obj.data.vertices[i].co
        if component in component_assignments:weights=joint_weights(p,component_assignments[component])
        elif component==0:
            def ramp(v):v=max(0,min(1,v));return v*v*(3-2*v)
            tail=ramp((p.z-3.1)/.6)*ramp((p.y+.7)/.7)
            head=ramp((-p.y-1.0)/.7)
            if tail>0:weights=[('Thorax',1-tail)]+[(n,w*tail) for n,w in joint_weights(p,['Tail_Base','Tail_Mid','Tail_Tip'])]
            else:weights=[('Thorax',1-head),('Head',head)]
        else:weights=[(rigid[component],1)]
        assign(i,weights)
modifier=obj.modifiers.new('Scorpion skin','ARMATURE');modifier.object=arm
obj.parent=arm;obj.matrix_parent_inverse=Matrix.Identity(4)
assert all(v.groups and len(v.groups)<=4 and abs(sum(g.weight for g in v.groups)-1)<1e-5 for v in obj.data.vertices)
def global_bone(name,head,tail):
    a,b=landmarks[name];rotation=(b-a).rotation_difference(tail-head)@rest[name].to_quaternion()
    matrix=rotation.to_matrix().to_4x4();matrix.translation=head;arm.pose.bones[name].matrix=matrix
def rotate(name,degrees,axis):
    q=rest[name].to_quaternion();arm.pose.bones[name].rotation_quaternion=q.inverted()@Quaternion(Vector(axis),math.radians(degrees))@q
def solve_two(a,end,bend,l1,l2):
    delta=end-a;length=min(l1+l2-.001,max(abs(l1-l2)+.001,delta.length));direction=delta.normalized()
    along=(l1*l1-l2*l2+length*length)/(2*length)
    normal=bend-direction*bend.dot(direction)
    if normal.length<.001:normal=Vector((0,0,1))-direction*direction.z
    return a+direction*along+normal.normalized()*math.sqrt(max(0,l1*l1-along*along))
def smooth(v):v=max(0,min(1,v));return v*v*(3-2*v)
durations={'Idle':2.4,'Walk':.9,'Cast':1.8,'Death':2.8}
def evaluated_points():
    evaluated=obj.evaluated_get(bpy.context.evaluated_depsgraph_get());mesh=evaluated.to_mesh()
    points=[v.co.copy() for v in mesh.vertices];evaluated.to_mesh_clear();return points
def pose_legs(transform,label,t,death):
    for (side,index),leg in legs.items():
        hip,elbow,knee,foot=leg['points'];target=foot.copy()
        if label=='Walk':
            step=(t/durations[label]+(.5 if (index+(side=='R'))%2 else 0))%1
            if step<.6:stride=.62-1.24*step/.6;lift=0
            else:flight=(step-.6)/.4;stride=-.62+1.24*smooth(flight);lift=.25*math.sin(math.pi*flight)
            target.y-=stride;target.z+=lift
        if death:target.x*=1-.15*death;target.y+=.12*death
        a=transform@elbow;h=transform@hip
        end_direction=(foot-elbow).normalized();bend=knee-elbow-end_direction*(knee-elbow).dot(end_direction)
        mid=solve_two(a,target,bend,(knee-elbow).length,(foot-knee).length)
        for n,start,end in zip(leg['names'],[h,a,mid],[a,mid,target]):global_bone(n,start,end);bpy.context.view_layer.update()

def pose(label,t):
    for b in arm.pose.bones:b.rotation_mode='QUATERNION';b.matrix_basis=Matrix.Identity(4)
    bpy.context.view_layer.update()
    phase=2*math.pi*t/durations[label]
    death=smooth((t-.25)/1.4) if label=='Death' else 0
    env=smooth(t/.18)*(1-smooth((t-1.5)/.3)) if label=='Cast' else 0
    offset=Vector((0,0,0));q=Quaternion()
    if label=='Idle':offset.z=.018*math.sin(phase)
    elif label=='Walk':offset=Vector((.018*math.sin(phase),0,-.45+.025*math.sin(phase*2)));q=Quaternion((0,1,0),math.radians(.8*math.sin(phase)))
    elif label=='Cast':offset.z=env*(.05+.025*math.sin(t*math.pi*6));q=Quaternion((0,1,0),math.radians(3*env*math.sin(t*math.pi*6)))
    elif label=='Death':offset=Vector((.16*death,0,-1.3*death));q=Quaternion((0,1,0),math.radians(18*death+3*math.sin(t*25)*(1-death)))
    body=landmarks['Thorax'][0]
    transform=Matrix.Translation(body+offset)@q.to_matrix().to_4x4()@Matrix.Translation(-body)
    arm.pose.bones['Thorax'].matrix=transform@rest['Thorax'];bpy.context.view_layer.update()
    rotate('Head',env*2*math.sin(t*math.pi*4)+death*10,(1,0,0))
    rotate('Tail_Base',env*3*math.sin(t*math.pi*4)-death*18,(1,0,0))
    rotate('Tail_Mid',env*2*math.sin(t*math.pi*4+.7)-death*8,(1,0,0))
    rotate('Tail_Tip',env*3*math.sin(t*math.pi*5)-death*10,(0,1,0))
    bpy.context.view_layer.update()
    pose_legs(transform,label,t,death)
    for side,claw in claws.items():
        shoulder,elbow,wrist,outer,inner=claw['points'];sign=1 if side=='L' else -1
        h=transform@shoulder;target=transform@wrist
        if label=='Walk':target.z+=.45
        target.z+=env*(.12+.045*math.sin(t*math.pi*6+(0 if side=='L' else 1)))
        target.y-=env*.12
        if death:target=target.lerp(Vector((sign*2,-1.75,.85)),death)
        direction=(wrist-shoulder).normalized();bend=elbow-shoulder-direction*(elbow-shoulder).dot(direction)
        mid=solve_two(h,target,bend,(elbow-shoulder).length,(wrist-elbow).length)
        for n,start,end in zip(claw['names'][:2],[h,mid],[mid,target]):global_bone(n,start,end);bpy.context.view_layer.update()
        for n,tip in zip(claw['names'][2:],[outer,inner]):
            d=q@(tip-wrist)
            if death:
                flat=Vector((d.x,d.y,-.35)).normalized()*d.length;d=d.lerp(flat,death).normalized()*d.length
            else:d=Quaternion((0,0,1),math.radians((1 if n.endswith('Outer') else -1)*sign*env*3*math.sin(t*math.pi*4)))@d
            global_bone(n,target,target+d);bpy.context.view_layer.update()
    # Keep the shell above the floor throughout the collapse, including claws.
    correction=0
    for _ in range(3):
        lowest=min(p.z for p in evaluated_points())
        if lowest>=-1e-6:break
        correction-=lowest
        arm.pose.bones['root'].location=rest['root'].to_quaternion().inverted()@Vector((0,0,correction))
        bpy.context.view_layer.update()
        # Root floor correction must not lift the supporting feet. Re-solve
        # the six leg chains with grounded targets after moving the shell.
        pose_legs(Matrix.Translation(Vector((0,0,correction)))@transform,label,t,death)
    # The pointed foot shell can extend a few centimeters beyond its bone
    # endpoint as the ankle tilts. Apply the last small surface clearance.
    lowest=min(p.z for p in evaluated_points())
    if lowest<0:
        correction-=lowest
        arm.pose.bones['root'].location=rest['root'].to_quaternion().inverted()@Vector((0,0,correction))
    bpy.context.view_layer.update()

scene.render.fps=30;scene.frame_start=1;scene.render.resolution_x=900;scene.render.resolution_y=900
camera=scene.camera;camera.data.ortho_scale=8.3;center=Vector((0,0,2.8));camera.location=(9,-11,8)
camera.rotation_euler=(center-camera.location).to_track_quat('-Z','Y').to_euler()
obj.data.calc_loop_triangles()
report={'vertices':len(obj.data.vertices),'triangles':len(obj.data.loop_triangles),'bones':len(arm.data.bones),'max_influences':max(len(v.groups) for v in obj.data.vertices),'unweighted':sum(not v.groups for v in obj.data.vertices),'anatomy':{'legs':6,'claws':2,'tail_bones':3},'clips':{}}
export=dict(use_selection=True,object_types={'ARMATURE'},add_leaf_bones=False,bake_anim=True,bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,bake_anim_force_startend_keying=True,bake_anim_simplify_factor=0,axis_forward='-Z',axis_up='Y',primary_bone_axis='Y',secondary_bone_axis='X')
for label,duration in durations.items():
    arm.animation_data_create();arm.animation_data.action=bpy.data.actions.new('PaleStinger_'+label)
    count=round(duration*30);scene.frame_end=count+1
    for i in range(count+1):
        scene.frame_set(i+1);pose(label,duration*i/count)
        for b in arm.pose.bones:
            b.keyframe_insert('location',frame=i+1);b.keyframe_insert('rotation_quaternion',frame=i+1);b.keyframe_insert('scale',frame=i+1)
    arm.animation_data.action.use_fake_user=True
    bpy.ops.object.select_all(action='DESELECT');arm.select_set(True);bpy.context.view_layer.objects.active=arm
    bpy.ops.export_scene.fbx(filepath=str(ART/('PaleStinger_'+label+'.fbx')),**export)
    samples=[];first=None;last=None;worst=0
    for fraction in (0,.125,.25,.375,.5,.625,.75,.875,1):
        scene.frame_set(round(1+count*fraction));points=evaluated_points()
        if fraction==0:first=points
        if fraction==1:last=points
        minimum=min(p.z for p in points);maximum=max(p.z for p in points)
        assert all(math.isfinite(v) for p in points for v in p)
        assert minimum>-.015,(label,'Floor penetration',minimum)
        for e in obj.data.edges:
            a,b=e.vertices;original=(obj.data.vertices[a].co-obj.data.vertices[b].co).length
            if original>.005:worst=max(worst,(points[a]-points[b]).length/original)
        samples.append(dict(fraction=fraction,time=(scene.frame_current-1)/30,min_z=minimum,max_z=maximum,
            bones_cm={n:list(arm.pose.bones[n].matrix.translation*100) for n in ['root','Thorax','Head','Foot_L1','Foot_R2','Tail_Tip','Claw_L_Outer']}))
    loop=max((a-b).length for a,b in zip(first,last)) if label in ('Idle','Walk') else None
    if loop is not None:assert loop<.001,(label,'Loop seam',loop)
    assert worst<4.5,(label,'Excessive skin stretching',worst)
    report['clips'][label]=dict(duration=duration,frames=count+1,samples=samples,loop_seam=loop,max_edge_stretch=worst)
    for fraction in ((.2,.65,1) if label=='Death' else (.25,.65)):
        scene.frame_set(round(1+count*fraction));scene.render.filepath=str(OUT/(label+'_'+str(int(fraction*100))+'.png'));bpy.ops.render.render(write_still=True)
arm.animation_data.action=None
for b in arm.pose.bones:b.matrix_basis=Matrix.Identity(4)
bpy.context.view_layer.update();scene.frame_start=1;scene.frame_end=85
bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);arm.select_set(True);bpy.context.view_layer.objects.active=arm
bpy.ops.wm.save_as_mainfile(filepath=str(ART/'PaleStinger_Rigged.blend'))
mesh_export=export|dict(object_types={'MESH','ARMATURE'},bake_anim=False,mesh_smooth_type='FACE',path_mode='COPY',embed_textures=False)
bpy.ops.export_scene.fbx(filepath=str(ART/'SK_PaleStinger.fbx'),**mesh_export)
(OUT/'RiggingReport.json').write_text(json.dumps(report,indent=2))
print('SCORPION_RIG_COMPLETE '+json.dumps(report),flush=True)
