from pathlib import Path
import json
import unreal as u

out=Path(u.Paths.project_dir())/'Saved/ObjectivePresentation'
out.mkdir(parents=True,exist_ok=True)
result={}
for path in ['/Game/HeavensDivide/Environment/OldVillage/Meshes/Landmarks/SM_OV_GuardianStatue',
             '/Game/Assets/JapaneseShrine/Meshes/SM_StatueJizo02a',
             '/Game/Assets/Environment/Objectives/BloodShrine']:
    mesh=u.load_asset(path)
    box=mesh.get_bounding_box()
    result[path]={'min':str(box.min),'max':str(box.max),'materials':[str(m.material_interface) for m in mesh.static_materials]}
for name in ['Rare','Epic','Legendary']:
    path='/Game/HeavensDivide/Blueprints/UI/UpgradeUI/Materials/MI_UI_Upgrade'+name
    mat=u.load_asset(path)
    result[name]={'parent':str(mat.parent),'scalar':str(mat.scalar_parameter_values),'vector':str(mat.vector_parameter_values)}
(out/'Before.json').write_text(json.dumps(result,indent=2))
u.log('OBJECTIVE_PRESENTATION_INSPECTED')
