from pathlib import Path
import shutil
import unreal as u
root=Path(u.Paths.project_dir()).resolve()
source_file=Path('C:/Users/deepe/OneDrive/Documents/HeavensDivide/Artwork/MainMenu.mp4')
target=root/'Content/Movies/MainMenu.mp4'
target.parent.mkdir(parents=True,exist_ok=True)
assert not target.exists() or target.read_bytes()==source_file.read_bytes(), 'Existing project video differs; preserve it'
if not target.exists(): shutil.copy2(source_file,target)
base='/Game/HeavensDivide/Blueprints/UI/MainMenu/'
backup=root/'Saved/Backups/MenuMedia'
backup.mkdir(parents=True,exist_ok=True)
for name in ['main_menu','MP_MainMenu']:
 asset_file=root/'Content/HeavensDivide/Blueprints/UI/MainMenu'/(name+'.uasset')
 if not (backup/asset_file.name).exists(): shutil.copy2(asset_file,backup/asset_file.name)
source=u.load_asset(base+'main_menu')
source.set_file_path(str(target))
assert source.validate()
player=u.load_asset(base+'MP_MainMenu')
player.set_looping(True)
assert player.get_editor_property('loop')
assert u.EditorAssetLibrary.save_loaded_asset(source,False)
assert u.EditorAssetLibrary.save_loaded_asset(player,False)
u.log('MENU_MEDIA_FIXED path='+str(source.get_editor_property('file_path'))+' valid='+str(source.validate())+' looping='+str(player.get_editor_property('loop')))
