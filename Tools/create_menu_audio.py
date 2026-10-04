"""Run in Unreal Editor Python. Generate original UI taps and randomized one-shot MetaSounds."""
import math, random, struct, wave
from pathlib import Path
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve()
BASE = '/Game/HeavensDivide/Audio/Menu'
at = u.AssetToolsHelpers.get_asset_tools()
bs = u.get_engine_subsystem(u.MetaSoundBuilderSubsystem)
ed = u.get_editor_subsystem(u.MetaSoundEditorSubsystem)

def good(result):
    assert result == u.MetaSoundBuilderResult.SUCCEEDED, str(result)

def value(result):
    good(result[-1])
    return result[0]

def save(asset):
    assert u.EditorAssetLibrary.save_loaded_asset(asset, False)

def create(name, cls, factory):
    path = BASE + '/' + name
    return u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else at.create_asset(name, BASE, cls, factory)

for name, duration, frequency, volume in [('MenuHover', .065, 920., .45), ('MenuPress', .105, 640., .65)]:
    rng = random.Random(name)
    samples = []
    for i in range(int(48000 * duration)):
        t = i / 48000
        envelope = min(1., t / .002) * math.exp(-t / (.013 if name == 'MenuHover' else .021))
        tail = min(1., (duration - t) / .01)
        # Soft woody tick with a brief brushed transient; no piercing high-frequency click.
        tone = math.sin(math.tau * frequency * t) + .22 * math.sin(math.tau * frequency * 1.51 * t)
        noise = rng.uniform(-1, 1) * .18 * math.exp(-t / .003)
        samples.append(.24 * (tone * envelope + noise) * tail)
    source = ROOT / 'SourceAudio' / 'Menu' / (name + '.wav')
    source.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(source), 'wb') as output:
        output.setparams((1, 2, 48000, 0, 'NONE', 'not compressed'))
        output.writeframes(struct.pack('<' + 'h' * len(samples), *(round(s * 32767) for s in samples)))
    if u.EditorAssetLibrary.does_asset_exist(BASE + '/MS_' + name):
        u.log('MENU_AUDIO_EXISTS ' + name)
        continue
    task = u.AssetImportTask()
    task.filename = str(source); task.destination_path = BASE; task.destination_name = 'W_' + name
    task.automated = True; task.save = True
    at.import_asset_tasks([task])
    sound_wave = u.load_asset(BASE + '/W_' + name)
    assert sound_wave
    concurrency = create('SC_' + name, u.SoundConcurrency, u.SoundConcurrencyFactory())
    settings = concurrency.get_editor_property('concurrency')
    settings.set_editor_property('max_count', 2)
    settings.set_editor_property('retrigger_time', .035 if name == 'MenuHover' else .02)
    settings.set_editor_property('resolution_rule', u.MaxConcurrentResolutionRule.STOP_OLDEST)
    settings.set_editor_property('voice_steal_release_time', .01)
    concurrency.set_editor_property('concurrency', settings); save(concurrency)
    b, play, finished, outputs, result = bs.create_source_builder('Build_' + name, u.MetaSoundOutputAudioFormat.MONO, True)
    good(result)
    def node(name, variant=''):
        c = u.MetasoundFrontendClassName()
        c.namespace = 'UE'; c.name = name; c.variant = variant
        return value(b.add_node_by_class_name(c))
    def inp(n, k): return value(b.find_node_input_by_name(n, k))
    def out(n, k): return value(b.find_node_output_by_name(n, k))
    def connect(a, z): good(b.connect_nodes(a, z))
    player = node('Wave Player', 'Mono'); pitch = node('RandomFloat')
    wave_input = value(b.add_graph_input_node('Wave', 'WaveAsset', bs.create_object_meta_sound_literal(sound_wave)))
    connect(wave_input, inp(player, 'Wave Asset'))
    for key, number in [('Min', -.35), ('Max', .35)]:
        graph_input = value(b.add_graph_input_node('Pitch' + key, 'Float', bs.create_float_meta_sound_literal(number)[0]))
        connect(graph_input, inp(pitch, key))
    good(b.set_node_input_default(inp(pitch, 'Seed'), bs.create_int_meta_sound_literal(-1)[0]))
    connect(play, inp(pitch, 'Next')); connect(out(pitch, 'On Next'), inp(player, 'Play'))
    connect(out(pitch, 'Value'), inp(player, 'Pitch Shift'))
    connect(out(player, 'Out Mono'), outputs[0]); connect(out(player, 'On Finished'), finished)
    ed.set_node_location(b, pitch, u.Vector2D(-350, -150)); ed.set_node_location(b, player, u.Vector2D(0, 0))
    value(ed.build_to_asset(b, 'HeavensDivide - original procedural UI audio', 'MS_' + name, BASE))
    sound = u.load_asset(BASE + '/MS_' + name)
    sound.set_editor_property('volume', volume)
    sound.set_editor_property('concurrency_set', {concurrency})
    save(sound)
    u.log('MENU_AUDIO_CREATED ' + sound.get_path_name())
u.log('MENU_AUDIO_READY')
