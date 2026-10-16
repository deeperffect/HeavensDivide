"""Remove only the unreferenced local texture from the rejected mip experiment."""
import json,unreal as u
path='/Game/HeavensDivide/Environment/SceneryRepair/Textures/T_Leaves_BC_Coverage'
E=u.EditorAssetLibrary
if E.does_asset_exist(path):
 refs=E.find_package_referencers_for_asset(path,True)
 assert not refs, list(refs)
 assert E.delete_asset(path)
(ROOT/'Saved/MapSceneryAudit/TextureTrialCleanup.json').write_text(json.dumps({'deleted_unreferenced_trial':path}))
