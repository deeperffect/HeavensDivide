"""Blender background: authored island geometry and route mask. No generated AI art."""
import bpy,json,math
import numpy as np
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Art/Environment/OldVillage/Ground';OUT.mkdir(parents=True,exist_ok=True)
cfg=json.loads((ROOT/'Tools/old_village_layout.json').read_text())
bpy.ops.wm.read_factory_settings(use_empty=True)
def mesh(name,polygon,z,depth=0):
    verts=[(x/100,y/100,z/100) for x,y in polygon];n=len(verts)
    faces=[tuple(reversed(range(n)))] # polygons here run clockwise
    if depth:
        verts += [(x/100,y/100,(z-depth)/100) for x,y in polygon]
        faces += [tuple(range(n,2*n))]+[(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
    data=bpy.data.meshes.new(name);data.from_pydata(verts,[],faces);data.update()
    obj=bpy.data.objects.new(name,data);bpy.context.collection.objects.link(obj)
    bpy.context.view_layer.objects.active=obj;obj.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.mesh.normals_make_consistent(inside=False);bpy.ops.uv.smart_project();bpy.ops.object.mode_set(mode='OBJECT')
    bpy.ops.export_scene.fbx(filepath=str(OUT/(name+'.fbx')),use_selection=True,object_types={'MESH'},bake_anim=False,axis_forward='-Z',axis_up='Y',apply_unit_scale=True)
    obj.select_set(False)
mesh('SM_OV_Island',cfg['land'],0,250)
mesh('SM_OV_Water',[[-9800,-9800],[-9800,-2500],[-6500,-2500],[-2500,-6500],[-2500,-9800]],-100)
N=2048
east,north=np.meshgrid(np.linspace(-6500,6500,N),np.linspace(-6500,6500,N))
def polygon(points):
    inside=np.zeros((N,N),dtype=bool)
    for (x1,y1),(x2,y2) in zip(points,points[1:]+points[:1]):
        inside ^= ((y1>north)!=(y2>north)) & (east < (x2-x1)*(north-y1)/(y2-y1+1e-12)+x1)
    return inside.astype(float)
def smooth(a,b,v):
    t=np.clip((v-a)/(b-a),0,1);return t*t*(3-2*t)
noise=12*np.sin(east/83)*np.sin(north/117)+9*np.sin(east/37+north/48)
grass=(1-smooth(-3000,-2750,east+noise))
grass=np.maximum(grass,polygon([[-1300,4100],[-1300,4600],[-400,4600],[-400,3900]]))
grass=np.maximum(grass,polygon([[2400,3900],[3400,3900],[3700,4600],[2200,4600]]))
stone=polygon(cfg['court'])*.97
for line in cfg['roads']:
    for (x1,y1),(x2,y2) in zip(line,line[1:]):
        dx=x2-x1;dy=y2-y1;t=np.clip(((east-x1)*dx+(north-y1)*dy)/(dx*dx+dy*dy),0,1)
        d=np.sqrt((east-x1-t*dx)**2+(north-y1-t*dy)**2)+noise
        stone=np.maximum(stone,1-smooth(360,440,d))
stone=np.maximum(stone,1-smooth(1020,1080,np.hypot(east+200,north)))
# Garden walking trails leave two spacious lawns.
for x1,y1,x2,y2 in [(-4550,4700,-3700,4100),(-3700,4100,-3700,2800),(-3700,2800,-4200,2300),(-4200,2300,-4200,-1600),(-6200,1300,-2650,1300),(-4200,-1600,-3000,-2000)]:
    dx=x2-x1;dy=y2-y1;t=np.clip(((east-x1)*dx+(north-y1)*dy)/(dx*dx+dy*dy),0,1)
    d=np.hypot(east-x1-t*dx,north-y1-t*dy)+noise*.4
    grass*=smooth(100,150,d)
grass*=1-stone
# Green geometric beds in the court; the court remains predominantly open.
for cx,cy,rx,ry in [(-650,3000,430,900),(2700,3500,500,400)]:
    g=1-smooth(.82,1,np.sqrt(((east-cx)/rx)**2+((north-cy)/ry)**2))
    grass=np.maximum(grass,g);stone*=1-g
training=polygon(cfg['training']);stone*=1-training;grass*=1-training
rgba=np.stack((grass,np.maximum(0,1-grass-stone),stone,np.ones_like(stone)),axis=-1).astype(np.float32)
img=bpy.data.images.new('T_OV_GroundWeights',width=N,height=N,alpha=True);img.colorspace_settings.name='Non-Color';img.pixels.foreach_set(rgba.ravel());img.filepath_raw=str(OUT/'T_OV_GroundWeights.png');img.file_format='PNG';img.save()
print('OLD_VILLAGE_GROUND_READY',flush=True)
