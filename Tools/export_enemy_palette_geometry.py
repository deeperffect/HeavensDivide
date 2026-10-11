import unreal as u,json
from pathlib import Path
out=Path(u.Paths.project_dir()).resolve()/'Saved/BearPatchwork';out.mkdir(parents=True,exist_ok=True)
Q=u.GeometryScript_MeshQueries
for label,asset in [('Bear','/Game/Assets/EnemyCharacters/Bear/Enemy_Bear'),('Crawler','/Game/HeavensDivide/Blueprints/EnemyCharacters/Mobs/BP_EnemyGoblinCrawler')]:
 obj=u.load_asset(asset)
 mesh=u.get_default_object(obj.generated_class()).get_editor_property('mesh').get_skinned_asset() if isinstance(obj,u.Blueprint) else obj
 dm=u.DynamicMesh();result=u.GeometryScript_AssetUtils.copy_mesh_from_skeletal_mesh(mesh,dm,u.GeometryScriptCopyMeshFromAssetOptions(),u.GeometryScriptMeshReadLOD())
 u.log('CPU_MESH '+label+' '+str(result)+' '+Q.get_mesh_info_string(dm))
 triangles=[]
 for i in range(getattr(Q,next(n for n in dir(Q) if n.replace('_','')=='getnumtriangleids'))(dm)):
  if not Q.is_valid_triangle_id(dm,i):continue
  p=[v for v in Q.get_triangle_positions(dm,i) if isinstance(v,u.Vector)]
  uv=[v for v in getattr(Q,next(n for n in dir(Q) if n.replace('_','')=='gettriangleuvs'))(dm,0,i) if isinstance(v,u.Vector2D)]
  assert len(p)==3 and len(uv)==3
  triangles.append({'p':[[v.x,v.y,v.z] for v in p],'uv':[[v.x,v.y] for v in uv]})
 assert triangles
 (out/(label+'_Geometry.json')).write_text(json.dumps(triangles,separators=(',',':')))
 u.log('CPU_MESH_EXPORTED '+label+' '+str(len(triangles)))

for label,texture in [('Bear_Original_BaseColor','/Game/Assets/EnemyCharacters/Bear/Enemy_Bear_base'),('Crawler_Enemy_Goblin_base','/Game/Assets/EnemyCharacters/Goblin/Enemy_Goblin_base')]:
 t=u.AssetExportTask();t.object=u.load_asset(texture);t.filename=str(out/(label+'.png'));t.exporter=u.TextureExporterPNG();t.automated=True;t.prompt=False;t.replace_identical=True;assert u.Exporter.run_asset_export_task(t)
