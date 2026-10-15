"""Apply final visual-only polish, then refresh previews and editor validation."""
from pathlib import Path
import unreal as u
root=Path(u.Paths.project_dir()).resolve()
exec(compile((root/'Tools/finish_survival_rework.py').read_text(encoding='utf-8'),'finish_survival_rework.py','exec'))
REPORT_NAME='FinalArtValidation.json'
SKIP_AUTOMATION=True
PREVIEW_ONLY=True
exec(compile((root/'Tools/review_survival_maps.py').read_text(encoding='utf-8'),'review_survival_maps.py','exec'))
