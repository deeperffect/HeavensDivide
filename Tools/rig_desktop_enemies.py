"""Fit joint axes to Desktop models, skin, author restrained animations and export.
Source FBXs/textures and existing Unreal skeletons are never modified.
"""
import bpy,json,math,shutil,sys,os
from pathlib import Path
from mathutils import Vector,Matrix,Quaternion
from mathutils.kdtree import KDTree
ROOT=Path(r'C:\Users\deepe\OneDrive\Documents\Unreal Projects\HeavensDivide')
OUT=ROOT/'Saved'/os.environ.get('BLOODSHIFT_ENEMY_WORKSPACE','EnemyModelReplacement');ART=ROOT/'Art/Enemies'
DONOR_OUT=ROOT/'Saved/EnemyModelReplacement'
donors=json.loads((DONOR_OUT/'DonorRigs.json').read_text())
sys.path.insert(0,str(ROOT/'Tools'))
from enemy_geodesic_skin import skin as surface_skin
C=Matrix.Diagonal((1,-1,1,1));TURN=Matrix.Rotation(-math.pi/2,4,'Z')
def mat(t,q):
    m=Quaternion((q[3],q[0],q[1],q[2])).to_matrix().to_4x4();m.translation=Vector(t)/100;return m
def rigid(m):
    r=m.to_quaternion().to_matrix().to_4x4();r.translation=m.translation;return r
def chain(base):
    result={}
    for n,values in base.items():
        if n.startswith('LR'):
            for side,sgn in [('Left',1),('Right',-1)]:result[side+n[2:]]=Vector((values[0]*sgn,values[1],values[2]))
        else:result[n]=Vector(values)
    return result
LANDMARKS={
'AshSeer':chain({'Hips':(0,.04,.435),'Spine':(0,.025,.49),'Spine1':(0,.018,.54),'Spine2':(0,.012,.59),'Neck':(0,0,.65),'Head':(0,-.01,.70),'HeadTop_End':(0,0,.92),'LRShoulder':(.075,.012,.64),'LRArm':(.142,.015,.61),'LRForeArm':(.193,-.003,.455),'LRHand':(.220,-.035,.335),'LRUpLeg':(.073,.04,.406),'LRLeg':(.103,.04,.252),'LRFoot':(.132,.015,.065),'LRToeBase':(.135,-.07,.033),'LRToe_End':(.135,-.13,.025)}),
'OgreWarden':chain({'Hips':(0,.065,.435),'Spine':(0,.055,.52),'Spine1':(0,.04,.60),'Spine2':(0,.005,.69),'Neck':(0,-.06,.80),'Head':(0,-.11,.845),'HeadTop_End':(0,-.075,.96),'LRShoulder':(.10,.005,.755),'LRArm':(.200,.005,.745),'LRForeArm':(.235,-.028,.56),'LRHand':(.252,-.10,.29),'LRUpLeg':(.085,.07,.395),'LRLeg':(.105,.10,.245),'LRFoot':(.14,.07,.072),'LRToeBase':(.145,-.04,.028),'LRToe_End':(.145,-.12,.025)}),
'HexSniper':chain({'Hips':(-.013,.025,.435),'Spine':(-.006,.01,.50),'Spine1':(.006,0,.56),'Spine2':(.017,0,.625),'Neck':(.018,-.008,.70),'Head':(.018,-.028,.75),'HeadTop_End':(.018,-.01,.90),'LRShoulder':(.061,.005,.658),'LRArm':(.108,.007,.639),'LRForeArm':(.163,-.025,.512),'LRHand':(.22,-.045,.39),'LRUpLeg':(.064,.035,.395),'LRLeg':(.105,.045,.209),'LRFoot':(.157,.038,.045),'LRToeBase':(.16,-.035,.025),'LRToe_End':(.16,-.09,.02)})}
LANDMARKS['HexSniper']['LeftArm']=Vector((.12,.009,.661))
LANDMARKS['HexSniper']['RightArm']=Vector((-.09,.006,.635))
LANDMARKS['GraveCantor']=chain({'Hips':(0,.025,.585),'Spine':(0,.023,.63),'Spine1':(0,.02,.672),
    'Spine2':(0,.015,.718),'Neck':(0,.002,.775),'Head':(0,-.027,.821),'HeadTop_End':(0,.02,.96),
    'LRShoulder':(.05,.013,.744),'LRArm':(.085,.014,.72),'LRForeArm':(.143,.002,.635),
    'LRHand':(.194,-.019,.535),'LRUpLeg':(.051,.025,.555),'LRLeg':(.075,.012,.30),
    'LRFoot':(.092,-.013,.045),'LRToeBase':(.096,-.085,.018),'LRToe_End':(.096,-.13,.011)})
END={'Hips':'Spine','Spine':'Spine1','Spine1':'Spine2','Spine2':'Neck','Neck':'Head','Head':'HeadTop_End'}
for side in ['Left','Right']:
    END.update({side+'Shoulder':side+'Arm',side+'Arm':side+'ForeArm',side+'ForeArm':side+'Hand',side+'UpLeg':side+'Leg',side+'Leg':side+'Foot',side+'Foot':side+'ToeBase',side+'ToeBase':side+'Toe_End'})
report=json.loads((OUT/'RiggingReport.json').read_text()) if (OUT/'RiggingReport.json').exists() else {}
selected=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else ['AshSeer','OgreWarden','HexSniper']
for name in selected:
    dest=ART/name;dest.mkdir(parents=True,exist_ok=True)
    donor='OgreWarden' if name=='OgreWarden' else 'AshSeer'
    bpy.ops.wm.open_mainfile(filepath=str(OUT/(name+'_Source.blend')))
    scene=bpy.context.scene;obj=next(o for o in scene.objects if o.type=='MESH');obj.name='SK_'+name
    # Normalize the Sniper's source-object heading before fitting its body joints.
    m=obj.matrix_world.copy()
    if name=='HexSniper':m=Matrix.Rotation(-math.atan2(m[1][0],m[0][0]),4,'Z')@m
    for v in obj.data.vertices:v.co=TURN@m@v.co
    obj.matrix_world=Matrix.Identity(4)
    source_points=[TURN.inverted()@v.co for v in obj.data.vertices]
    texture=next(n.image for n in obj.data.materials[0].node_tree.nodes if n.type=='TEX_IMAGE')
    shutil.copy2(Path(bpy.path.abspath(texture.filepath)),dest/(name+'_BaseColor.jpg'))
    texture.filepath=str(dest/(name+'_BaseColor.jpg'));texture.pack()
    for o in scene.objects:
        if o.type=='LIGHT':o.data.energy*=1 if name=='GraveCantor' else .12;o.location=TURN@o.location;o.rotation_euler=(Vector((0,0,.5))-o.location).to_track_quat('-Z','Y').to_euler()
    scene.view_settings.view_transform='AgX';scene.cycles.samples=12
    with bpy.data.libraries.load(str(DONOR_OUT/(donor+'_Donor.blend')),link=False) as (src,dst):dst.objects=[n for n in src.objects if n=='Hips']
    arm=dst.objects[0];scene.collection.objects.link(arm);arm.name='Armature';arm.show_in_front=True
    bpy.ops.object.select_all(action='DESELECT');arm.select_set(True);bpy.context.view_layer.objects.active=arm
    bpy.ops.object.mode_set(mode='EDIT')
    roots=[b for b in arm.data.edit_bones if not b.parent]
    root=arm.data.edit_bones.new('Hips');root.head=(0,0,0);root.tail=(0,5,0)
    for b in roots:b.parent=root
    bpy.ops.object.mode_set(mode='OBJECT');bpy.context.view_layer.update()
    old_bworld={b.name:rigid(arm.matrix_world@b.matrix_local) for b in arm.data.bones}
    ref={b['name']:b for b in donors[donor]['bones']}
    order=[b['name'] for b in donors[donor]['bones']]
    corrections={n:(C@mat(ref[n]['world']['translation'],ref[n]['world']['rotation'])@C).inverted()@old_bworld[n] for n in order}
    positions={n:TURN@p for n,p in LANDMARKS[name].items()}
    # Keep finger axes/hierarchy; scale their offsets with the fitted hand.
    for n in order:
        if n not in positions:
            parent=ref[n]['parent']
            positions[n]=positions[parent]+(old_bworld[n].translation-old_bworld[parent].translation)*(.8 if name!='OgreWarden' else 1.2)
    bpy.ops.object.mode_set(mode='EDIT')
    axes={n:(arm.data.edit_bones[n].tail-arm.data.edit_bones[n].head,arm.data.edit_bones[n].roll) for n in order}
    for b in arm.data.edit_bones:b.use_connect=False
    for n in order:
        b=arm.data.edit_bones[n];b.head=arm.matrix_world.inverted()@positions[n];b.tail=b.head+axes[n][0];b.roll=axes[n][1]
    bpy.ops.object.mode_set(mode='OBJECT');bpy.context.view_layer.update()
    new_ue_world={n:C@rigid(arm.matrix_world@arm.data.bones[n].matrix_local)@corrections[n].inverted()@C for n in order}
    newlocal={}
    for n in order:
        parent=ref[n]['parent'];newlocal[n]=new_ue_world[parent].inverted()@new_ue_world[n] if parent!='None' else new_ue_world[n]
        qref=Quaternion((ref[n]['local']['rotation'][3],*ref[n]['local']['rotation'][:3]))
        assert abs(newlocal[n].to_quaternion().dot(qref))>.99999,(name,n,'Joint axes changed during fitting')
    # Heat weighting uses anatomically aligned segments, independent of the legacy animation axes.
    proxy=bpy.data.objects.new('WeightingProxy',bpy.data.armatures.new('WeightingProxy'));scene.collection.objects.link(proxy)
    bpy.ops.object.select_all(action='DESELECT');proxy.select_set(True);bpy.context.view_layer.objects.active=proxy;bpy.ops.object.mode_set(mode='EDIT')
    deform=list(END)+['LeftHand','RightHand']
    for n in deform:
        b=proxy.data.edit_bones.new(n);b.head=positions[n]
        b.tail=positions[END[n]] if n in END else positions[n]+TURN@Vector((0,-.015,-.085 if name!='OgreWarden' else -.11))
        if (b.tail-b.head).length<.01:b.tail.z+=.03
    for n in deform:
        p=ref[n]['parent']
        if p in deform:proxy.data.edit_bones[n].parent=proxy.data.edit_bones[p]
    bpy.ops.object.mode_set(mode='OBJECT')
    bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);proxy.select_set(True);bpy.context.view_layer.objects.active=proxy
    bpy.ops.object.parent_set(type='ARMATURE_AUTO')
    obj.parent=None;obj.matrix_world=Matrix.Identity(4)
    for m in list(obj.modifiers):obj.modifiers.remove(m)
    valid=[v for v in obj.data.vertices if sum(g.weight for g in v.groups)>.01]
    assert len(valid)>len(obj.data.vertices)*.8,(name,'Heat weighting failed')
    nearest=KDTree(len(valid))
    original_weights={v.index:[(obj.vertex_groups[g.group].name,g.weight) for g in v.groups] for v in valid}
    for v in valid:nearest.insert(v.co,v.index)
    nearest.balance();repaired=0
    def weight(index,values):
        for g in list(obj.data.vertices[index].groups):obj.vertex_groups[g.group].remove([index])
        for n,w in values:
            if n not in obj.vertex_groups:obj.vertex_groups.new(name=n)
            if w>1e-5:obj.vertex_groups[n].add([index],w,'REPLACE')
    for v,p in zip(obj.data.vertices,source_points):
        # Rigid cranial shells, claws and weapons must not stretch with torso/leg weights.
        side='Left' if p.x>=0 else 'Right'
        if p.z<.085:weight(v.index,[(side+'Foot',1)])
        elif (name=='AshSeer' and (p.z>.685 or (p.z>.625 and abs(p.x)<.17))) or (name=='HexSniper' and p.z>.735):weight(v.index,[('Head',1)])
        elif name=='OgreWarden' and p.z>.82 and p.y<-.06 and abs(p.x)<.135:weight(v.index,[('Head',1)])
        elif name=='HexSniper' and abs(p.x)>.19 and p.z<.425 and p.z>.20:weight(v.index,[(side+'Hand',1)])
        elif name=='AshSeer' and abs(p.x)>.195 and .19<p.z<.335:weight(v.index,[(side+'Hand',1)])
        elif name=='OgreWarden' and abs(p.x)>.20 and .09<p.z<.29 and p.y<-.04:weight(v.index,[(side+'Hand',1)])
        elif name=='HexSniper' and abs(p.x)<.045 and .19<p.z<.44 and p.y<-.05:weight(v.index,[('Hips',1)])
        entries=sorted([(obj.vertex_groups[g.group].name,g.weight) for g in v.groups if g.weight>.004],key=lambda x:-x[1])[:4]
        # Heat diffusion can cross the narrow gap between long hands and robes/feet.
        # Reject those anatomically impossible cross-limb influences.
        if (abs(p.x)<.17 and p.z<.50) or (name=='OgreWarden' and p.z<.135) or (name=='HexSniper' and abs(p.x)<.19 and p.z<.45):
            entries=[(n,w) for n,w in entries if 'Hand' not in n and 'Arm' not in n and 'Shoulder' not in n]
            if name=='OgreWarden' and p.z<.135:entries=[(side+'Foot',1)]
            if name=='HexSniper' and abs(p.x)<.07 and .15<p.z<.44 and p.y<-.065:entries=[('Hips',1)]
            if not entries:entries=[('Hips' if p.z>.34 else side+'UpLeg' if p.z>.23 else side+'Leg',1)]
        if not entries:
            # Small disconnected details inherit interpolated skin from nearby body surfaces.
            values={}
            for co,idx,distance in nearest.find_n(v.co,3):
                assert distance<.15,(name,'Detached surface too far from weighted body',v.index,distance)
                for n,w in original_weights[idx]:values[n]=values.get(n,0)+w/max(.003,distance)**2
            entries=sorted(values.items(),key=lambda x:-x[1])[:4];repaired+=1
        total=sum(w for n,w in entries);weight(v.index,[(n,w/total) for n,w in entries])
    bpy.data.objects.remove(proxy,do_unlink=True)
    if name in ['OgreWarden','HexSniper']:surface_skin(obj,arm,name)
    if name=='GraveCantor':
        surface_skin(obj,arm,name)
        # Treat the long kimono as a single hanging garment. Locomotion is a
        # ghostly glide, so the robe never stretches between opposing leg swings.
        for v,p in zip(obj.data.vertices,source_points):
            side='Left' if p.x>=0 else 'Right'
            if p.z>.802 or (p.z>.71 and p.y>.07):weight(v.index,[('Head',1)])
            elif abs(p.x)>.20 and .35<p.z<.49:weight(v.index,[(side+'Hand',1)])
            elif p.z<.35:
                weight(v.index,[(side+'Foot',1)] if p.z<.065 and abs(p.x)<.14 and p.y<.05 else [('Hips',1)])
    mod=obj.modifiers.new('Enemy skin','ARMATURE');mod.object=arm;obj.parent=arm;obj.matrix_parent_inverse=arm.matrix_world.inverted()
    # Author restrained creature motions in the fitted anatomy. This avoids the
    # legacy donors' extreme wrist/ankle poses and keeps solid armor/weapon pieces stable.
    windup={'AshSeer':1.15,'OgreWarden':1.4,'HexSniper':1.35,'GraveCantor':1.5}[name]
    durations={'Idle':3.2,'Walk':1.6 if name=='GraveCantor' else 1.1 if name=='OgreWarden' else .9,'Cast':windup+.45}
    restrot={b.name:(arm.matrix_world@b.matrix_local).to_quaternion() for b in arm.data.bones}
    def rotate(n,degrees,axis=(0,1,0)):
        q=Quaternion(Vector(axis),math.radians(degrees));r=restrot[n]
        arm.pose.bones[n].rotation_quaternion=r.inverted()@q@r
    def offset(n,delta):arm.pose.bones[n].location=restrot[n].inverted()@Vector(delta)/.01
    def smooth(x):x=max(0,min(1,x));return x*x*(3-2*x)
    def pose(label,t):
        for b in arm.pose.bones:b.rotation_mode='QUATERNION';b.matrix_basis=Matrix.Identity(4)
        if label=='Idle':
            s=math.sin(2*math.pi*t/durations[label]);offset('Hips',(0,0,.003*s));rotate('Spine2',1.2*s);rotate('Head',-.7*s)
            rotate('LeftArm',1.3*s);rotate('RightArm',-1.3*s)
        elif label=='Walk' and name=='GraveCantor':
            phase=2*math.pi*t/durations[label];s=math.sin(phase)
            offset('Hips',(0,.003*s,.012*(1-math.cos(phase))))
            rotate('Spine2',-3+1.2*s);rotate('Head',1.5-.8*s)
            rotate('LeftArm',5+2*s);rotate('RightArm',5-2*s)
            rotate('LeftForeArm',3);rotate('RightForeArm',3)
        elif label=='Walk':
            phase=2*math.pi*t/durations[label];s=math.sin(phase)
            offset('Hips',(0,.004*s,.008*(1-math.cos(2*phase))))
            rotate('Spine2',-3);rotate('Head',2)
            for side,sign in [('Left',1),('Right',-1)]:
                hip=17*s*sign;knee=-24*max(0,math.cos(phase)*sign)
                rotate(side+'UpLeg',hip);rotate(side+'Leg',knee);rotate(side+'Foot',-hip-knee)
                rotate(side+'Arm',-9*s*sign);rotate(side+'ForeArm',4*max(0,-s*sign))
        else:
            lift=smooth(t/(windup*.7))*(1-smooth((t-windup)/.40))
            strike=smooth((t-windup+.08)/.12)*(1-smooth((t-windup-.05)/.28))
            if name=='OgreWarden':
                rotate('LeftArm',78*lift-32*strike);rotate('RightArm',78*lift-32*strike)
                rotate('LeftForeArm',24*lift);rotate('RightForeArm',24*lift)
                rotate('Spine2',6*lift-15*strike);rotate('Head',-3*lift+7*strike)
                offset('Hips',(0,0,-.02*strike))
            elif name=='GraveCantor':
                rotate('LeftArm',38*lift);rotate('RightArm',38*lift)
                rotate('LeftForeArm',25*lift);rotate('RightForeArm',25*lift)
                rotate('LeftHand',-10*lift);rotate('RightHand',-10*lift)
                rotate('Spine2',-3*lift);rotate('Head',4*lift)
                offset('Hips',(0,0,.014*lift))
            else:
                rotate('LeftArm',(34 if name=='AshSeer' else 42)*lift)
                rotate('RightArm',(34 if name=='AshSeer' else 22)*lift)
                rotate('LeftForeArm',30*lift);rotate('RightForeArm',30*lift)
                rotate('Spine2',-4*lift);rotate('Head',2*lift)
                rotate('LeftHand',-12*lift);rotate('RightHand',-12*lift)
        bpy.context.view_layer.update()
    # Save editable, looping locomotion and a cast clip timed to the ability windup.
    scene.render.fps=30;scene.frame_start=1
    for label,duration in durations.items():
        arm.animation_data_create();arm.animation_data.action=bpy.data.actions.new(name+'_'+label)
        count=round(duration*30)
        for i in range(count+1):
            scene.frame_set(i+1);pose(label,duration*i/count)
            for b in arm.pose.bones:
                b.rotation_mode='QUATERNION';b.keyframe_insert('location',frame=i+1);b.keyframe_insert('rotation_quaternion',frame=i+1);b.keyframe_insert('scale',frame=i+1)
        arm.animation_data.action.use_fake_user=True
        scene.frame_end=count+1
        bpy.ops.object.select_all(action='DESELECT');arm.select_set(True);bpy.context.view_layer.objects.active=arm
        bpy.ops.export_scene.fbx(filepath=str(dest/(name+'_'+label+'.fbx')),use_selection=True,object_types={'ARMATURE'},add_leaf_bones=False,bake_anim=True,bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,bake_anim_force_startend_keying=True,bake_anim_simplify_factor=0,axis_forward='-Z',axis_up='Y',primary_bone_axis='Y',secondary_bone_axis='X')
        scene.frame_set(1+int(count*(.65 if label=='Cast' else .25)))
        center=Vector((0,0,.50));camera=scene.camera;camera.location=(-3,-.6,.7);camera.rotation_euler=(center-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.ortho_scale=1.25
        scene.render.filepath=str(OUT/(name+'_'+label+'_Rigged.png'));bpy.ops.render.render(write_still=True)
    arm.animation_data.action=None
    for b in arm.pose.bones:b.matrix_basis=Matrix.Identity(4)
    bpy.context.view_layer.update()
    scene.render.filepath=str(OUT/(name+'_RiggedRest.png'));bpy.ops.render.render(write_still=True)
    bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);arm.select_set(True);bpy.context.view_layer.objects.active=arm
    bpy.ops.wm.save_as_mainfile(filepath=str(dest/('Bloodshift_'+name+'_Rigged.blend')))
    bpy.ops.export_scene.fbx(filepath=str(dest/('SK_'+name+'.fbx')),use_selection=True,object_types={'MESH','ARMATURE'},add_leaf_bones=False,bake_anim=False,mesh_smooth_type='FACE',axis_forward='-Z',axis_up='Y',primary_bone_axis='Y',secondary_bone_axis='X',path_mode='COPY',embed_textures=False)
    obj.data.calc_loop_triangles()
    report[name]={'donor':donor,'vertices':len(obj.data.vertices),'triangles':len(obj.data.loop_triangles),'bones':len(arm.data.bones),'max_influences':max(len(v.groups) for v in obj.data.vertices),'unweighted':sum(not v.groups for v in obj.data.vertices),'detached_vertices_repaired':repaired,'new_local':{n:{'t':list(m.translation*100),'q':list(m.to_quaternion())} for n,m in newlocal.items()}}
    (OUT/'RiggingReport.json').write_text(json.dumps(report,indent=2))
    print('ENEMY_RIG_COMPLETE',name,report[name]['vertices'],flush=True)
