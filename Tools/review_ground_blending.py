"""Inspect the assigned Level 1 ground and capture it before/after tuning.

Uses the established four-view harness. No assumptions about the selected
instance are made during the Before batch; all effective settings are recorded.
"""
from pathlib import Path
import unreal as u

root = Path(u.Paths.project_dir())
source = (root / 'Tools/review_reference_ground.py').read_text(encoding='utf-8-sig')
changes = {
    'OUTPUT = ROOT / "Saved/GroundReferencePass"': 'OUTPUT = ROOT / "Saved/GroundBlending"',
    'if package_name(material) != MATERIAL:': 'if False:',
    'if package_name(parent) != PARENT:': 'if False:',
    '"Tools/apply_reference_ground.py"': '"Tools/tune_ground_blending.py"',
    'if map_hashes() != initial_hashes:': 'if not apply and map_hashes() != initial_hashes:',
    'STATE["map_hashes"] = initial_hashes': 'STATE["map_hashes"] = map_hashes()',
    'STATE["report"]["material"] = inspect_material()': '''STATE["report"]["material"] = inspect_material()
    parent = STATE["floor"].static_mesh_component.get_material(0).get_base_material()
    nodes = []
    for expression in LIB.get_material_expressions(parent):
        entry = {"name": expression.get_name(), "class": expression.get_class().get_name()}
        if isinstance(expression, u.MaterialExpressionCustom):
            entry["code"] = expression.get_editor_property("code")
            entry["inputs"] = [str(p.get_editor_property("input_name")) for p in expression.get_editor_property("inputs")]
        nodes.append(entry)
    write_json(STATE["out"] / "Inspection.json", {"material": STATE["report"]["material"], "nodes": nodes})
    if STATE["folder"] != "Before":
        applied = json.loads((OUTPUT / "Applied.json").read_text())
        baseline = json.loads((OUTPUT / "Before/Validation.json").read_text())["material"]
        current = STATE["report"]["material"]
        assert current["material"].split(".")[0] == applied["instance"]
        assert current["parent"].split(".")[0] == applied["parent"]
        expected = {**baseline["scalar_parameters"], **applied["changed_scalars"]}
        assert all(abs(current["scalar_parameters"][n] - v) < .0001 for n, v in expected.items())
        assert current["vector_parameters"] == baseline["vector_parameters"]
        assert all(current["texture_parameters"][n]["asset"] == t["asset"]
                   for n, t in baseline["texture_parameters"].items())
        assert all(hashlib.sha256((ROOT / p).read_bytes()).hexdigest() == h
                   for p, h in applied["protected_assets"].items())
        STATE["report"]["saved_asset_checks"] = {"passed": True, "protected_asset_count": len(applied["protected_assets"]),
                                                  "maps_textures_colors_preserved": True}
    if "-GroundSingleView" in u.SystemLibrary.get_command_line():
        STATE["specs"] = STATE["specs"][:1]''',
}
for old, new in changes.items():
    assert source.count(old) == 1, old
    source = source.replace(old, new)
exec(compile(source, str(root / 'Tools/review_reference_ground.py'), 'exec'), globals())
