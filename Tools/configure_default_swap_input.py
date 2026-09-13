"""Persist right mouse button as the default swap mapping, preserving controllers."""
from pathlib import Path
import shutil
import unreal
root=Path(unreal.Paths.project_dir()).resolve()
path='/Game/Inputs/IMC_Player'
context=unreal.load_asset(path)
swap=unreal.load_asset('/Game/Inputs/IA_Swap')
assert context and swap
def get_mappings():
    return context.get_editor_property('default_key_mappings').get_editor_property('mappings')
def key(name):
    result=unreal.Key()
    result.set_editor_property('key_name',name)
    return result
validate='-ValidateSwapInput' in unreal.SystemLibrary.get_command_line()
if not validate:
    source=root/'Content/Inputs/IMC_Player.uasset'
    backup=root/'Saved/Backups/Keybinds/IMC_Player.uasset'
    if not backup.exists():backup.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,backup)
    context.unmap_key(swap,key('Q'))
    if not any(m.action==swap and str(m.key.get_editor_property('key_name'))=='RightMouseButton' for m in get_mappings()):
        context.map_key(swap,key('RightMouseButton'))
    assert unreal.EditorAssetLibrary.save_loaded_asset(context,False)
mappings=get_mappings()
assert any(m.action==swap and str(m.key.get_editor_property('key_name'))=='RightMouseButton' for m in mappings)
assert not any(m.action==swap and str(m.key.get_editor_property('key_name'))=='Q' for m in mappings)
for m in mappings:
    if m.action:unreal.log('INPUT_MAPPING: '+m.action.get_name()+' '+str(m.key.get_editor_property('key_name')))
unreal.log('DEFAULT_SWAP_INPUT_OK')
