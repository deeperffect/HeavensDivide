"""Use a smooth unlit altitude wash on far terrain to remove grid-like shading."""
from pathlib import Path
import unreal as u
root=Path(u.Paths.project_dir()).resolve();ns={'LIBRARY_ONLY':True}
exec(compile((root/'Tools/build_survival_maps.py').read_text(encoding='utf-8'),'build_survival_maps.py','exec'),ns)
for key,cfg in ns['MAPS'].items():
 ns['name']=key;ns['cfg']=cfg
 ns['custom_material']('DistantLand','float h=saturate(P.z/2400);return '+ns['color'](cfg['ground'])+'*lerp(.52,.72,h);',unlit=True)
u.log('SURVIVAL_DISTANT_LAND_SIMPLIFIED')
