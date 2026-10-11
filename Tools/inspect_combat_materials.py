import unreal as u
from pathlib import Path
out=Path(u.Paths.project_saved_dir())/'CombatPolish'
for name in ['M_EncounterSurface','M_EncounterEnergy']:
    m=u.load_asset('/Game/HeavensDivide/Materials/'+name)
    t=u.AssetExportTask();t.object=m;t.exporter=u.ObjectExporterT3D();t.filename=str(out/(name+'.copy'));t.automated=True;t.prompt=False;t.replace_identical=True
    u.Exporter.run_asset_export_task(t)
    u.log(name+' '+str(m.get_editor_property('translucency_pass')))
u.EditorLoadingAndSavingUtils.load_map('/Game/Maps/Lvl_B1_Lvl1')
for a in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors():
    if isinstance(a,u.EncounterDirector):
        u.log('LEVEL_MAGNET '+a.get_actor_label()+' '+str(a.get_editor_property('magnet_urn_class')))
