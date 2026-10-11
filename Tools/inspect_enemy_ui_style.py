import unreal as u,json
from pathlib import Path
OUT=Path(u.Paths.project_saved_dir()).resolve()/'EnemyUIStyle';OUT.mkdir(parents=True,exist_ok=True)
bp=u.load_asset('/Game/HeavensDivide/Blueprints/UI/EnemyUI/WBP_EnemyHealthBar')
tree=u.find_object(None,bp.get_path_name()+':WidgetTree')
report={'widget':bp.get_path_name(),'tree':str(tree),'widgets':[],'enemies':[]}
for name in ['CanvasPanel_0','ProgressBar_0','ProgressBar','HealthBar','PB_HealthBar','SizeBox_0','Border_0']:
    obj=u.find_object(None,bp.get_path_name()+':WidgetTree.'+name)
    if obj:
        row={'name':name,'class':obj.get_class().get_name()}
        for prop in ['widget_style','fill_color_and_opacity','slot','percent','bar_fill_style','bar_fill_type','render_transform']:
            try:row[prop]=str(obj.get_editor_property(prop))
            except Exception:pass
        if isinstance(obj,u.PanelWidget):row['children']=[c.get_name() for c in obj.get_all_children()]
        report['widgets'].append(row)
export=u.AssetExportTask();export.object=bp;export.filename=str(OUT/'WBP_EnemyHealthBar.copy');export.automated=True;export.prompt=False
try:u.Exporter.run_asset_export_task(export)
except Exception as ex:u.log_warning(str(ex))
for name in ['BleedIcon','PoisonIcon']:
    tex=u.load_asset('/Game/HeavensDivide/Blueprints/UI/EnemyUI/'+name)
    task=u.AssetExportTask();task.object=tex;task.filename=str(OUT/(name+'_before.png'));task.exporter=u.TextureExporterPNG();task.automated=True;task.prompt=False
    assert u.Exporter.run_asset_export_task(task)
for p in u.EditorAssetLibrary.list_assets('/Game/HeavensDivide/Blueprints/EnemyCharacters',True,False):
    asset=u.load_asset(p)
    if not isinstance(asset,u.Blueprint):continue
    cdo=u.get_default_object(asset.generated_class())
    if not isinstance(cdo,u.EnemyBase):continue
    row={'path':p}
    for prop in ['health_bar_draw_size','status_indicator_draw_size','status_indicator_spacing','status_indicator_health_bar_gap','status_stack_font_size','health_bar_widget_class','health_bar_height_offset']:
        value=cdo.get_editor_property(prop)
        if isinstance(value,u.Vector2D):value=[value.x,value.y]
        elif isinstance(value,u.Object):value=value.get_path_name()
        row[prop]=value
    report['enemies'].append(row)
(OUT/'Before.json').write_text(json.dumps(report,indent=2));u.log('ENEMY_UI_INSPECTION '+json.dumps(report))
