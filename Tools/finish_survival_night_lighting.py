"""Apply explicit RGB light colors; FColor's positional field order is B,G,R,A."""
from pathlib import Path
from datetime import datetime
import shutil,unreal as u
root=Path(u.Paths.project_dir()).resolve()
backup=root/'Saved/SurvivalMaps/Backups'/datetime.now().strftime('%Y%m%d_%H%M%S')
backup.mkdir(parents=True,exist_ok=True)
disk=root/'Content/Maps/Lvl_MoonlitForest.umap';shutil.copy2(disk,backup/disk.name)
level=u.get_editor_subsystem(u.LevelEditorSubsystem);assert level.load_level('/Game/Maps/Lvl_MoonlitForest')
for a in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors():
 if isinstance(a,u.DirectionalLight):
  c=a.get_component_by_class(u.DirectionalLightComponent);c.set_editor_property('light_color',u.Color(r=125,g=165,b=255,a=255))
 if isinstance(a,u.SkyLight):
  c=a.get_component_by_class(u.SkyLightComponent);c.set_editor_property('light_color',u.Color(r=130,g=165,b=230,a=255))
 if isinstance(a,u.PointLight):
  c=a.get_component_by_class(u.PointLightComponent);c.set_editor_property('light_color',u.Color(r=85,g=165,b=255,a=255))
assert level.save_current_level()
u.log('SURVIVAL_NIGHT_LIGHTING_SAVED')
