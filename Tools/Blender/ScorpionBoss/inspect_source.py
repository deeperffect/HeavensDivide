"""Stage and inspect the supplied scorpion without changing Desktop originals."""
import bpy
import hashlib
import json
import shutil
from pathlib import Path
from mathutils import Vector, Matrix

ROOT=Path(r'C:\Users\deepe\OneDrive\Documents\Unreal Projects\HeavensDivide')
SOURCE=Path(r'C:\Users\deepe\OneDrive\Desktop\EnemyScorpion')
ART=ROOT/'Art/Enemies/ScorpionBoss'
OUT=ROOT/'Saved/ScorpionBoss'
ART.mkdir(parents=True,exist_ok=True);OUT.mkdir(parents=True,exist_ok=True)
for name in ('Enemy_BossScorpion.fbx','Enemy_BossScorpion_base.jpg'):
    shutil.copy2(SOURCE/name,ART/name)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(ART/'Enemy_BossScorpion.fbx'))
meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
points=[o.matrix_world@v.co for o in meshes for v in o.data.vertices]
low=Vector(tuple(min(p[i] for p in points) for i in range(3)))
high=Vector(tuple(max(p[i] for p in points) for i in range(3)))
scale=6/max(high-low);origin=Vector(((low.x+high.x)/2,(low.y+high.y)/2,low.z))
for o in meshes:
    world=o.matrix_world.copy()
    for v in o.data.vertices:v.co=(world@v.co-origin)*scale
    o.matrix_world=Matrix.Identity(4)
image=bpy.data.images.load(str(ART/'Enemy_BossScorpion_base.jpg'),check_existing=True)
mat=bpy.data.materials.new('M_ScorpionSource');mat.use_nodes=True
shader=mat.node_tree.nodes.get('Principled BSDF');shader.inputs['Roughness'].default_value=.72
tex=mat.node_tree.nodes.new('ShaderNodeTexImage');tex.image=image
mat.node_tree.links.new(tex.outputs['Color'],shader.inputs['Base Color'])
report={'source_sha256':{n:hashlib.sha256((SOURCE/n).read_bytes()).hexdigest() for n in ('Enemy_BossScorpion.fbx','Enemy_BossScorpion_base.jpg')},'original_bounds':[list(low),list(high)],'normalize_scale':scale,'meshes':[],'armatures':[o.name for o in bpy.context.scene.objects if o.type=='ARMATURE']}
for o in meshes:
    o.data.materials.clear();o.data.materials.append(mat);o.data.calc_loop_triangles()
    neighbors=[set() for v in o.data.vertices]
    for e in o.data.edges:a,b=e.vertices;neighbors[a].add(b);neighbors[b].add(a)
    unseen=set(range(len(neighbors)));components=[]
    while unseen:
        todo=[unseen.pop()];indices=[]
        while todo:
            i=todo.pop();indices.append(i)
            for j in neighbors[i]:
                if j in unseen:unseen.remove(j);todo.append(j)
        pts=[o.data.vertices[i].co for i in indices]
        components.append(dict(indices=indices,count=len(indices),center=list(sum(pts,Vector())/len(pts)),bounds=[[min(p[k] for p in pts) for k in range(3)],[max(p[k] for p in pts) for k in range(3)]]))
    components.sort(key=lambda c:-c['count'])
    report['meshes'].append(dict(name=o.name,vertices=len(o.data.vertices),triangles=len(o.data.loop_triangles),uv_layers=[v.name for v in o.data.uv_layers],groups=[v.name for v in o.vertex_groups],components=components))
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=12;scene.cycles.use_denoising=True
scene.render.resolution_x=900;scene.render.resolution_y=900;scene.render.resolution_percentage=100
scene.world=bpy.data.worlds.new('Studio');scene.world.use_nodes=True
scene.world.node_tree.nodes['Background'].inputs['Color'].default_value=(.055,.07,.09,1)
scene.world.node_tree.nodes['Background'].inputs['Strength'].default_value=.6
scene.view_settings.view_transform='AgX'
center=Vector((0,0,1.4))
def aim(o):o.rotation_euler=(center-o.location).to_track_quat('-Z','Y').to_euler()
for name,location,energy in [('Key',(-5,-6,8),1800),('Fill',(6,-3,5),1200),('Rim',(0,6,7),2200)]:
    light=bpy.data.objects.new(name,bpy.data.lights.new(name,'AREA'));scene.collection.objects.link(light)
    light.location=location;light.data.energy=energy;light.data.shape='DISK';light.data.size=5;aim(light)
camera=bpy.data.objects.new('Camera',bpy.data.cameras.new('Camera'));scene.collection.objects.link(camera);scene.camera=camera
camera.data.type='ORTHO';camera.data.ortho_scale=7.5
for view,location in [('Top',(0,0,12)),('Front',(0,-12,1.4)),('Side',(12,0,1.4)),('Perspective',(8,-10,9))]:
    camera.location=location;aim(camera);scene.render.filepath=str(OUT/('Source_'+view+'.png'));bpy.ops.render.render(write_still=True)
image.pack();bpy.ops.wm.save_as_mainfile(filepath=str(ART/'Scorpion_Source.blend'))
(OUT/'SourceInspection.json').write_text(json.dumps(report,indent=2))
print('SCORPION_SOURCE_INSPECTED',json.dumps({**report,'meshes':[{k:v for k,v in m.items() if k!='components'}|{'component_counts':[c['count'] for c in m['components']]} for m in report['meshes']]}),flush=True)
