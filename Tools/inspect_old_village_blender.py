"""Normalize source previews, export named FBXs, and inspect all supplied models."""
import bpy, json, math
from pathlib import Path
from mathutils import Vector, Matrix

ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'Art/Environment/OldVillage'
OUT=ROOT/'Saved/OldVillage/SourceReview'; OUT.mkdir(parents=True,exist_ok=True)
manifest=json.loads((ART/'SourceManifest.json').read_text())
report=[]
for a in manifest['assets']:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(ROOT/a['original_fbx']))
    objects=[o for o in bpy.context.scene.objects if o.type=='MESH']
    assert objects,a['name']
    points=[o.matrix_world@v.co for o in objects for v in o.data.vertices]
    low=Vector(tuple(min(p[i] for p in points) for i in range(3)))
    high=Vector(tuple(max(p[i] for p in points) for i in range(3)))
    size=high-low
    # FBX importer maps source axes into Blender Z-up. One unit is one meter.
    scale=(a['target_cm']/100)/(size.z if a['scale_axis']=='height' else size.x)
    origin=Vector(((low.x+high.x)/2,(low.y+high.y)/2,low.z))
    rotation=Matrix.Rotation(math.radians(a.get('yaw_degrees',0)),4,'Z')
    for o in objects:
        world=o.matrix_world.copy()
        for v in o.data.vertices:v.co=rotation@((world@v.co-origin)*scale)
        o.matrix_world=Matrix.Identity(4)
    bpy.ops.object.select_all(action='DESELECT')
    for o in objects:o.select_set(True)
    bpy.context.view_layer.objects.active=objects[0]
    bpy.ops.object.join()
    mesh=bpy.context.object;mesh.name='SM_OV_'+a['name'];mesh.data.name=mesh.name
    image=bpy.data.images.load(str(ROOT/a['texture']),check_existing=True)
    mat=bpy.data.materials.new('M_OV_'+a['name']);mat.use_nodes=True
    shader=mat.node_tree.nodes.get('Principled BSDF');shader.inputs['Roughness'].default_value=.9
    tex=mat.node_tree.nodes.new('ShaderNodeTexImage');tex.image=image
    mat.node_tree.links.new(tex.outputs['Color'],shader.inputs['Base Color'])
    mesh.data.materials.clear();mesh.data.materials.append(mat)
    for poly in mesh.data.polygons:poly.material_index=0
    mesh.data.calc_loop_triangles()
    assert len(mesh.data.uv_layers)>0,a['name']
    bpy.ops.export_scene.fbx(filepath=str(ROOT/a['prepared_fbx']),use_selection=True,
        object_types={'MESH'},bake_anim=False,axis_forward='-Z',axis_up='Y',
        apply_unit_scale=True,mesh_smooth_type='FACE',path_mode='RELATIVE')
    size=mesh.dimensions.copy()
    record=dict(name=a['name'],original_bounds=[list(low),list(high)],dimensions_cm=[v*100 for v in size],
        vertices=len(mesh.data.vertices),triangles=len(mesh.data.loop_triangles),
        uv_layers=len(mesh.data.uv_layers),texture_size=list(image.size))
    report.append(record)
    # Emission preview reveals the actual painted texture without lighting bias.
    nt=mat.node_tree;outnode=nt.nodes.get('Material Output')
    emission=nt.nodes.new('ShaderNodeEmission');nt.links.new(tex.outputs['Color'],emission.inputs['Color']);nt.links.new(emission.outputs[0],outnode.inputs['Surface'])
    scene=bpy.context.scene;scene.render.engine='BLENDER_EEVEE'
    scene.render.resolution_x=420;scene.render.resolution_y=420;scene.render.resolution_percentage=100
    scene.render.image_settings.file_format='PNG';scene.render.film_transparent=False
    scene.world=bpy.data.worlds.new('ReviewWorld');scene.world.use_nodes=True
    scene.world.node_tree.nodes['Background'].inputs['Color'].default_value=(.11,.13,.17,1)
    scene.world.node_tree.nodes['Background'].inputs['Strength'].default_value=.5
    scene.view_settings.view_transform='Standard'
    center=Vector((0,0,size.z*.48)); radius=max(size)*1.8
    camera=bpy.data.objects.new('ReviewCamera',bpy.data.cameras.new('ReviewCamera'));scene.collection.objects.link(camera)
    camera.data.type='ORTHO';camera.data.ortho_scale=max(size)*1.48;scene.camera=camera
    for suffix,loc in [('Front',(radius*.8,-radius,size.z*.5+radius*.7)),('Back',(-radius*.8,radius,size.z*.5+radius*.7))]:
        camera.location=loc;camera.rotation_euler=(center-camera.location).to_track_quat('-Z','Y').to_euler()
        scene.render.filepath=str(OUT/(a['name']+'_'+suffix+'.png'));bpy.ops.render.render(write_still=True)
    (OUT/'Inspection.json').write_text(json.dumps(report,indent=2))
    print('OLD_VILLAGE_SOURCE_READY '+a['name'],flush=True)
print('OLD_VILLAGE_ALL_SOURCES_READY',flush=True)
