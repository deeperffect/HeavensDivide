"""Run in Unreal Python to import the fanfare and build its one-shot MetaSound."""
import json
from pathlib import Path
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve()
BASE = '/Game/HeavensDivide/Audio'
bs = u.get_engine_subsystem(u.MetaSoundBuilderSubsystem)
ed = u.get_editor_subsystem(u.MetaSoundEditorSubsystem)


def good(result):
    assert result == u.MetaSoundBuilderResult.SUCCEEDED, str(result)


def value(result):
    good(result[-1])
    return result[0]


task = u.AssetImportTask()
task.filename = str(ROOT / 'SourceAudio/Combat/ChestJackpot.wav')
task.destination_path = BASE + '/Waves'
task.destination_name = 'W_ChestJackpot'
task.automated, task.save, task.replace_existing = True, True, True
u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
wave = u.load_asset(BASE + '/Waves/W_ChestJackpot')
assert wave
path = BASE + '/MetaSounds/MS_ChestJackpot'
source = u.load_asset(path)
if not source:
    b, onplay, finished, outputs, result = bs.create_source_builder('Build_ChestJackpot', u.MetaSoundOutputAudioFormat.MONO, True)
    good(result)

    def node(name, variant=''):
        cls = u.MetasoundFrontendClassName()
        cls.set_editor_property('namespace', 'UE')
        cls.set_editor_property('name', name)
        cls.set_editor_property('variant', variant)
        return value(b.add_node_by_class_name(cls))

    def inp(n, name): return value(b.find_node_input_by_name(n, name))
    def out(n, name): return value(b.find_node_output_by_name(n, name))
    def connect(a, z): good(b.connect_nodes(a, z))

    player, random = node('Wave Player', 'Mono'), node('RandomFloat')
    connect(value(b.add_graph_input_node('Wave', 'WaveAsset', bs.create_object_meta_sound_literal(wave))), inp(player, 'Wave Asset'))
    for name, amount in (('Min', -.15), ('Max', .15)):
        good(b.set_node_input_default(inp(random, name), bs.create_float_meta_sound_literal(amount)[0]))
    good(b.set_node_input_default(inp(random, 'Seed'), bs.create_int_meta_sound_literal(-1)[0]))
    connect(onplay, inp(random, 'Next'))
    connect(out(random, 'On Next'), inp(player, 'Play'))
    connect(out(random, 'Value'), inp(player, 'Pitch Shift'))
    connect(out(player, 'Out Mono'), outputs[0])
    connect(out(player, 'On Finished'), finished)
    source = value(ed.build_to_asset(b, 'HeavensDivide - original jackpot fanfare', 'MS_ChestJackpot', BASE + '/MetaSounds'))
    source = u.load_asset(path)
assert isinstance(source, u.MetaSoundSource)
source.set_editor_property('volume', .9)
source.set_editor_property('concurrency_set', {u.load_asset(BASE + '/Mix/SC_UI')})
assert u.EditorAssetLibrary.save_loaded_asset(source, False)
report_dir = ROOT / 'Saved/ChestJackpot'
report_dir.mkdir(parents=True, exist_ok=True)
(report_dir / 'AudioImport.json').write_text(json.dumps(dict(source=source.get_path_name(), wave=wave.get_path_name()), indent=2))
u.log('CHEST_JACKPOT_AUDIO_READY')
