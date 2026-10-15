"""Correct provisional archive names after visual model inspection."""
from pathlib import Path
import json, runpy
ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'Art/Environment/OldVillage'
old=json.loads((ART/'SourceManifest.json').read_text())
# Read the new table without running the extractor.
source=(ROOT/'Tools/prepare_old_village_sources.py').read_text()
ns={};exec(source[source.index('SPECS = '):source.index('\nmanifest = ')],ns)
new={row[0]:row[1:] for row in ns['SPECS']}
archive=ROOT/'Saved/OldVillage/InitialNaming';archive.mkdir(parents=True,exist_ok=True)
for a in old['assets']:
    category,name,*_=new[a['zip']]
    if name==a['name'] and category==a['category']:continue
    # Move the whole provisional prepared folder out of Art. Destination is
    # verified within this project's Saved directory before every move.
    src=(ART/a['category']/a['name']).resolve();dst=(archive/a['name']).resolve()
    assert src.is_relative_to(ART.resolve()) and dst.is_relative_to((ROOT/'Saved').resolve())
    if src.exists():
        assert not dst.exists(),dst
        src.rename(dst)
    src=(ART/'_Original'/a['name']).resolve();dst=(ART/'_Original'/name).resolve()
    assert src.is_relative_to(ART.resolve()) and dst.is_relative_to(ART.resolve())
    if src.exists():
        assert not dst.exists(),dst
        src.rename(dst)
runpy.run_path(str(ROOT/'Tools/prepare_old_village_sources.py'),run_name='__main__')
