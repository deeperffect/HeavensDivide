"""Extend the existing elite materials for ring attacks and fading hazard tails.

Existing colors, fill, animated outlines and ordinary elite warnings are preserved.
Run through UnrealEditor-Cmd -run=pythonscript.
"""
from datetime import datetime
from pathlib import Path
import json
import shutil
import unreal as u

root = Path(u.Paths.project_dir()).resolve()
lib = u.MaterialEditingLibrary
backup = root / 'Saved/Backups/UnifiedEnemyIndicators' / datetime.now().strftime('%Y%m%d_%H%M%S')
report = {}

for name in ['M_AttackIndicatorCircle', 'M_AttackIndicatorRectangle']:
    material = u.load_asset('/Game/HeavensDivide/Materials/' + name)
    assert material, name
    backup.mkdir(parents=True, exist_ok=True)
    shutil.copy2(root / 'Content/HeavensDivide/Materials' / (name + '.uasset'), backup / (name + '.uasset'))
    expressions = lib.get_material_expressions(material)
    custom = next(e for e in expressions if isinstance(e, u.MaterialExpressionCustom))
    colors = {str(e.get_editor_property('parameter_name')): str(e.get_editor_property('default_value'))
              for e in expressions if isinstance(e, u.MaterialExpressionVectorParameter)}
    code = custom.get_editor_property('code')

    def parameter(name, default):
        inputs = list(custom.get_editor_property('inputs'))
        if any(str(i.get_editor_property('input_name')) == name for i in inputs):
            return
        expression = lib.create_material_expression(material, u.MaterialExpressionScalarParameter, -700, 1500 + len(inputs)*80)
        expression.set_editor_property('parameter_name', name)
        expression.set_editor_property('default_value', default)
        expression.set_editor_property('group', 'Encounter Shape')
        pin = u.CustomInput()
        pin.set_editor_property('input_name', name)
        inputs.append(pin)
        custom.set_editor_property('inputs', inputs)
        assert lib.connect_material_expressions(expression, '', custom, name)

    if '// Shared encounter impact and fade' not in code:
        parameter('Fade', 1.)
        parameter('Impact', 0.)
        assert 'return float4(rgb,silhouette);' in code, name
        code = code.replace('return float4(rgb,silhouette);', '''// Shared encounter impact and fade; zero impact preserves the authored warning.
rgb += Edge.rgb * rim * saturate(Impact) * (0.8 + 0.3*sin(Clock*18.0));
return float4(rgb,silhouette*saturate(Fade));''')

    if name == 'M_AttackIndicatorCircle' and '// Shared encounter safe center' not in code:
        parameter('InnerRadius', 0.)
        assert 'float travel=saturate(r/0.495);' in code, name
        code = code.replace('float travel=saturate(r/0.495);', '''// Shared encounter safe center, expressed as a fraction of the outer radius.
float inner=0.5*saturate(InnerRadius);
float travel=InnerRadius>0.0?saturate((r-inner)/max(0.001,0.495-inner)):saturate(r/0.495);''')
        assert 'float aa=' in code, name
        code = code.replace('float aa=', 'if(InnerRadius>0.0) d=max(d,inner-r);\nfloat aa=', 1)

    code = code.replace('float aa=max(max(fwidth(UV.x),fwidth(UV.y)),0.0008);', 'float aa=max(fwidth(d),0.0008);')
    if '// Bounded animation clock' not in code:
        code = code.replace('float t=Clock*1.5;', '''// Bounded animation clock also handles scene captures with invalid world time.
Clock=isfinite(Clock)?fmod(Clock,125.663706):0.0;
float t=Clock*1.5;''')
    custom.set_editor_property('code', code)
    # Match the encounter surfaces' main translucency pass. Separate translucency
    # can omit these world-space planes from scene captures and depth processing.
    material.set_editor_property('translucency_pass', u.MaterialTranslucencyPass.MTP_BEFORE_DOF)
    lib.recompile_material(material)
    assert u.EditorAssetLibrary.save_loaded_asset(material, False)
    after = {str(e.get_editor_property('parameter_name')): str(e.get_editor_property('default_value'))
             for e in lib.get_material_expressions(material) if isinstance(e, u.MaterialExpressionVectorParameter)}
    assert after == colors, 'Authored elite colors changed'
    report[name] = {'colors_preserved': colors, 'code': code}

out = root / 'Saved/UnifiedEnemyIndicators'
out.mkdir(parents=True, exist_ok=True)
(out / 'MaterialUpdate.json').write_text(json.dumps(report, indent=2))
u.log('UNIFIED_ENEMY_INDICATORS_SAVED')
