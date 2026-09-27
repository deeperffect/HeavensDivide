"""Preserve existing materials; add ring-only continuous ripples and triple the accent."""
from pathlib import Path
import shutil
import unreal as u

root = Path(u.Paths.project_dir()).resolve()
lib = u.MaterialEditingLibrary
for name in ['M_AttackIndicatorRectangle', 'M_AttackIndicatorRectangle_Decal',
             'M_AttackIndicatorCircle', 'M_AttackIndicatorCircle_Decal', 'M_SamuraiLaneIndicator']:
    material = u.load_asset('/Game/HeavensDivide/Materials/'+name)
    source = root/'Content/HeavensDivide/Materials'/(name+'.uasset')
    backup = root/'Saved/Backups/IndicatorRippleRefinement'/source.name
    backup.parent.mkdir(parents=True,exist_ok=True)
    if not backup.exists(): shutil.copy2(source,backup)
    expressions = lib.get_material_expressions(material)
    custom = next(e for e in expressions if isinstance(e,u.MaterialExpressionCustom))
    code = custom.get_editor_property('code')
    if '// Continuous outline mode' not in code:
        code = code.replace('clamp(WarningStrength,0.0,2.0)', 'clamp(WarningStrength,0.0,4.0)')
        insert = '''// Continuous outline mode (aura only).
impactGate=lerp(impactGate,clamp(WarningStrength,0.0,4.0),saturate(Continuous));
impactPhase=lerp(impactPhase,Clock*12.0,saturate(Continuous));
'''
        code = code.replace('float impactBeat=', insert+'float impactBeat=')
        code = code.replace('return float4', 'silhouette *= lerp(1.0,1.0-smoothstep(0.008,0.025,abs(d+0.008)),saturate(RingOnly));\nreturn float4')
        for index,(parameter,pin) in enumerate([('OutlineOnly','RingOnly'),('ContinuousRipple','Continuous')]):
            expression=lib.create_material_expression(material,u.MaterialExpressionScalarParameter,-600,1200+index*150)
            expression.set_editor_property('parameter_name',parameter)
            expression.set_editor_property('default_value',0.)
            expression.set_editor_property('group','Outline Animation')
            inputs=list(custom.get_editor_property('inputs'))
            item=u.CustomInput();item.set_editor_property('input_name',pin);inputs.append(item)
            custom.set_editor_property('inputs',inputs)
            assert lib.connect_material_expressions(expression,'',custom,pin)
        custom.set_editor_property('code',code)
        strength=next(e for e in expressions if isinstance(e,u.MaterialExpressionScalarParameter)
                      and str(e.get_editor_property('parameter_name'))=='ImpactWarningStrength')
        strength.set_editor_property('default_value',3.)
    lib.recompile_material(material)
    assert u.EditorAssetLibrary.save_loaded_asset(material,False)
u.log('INDICATOR_RIPPLE_REFINEMENT_PASS')
