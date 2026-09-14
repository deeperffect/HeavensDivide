"""Author one-shot swap Niagara systems from UE's lightweight burst template.

Run in the Unreal editor Python environment. Existing generated assets are
preserved; use new names for variants. Blueprint backups precede assignment.
"""
from pathlib import Path
import shutil
import uuid
import unreal as u

ROOT = '/Game/HeavensDivide/VFX/Swap'
TEMPLATE = '/Niagara/DefaultAssets/Templates/Systems/DirectionalBurstLightweight'
ML = u.MaterialEditingLibrary


def setp(obj, **values):
    for name, value in values.items():
        obj.set_editor_property(name, value)
    return obj


def text_property(obj, field, text):
    assert u.SwapVFXSetupLibrary.set_property_text(obj, field, text), (field, text)


def distribution(obj, field, values, mode='NON_UNIFORM_CONSTANT'):
    mode_name = ''.join(word.title() for word in mode.split('_'))
    text_property(obj, field, '(Mode=%s,ChannelConstantsAndRanges=(%s))' %
                  (mode_name, ','.join(str(v) for v in values)))


def range_value(obj, field, lo, hi=None):
    hi = lo if hi is None else hi
    text_property(obj, field, '(Mode=%s,Min=%s,Max=%s,ChannelConstantsAndRanges=(%s))' %
                  ('UniformConstant' if lo == hi else 'UniformRange', lo, hi,
                   str(lo) if lo == hi else '%s,%s' % (lo,hi)))


def curve(obj, field, channels):
    curves = []
    for points in channels:
        keys = ','.join('(Time=%s,Value=%s,InterpMode=RCIM_Linear)' % point for point in points)
        curves.append('(Keys=(%s))' % keys)
    text_property(obj, field, '(Mode=NonUniformCurve,ChannelCurves=(%s))' % ','.join(curves))


def material(smoke):
    name = 'M_SwapSmoke' if smoke else 'M_SwapInkSlash'
    path = ROOT + '/' + name
    if u.EditorAssetLibrary.does_asset_exist(path):
        return u.load_asset(path)
    m = u.AssetToolsHelpers.get_asset_tools().create_asset(name, ROOT, u.Material, u.MaterialFactoryNew())
    setp(m, blend_mode=u.BlendMode.BLEND_TRANSLUCENT, shading_model=u.MaterialShadingModel.MSM_UNLIT, two_sided=True)
    ML.set_material_usage(m, u.MaterialUsage.MATUSAGE_NIAGARA_SPRITES)
    def node(cls, **values):
        return setp(ML.create_material_expression(m, cls), **values)
    def link(a, output, b, input_name):
        assert ML.connect_material_expressions(a, output, b, input_name)
    tex = node(u.MaterialExpressionTextureSampleParameter2D, parameter_name='Shape',
               texture=u.load_asset('/Game/Assets/VFX/MixedVFX/Textures/' + ('T_Smoke_01' if smoke else 'T_Slash_01')))
    gain = node(u.MaterialExpressionMultiply, const_b=4.0 if smoke else 1.0)
    link(tex, 'R' if smoke else 'A', gain, 'A')
    mask = node(u.MaterialExpressionSaturate)
    link(gain, '', mask, '')
    particle = node(u.MaterialExpressionParticleColor)
    opacity = node(u.MaterialExpressionMultiply)
    link(mask, '', opacity, 'A'); link(particle, 'A', opacity, 'B')
    ML.connect_material_property(opacity, '', u.MaterialProperty.MP_OPACITY)
    # Dense centers stay almost black; thinner edges carry the character color.
    ink = node(u.MaterialExpressionVectorParameter, parameter_name='InkColor', default_value=u.LinearColor(.008,.004,.015,1))
    edge = node(u.MaterialExpressionScalarParameter, parameter_name='EdgeBrightness', default_value=.65 if smoke else 2.5)
    colored_edge = node(u.MaterialExpressionMultiply)
    link(particle, 'RGB', colored_edge, 'A'); link(edge, '', colored_edge, 'B')
    color = node(u.MaterialExpressionLinearInterpolate)
    link(colored_edge, '', color, 'A'); link(ink, '', color, 'B'); link(mask, '', color, 'Alpha')
    ML.connect_material_property(color, '', u.MaterialProperty.MP_EMISSIVE_COLOR)
    ML.layout_material_expressions(m)
    ML.recompile_material(m)
    assert u.EditorAssetLibrary.save_loaded_asset(m, False)
    return m


def system(smoke):
    name = 'NS_SwapVioletSmoke' if smoke else 'NS_SwapInkSlash'
    path = ROOT + '/' + name
    if u.EditorAssetLibrary.does_asset_exist(path):
        return u.load_asset(path)
    mat = material(smoke)
    s = u.AssetToolsHelpers.get_asset_tools().duplicate_asset(name, ROOT, u.load_asset(TEMPLATE))
    assert s
    emitters = u.SwapVFXSetupLibrary.get_emitters(s)
    assert len(emitters) == 1
    e = emitters[0]
    text_property(e, 'EmitterState', '(LoopBehavior=Once,InactiveResponse=Complete,LoopDurationMode=Fixed,LoopDuration=(Mode=UniformConstant,Min=0.08,Max=0.08,ChannelConstantsAndRanges=(0.08)))')
    text_property(e, 'SpawnInfos', '((Type=Burst,SpawnTime=0.0,SourceId=%s,Amount=(Min=%d,Max=%d)))' %
                  (uuid.uuid4().hex.upper(), 14 if smoke else 2, 14 if smoke else 2))
    modules = {m.get_class().get_name().replace('NiagaraStatelessModule_', ''): m for m in e.get_editor_property('Modules')}
    enabled = {'InitializeParticle','ShapeLocation','AddVelocity','SolveVelocitiesAndForces',
               'ScaleColor','ScaleSpriteSize','SpriteRotationRate','ApplyOwnerScaleToAttributes'}
    for name, m in modules.items():
        m.set_editor_property('bModuleEnabled', name in enabled)
    init = modules['InitializeParticle']
    range_value(init, 'LifetimeDistribution', .38 if smoke else .18, .58 if smoke else .25)
    range_value(init, 'SpriteRotationDistribution', -180 if smoke else -18, 180 if smoke else 18)
    distribution(init, 'SpriteSizeDistribution', [95,95,145,145] if smoke else [300,95,340,110], 'NON_UNIFORM_RANGE')
    distribution(init, 'ColorDistribution', [.45,.12,1,.8] if smoke else [1,.06,.025,1])
    distribution(init, 'InitialPositionDistribution', [0,0,65])
    shape = modules['ShapeLocation']
    assert u.SwapVFXSetupLibrary.set_property_text(shape, 'ShapePrimitive', 'Box')
    distribution(shape, 'BoxSize', [60,60,40] if smoke else [10,10,15])
    velocity = modules['AddVelocity']
    assert u.SwapVFXSetupLibrary.set_property_text(velocity, 'VelocityType', 'Linear')
    distribution(velocity, 'LinearVelocityDistribution', [-100,-100,45,100,100,110] if smoke else [-15,-15,10,15,15,20], 'NON_UNIFORM_RANGE')
    range_value(velocity, 'LinearVelocityScale', 1.0)
    range_value(modules['SpriteRotationRate'], 'RotationRateDistribution', -70 if smoke else 100, 70 if smoke else 140)
    curve(modules['ScaleSpriteSize'], 'ScaleDistribution',
          [[(0,.55),(.3,1.15),(1,1.7)]]*2 if smoke else [[(0,.5),(.22,1),(1,1.2)],[(0,.25),(.15,1),(1,.4)]])
    curve(modules['ScaleColor'], 'ScaleDistribution', [[(0,1),(1,1)]]*3 +
          [[(0,0),(.08,1),(.35,.8),(1,0)] if smoke else [(0,0),(.06,1),(.5,.85),(1,0)]])
    text_property(modules['ApplyOwnerScaleToAttributes'], 'SystemScaleData',
                  '(bScaleInitialSpriteSize=True,bScaleInitialVelocity=True)')
    for renderer in e.get_editor_property('RendererProperties'):
        renderer.set_editor_property('Material', mat)
        renderer.set_editor_property('SubImageSize', u.Vector2D(1,1))
        text_property(renderer, 'Alignment', 'Unaligned')
        text_property(renderer, 'FacingMode', 'FaceCamera')
    text_property(e, 'FixedBounds', '(Min=(X=-400,Y=-400,Z=-150),Max=(X=400,Y=400,Z=400),IsValid=1)')
    assert u.SwapVFXSetupLibrary.rebuild_system(s, u.LinearColor(.45,.12,1,1) if smoke else u.LinearColor(1,.12,.08,1)), path
    assert u.EditorAssetLibrary.save_loaded_asset(s, False)
    return s


def assign(character, effect):
    path = '/Game/HeavensDivide/Blueprints/PlayerCharacters/BP_' + character
    project = Path(u.Paths.project_dir())
    source = project / ('Content/' + path.removeprefix('/Game/') + '.uasset')
    backup = project / 'Saved/Backups/SwapVFX' / source.name
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists():
        shutil.copy2(source, backup)
    bp = u.load_asset(path)
    component = u.get_default_object(bp.generated_class()).get_editor_property('swap_presentation')
    # Preserve any custom effects assigned by the user.
    if not component.get_editor_property('arrival_vfx'):
        component.set_editor_property('arrival_vfx', effect)
        u.BlueprintEditorLibrary.compile_blueprint(bp)
        assert u.EditorAssetLibrary.save_loaded_asset(bp, False)
    u.log('SWAP_VFX_ASSIGNED ' + character + ' ' + str(component.get_editor_property('arrival_vfx')))


if __name__ == '__main__':
    smoke = system(True)
    slash = system(False)
    assign('Ninja', smoke)
    assign('Samurai', slash)
    u.log('SWAP_VFX_CREATED_OK')
