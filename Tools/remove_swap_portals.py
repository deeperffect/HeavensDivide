"""Replace swap portal presentation with the Samurai's one-shot ink slash."""
from pathlib import Path
import shutil
import json
import unreal as u

root = Path(u.Paths.project_dir()).resolve()
backup = root / 'Saved/Backups/SwapWithoutPortals'
backup.mkdir(parents=True, exist_ok=True)
slash = u.load_asset('/Game/HeavensDivide/VFX/Swap/NS_SwapInkSlash')
assert isinstance(slash, u.NiagaraSystem)
report = []
for name in ['Samurai', 'Ninja']:
    path = '/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_' + name
    source = root / 'Content' / (path.removeprefix('/Game/') + '.uasset')
    if not (backup / source.name).exists():
        shutil.copy2(source, backup / source.name)
    bp = u.load_asset(path)
    comp = u.get_default_object(bp.generated_class()).get_editor_property('swap_presentation')
    preserved_keys = ['entrance_montage','departure_montage','arrival_impact_vfx','arrival_impact_scale',
                      'enable_ninja_arrival_drop','enable_samurai_walk_in','swap_freeze_duration','vfx_offset']
    preserved = {key: comp.get_editor_property(key) for key in preserved_keys}
    removed = {}
    for key in ['arrival_portal','departure_portal','portal_close_burst','arrival_portal_sound','departure_portal_sound']:
        old = comp.get_editor_property(key)
        removed[key] = old.get_path_name() if old else None
        comp.set_editor_property(key, None)
    if name == 'Samurai':
        comp.set_editor_property('arrival_vfx', slash)
        comp.set_editor_property('vfx_scale', 1.0)
    u.BlueprintEditorLibrary.compile_blueprint(bp)
    comp = u.get_default_object(bp.generated_class()).get_editor_property('swap_presentation')
    assert all(comp.get_editor_property(key) is None for key in removed), name
    for key, old in preserved.items():
        assert comp.get_editor_property(key) == old, (name, key)
    if name == 'Samurai':
        assert comp.get_editor_property('arrival_vfx') == slash
        assert comp.get_editor_property('vfx_scale') == 1.0
    assert u.EditorAssetLibrary.save_loaded_asset(bp, False)
    report.append({'character': name, 'removed': removed,
                   'arrival_vfx': comp.get_editor_property('arrival_vfx').get_path_name() if comp.get_editor_property('arrival_vfx') else None})
(backup / 'changes.json').write_text(json.dumps(report, indent=2))
u.log('SWAP_WITHOUT_PORTALS_PASS ' + json.dumps(report))
