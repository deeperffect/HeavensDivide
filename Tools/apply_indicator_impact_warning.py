"""Add a late-charge outline accent without rebuilding existing material graphs."""
from pathlib import Path
import shutil
import unreal as u

root = Path(u.Paths.project_dir()).resolve()
lib = u.MaterialEditingLibrary
names = ['M_AttackIndicatorRectangle', 'M_AttackIndicatorRectangle_Decal',
         'M_AttackIndicatorCircle', 'M_AttackIndicatorCircle_Decal', 'M_SamuraiLaneIndicator']
for name in names:
    path = '/Game/HeavensDivide/Materials/' + name
    material = u.load_asset(path)
    assert material, path
    source = root / 'Content/HeavensDivide/Materials' / (name + '.uasset')
    backup = root / 'Saved/Backups/IndicatorImpactWarning' / source.name
    backup.parent.mkdir(parents=True, exist_ok=True)
    if not backup.exists():
        shutil.copy2(source, backup)
    expressions = lib.get_material_expressions(material)
    custom = next(e for e in expressions if isinstance(e, u.MaterialExpressionCustom))
    code = custom.get_editor_property('code')
    if '// Impact warning accent' in code:
        for parameter, pin in [('ImpactWarningStart', 'WarningStart'), ('ImpactWarningStrength', 'WarningStrength')]:
            expression = next(e for e in expressions if isinstance(e, u.MaterialExpressionScalarParameter)
                              and str(e.get_editor_property('parameter_name')) == parameter)
            assert lib.connect_material_expressions(expression, '', custom, pin)
        lib.recompile_material(material)
        assert u.EditorAssetLibrary.save_loaded_asset(material, False)
        continue
    # A full static aura has FillAmount=1, so it never enters this charge-only window.
    # Deform inward, retaining the existing outer footprint and avoiding UV clipping.
    accent = r'''
// Impact warning accent: synchronized to charge progress, not global time.
float impactQ=saturate((saturate(Fill)-clamp(WarningStart,0.0,0.98))/max(0.02,1-clamp(WarningStart,0.0,0.98)));
float impactGate=smoothstep(0.0,0.35,impactQ)*(1-step(0.9999,Fill))*clamp(WarningStrength,0.0,2.0);
float impactPhase=6.2831853*(impactQ+2*impactQ*impactQ);
float impactBeat=0.5+0.5*sin(impactPhase);
float impactRipple=0.5+0.5*sin(IMPACT_COORD*55.0-impactPhase*2.0);
d+=impactGate*(0.0008+0.0022*impactRipple)*(0.5+0.5*impactBeat);
'''.replace('IMPACT_COORD', 'angle' if 'Circle' in name else '(p.x+p.y)')
    index = code.index('float aa')
    code = code[:index] + accent + code[index:]
    index = code.index('return float4')
    edge = 'border' if name == 'M_SamuraiLaneIndicator' else 'Edge.rgb'
    code = code[:index] + f'rgb += {edge} * rim * impactGate * (0.4 + impactBeat * 1.8);\n' + code[index:]
    inputs = list(custom.get_editor_property('inputs'))
    for i, (parameter, pin, value) in enumerate([
        ('ImpactWarningStart', 'WarningStart', .8),
        ('ImpactWarningStrength', 'WarningStrength', 1.)]):
        expression = lib.create_material_expression(material, u.MaterialExpressionScalarParameter, -600, 850+i*150)
        expression.set_editor_property('parameter_name', parameter)
        expression.set_editor_property('default_value', value)
        expression.set_editor_property('group', 'Impact Warning')
        item = u.CustomInput()
        item.set_editor_property('input_name', pin)
        inputs.append(item)
        custom.set_editor_property('inputs', inputs)
        assert lib.connect_material_expressions(expression, '', custom, pin)
        inputs = list(custom.get_editor_property('inputs'))
    custom.set_editor_property('code', code)
    lib.recompile_material(material)
    assert u.EditorAssetLibrary.save_loaded_asset(material, False)
    u.log('IMPACT_WARNING_UPDATED ' + name)
u.log('IMPACT_WARNING_SETUP_PASS')
