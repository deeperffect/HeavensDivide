from pathlib import Path
import unreal as u
out=Path(u.Paths.project_dir())/'Saved/CombatPresentation'
out.mkdir(parents=True,exist_ok=True)
def export(name,obj):
 t=u.AssetExportTask();t.object=obj;t.exporter=u.ObjectExporterT3D();t.filename=str(out/(name+'.copy'));t.automated=True;t.prompt=False;t.replace_identical=True
 u.Exporter.run_asset_export_task(t)
a=u.load_asset('/Game/HeavensDivide/Abilities/Combos/DA_Combo_BloodMoon')
export('BloodMoon',a)
export('BloodMoonVFX',a.get_editor_property('vfx'))
for n in ('Samurai','Ninja'):
 b=u.load_asset('/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_'+n)
 c=u.get_default_object(b.generated_class())
 m=c.get_editor_property('death_montage')
 export(n+'Death',m) if m else None
 export(n,b)
 u.log(n+' death: '+str(m))
