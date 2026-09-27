from pathlib import Path
import unreal as u
out=Path(u.Paths.project_saved_dir(),'DoubleCutInspection').resolve()
out.mkdir(parents=True,exist_ok=True)
def export(obj,name):
 t=u.AssetExportTask();t.object=obj;t.exporter=u.ObjectExporterT3D();t.filename=str(out/(name+'.copy'));t.automated=True;t.prompt=False;t.replace_identical=True
 assert u.Exporter.run_asset_export_task(t)
montage=u.load_asset('/Game/HeavensDivide/Blueprints/PlayerCharacters/Montages/Samurai/AM_DoubleCutSamurai')
export(montage,'montage')
for event in u.AnimationLibrary.get_animation_notify_events(montage):
 n=event.get_editor_property('notify')
 if not n: continue
 for field in ['niagara_system','template']:
  try: fx=n.get_editor_property(field)
  except Exception: continue
  if fx: export(fx,fx.get_name())
u.log('DOUBLE_CUT_INSPECTION_PASS')
