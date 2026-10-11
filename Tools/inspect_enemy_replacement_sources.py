"""Inspect and preview the Desktop enemy meshes in Blender without editing them."""
import bpy, json
from pathlib import Path
from mathutils import Vector

ROOT=Path(r'C:\Users\deepe\OneDrive\Documents\Unreal Projects\HeavensDivide')
SOURCE=Path(r'C:\Users\deepe\OneDrive\Desktop\Enemies')
OUT=ROOT/'Saved/EnemyModelReplacement';OUT.mkdir(parents=True,exist_ok=True)
report={}
for name in ['AshSeer','OgreWarden','HexSniper']:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(SOURCE/name/(name+'.fbx')))
    meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
    points=[o.matrix_world@v.co for o in meshes for v in o.data.vertices]
    low=Vector([min(p[i] for p in points) for i in range(3)])
    high=Vector([max(p[i] for p in points) for i in range(3)])
    report[name]={'bounds':[list(low),list(high)],'objects':[]}
    for o in bpy.context.scene.objects:
        r={'name':o.name,'type':o.type,'matrix':[list(row) for row in o.matrix_world]}
        if o.type=='MESH':
            o.data.calc_loop_triangles()
            r.update(vertices=len(o.data.vertices),triangles=len(o.data.loop_triangles),uv_layers=[l.name for l in o.data.uv_layers],groups=[g.name for g in o.vertex_groups])
        if o.type=='ARMATURE':r['bones']=[{'name':b.name,'head':list(o.matrix_world@b.head_local),'tail':list(o.matrix_world@b.tail_local),'parent':b.parent.name if b.parent else None} for b in o.data.bones]
        report[name]['objects'].append(r)
    image=bpy.data.images.load(str(SOURCE/name/(name+'_base.jpg')),check_existing=True)
    report[name]['texture_size']=list(image.size)
    mat=bpy.data.materials.new(name+'_Base');mat.use_nodes=True
    bsdf=mat.node_tree.nodes.get('Principled BSDF');bsdf.inputs['Roughness'].default_value=.72
    tex=mat.node_tree.nodes.new('ShaderNodeTexImage');tex.image=image
    mat.node_tree.links.new(tex.outputs['Color'],bsdf.inputs['Base Color'])
    for o in meshes:o.data.materials.clear();o.data.materials.append(mat)
    center=(low+high)*.5;height=high.z-low.z
    scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=8;scene.cycles.use_denoising=True
    scene.render.resolution_x=640;scene.render.resolution_y=768;scene.render.resolution_percentage=100
    scene.view_settings.view_transform='Standard';scene.world=bpy.data.worlds.new('Studio');scene.world.use_nodes=True
    scene.world.node_tree.nodes['Background'].inputs['Color'].default_value=(.09,.11,.14,1)
    scene.world.node_tree.nodes['Background'].inputs['Strength'].default_value=.65
    def aim(o):o.rotation_euler=(center-o.location).to_track_quat('-Z','Y').to_euler()
    for label,offset,energy in [('Key',(-1,-1.5,1),230),('Fill',(1,-1,.4),150),('Rim',(0,1.5,1),260)]:
        light=bpy.data.lights.new(label,'AREA');light.energy=energy*height*height;light.size=height
        obj=bpy.data.objects.new(label,light);scene.collection.objects.link(obj);obj.location=center+Vector(offset)*height;aim(obj)
    camera=bpy.data.objects.new('PreviewCamera',bpy.data.cameras.new('PreviewCamera'));scene.collection.objects.link(camera)
    camera.data.type='ORTHO';camera.data.ortho_scale=max(height*1.15,(high.x-low.x)*1.3);camera.data.clip_start=height*.001;camera.data.clip_end=height*100;scene.camera=camera
    for view,direction in [('Front',(0,-1,0)),('Side',(1,0,0))]:
        camera.location=center+Vector(direction)*height*3;aim(camera)
        scene.render.filepath=str(OUT/(name+'_'+view+'.png'));bpy.ops.render.render(write_still=True)
    image.pack();bpy.ops.wm.save_as_mainfile(filepath=str(OUT/(name+'_Source.blend')))
(OUT/'SourceInspection.json').write_text(json.dumps(report,indent=2))
print('ENEMY_SOURCE_INSPECTION_COMPLETE',json.dumps(report),flush=True)
