"""Configure and verify a single stable ponytail simulation before saving."""
import shutil
from pathlib import Path
import unreal as u

root = Path(u.Paths.project_dir())
out = root / 'Saved/CharacterSimulation'
out.mkdir(parents=True, exist_ok=True)
backup = root / 'Saved/Backups/CharacterSimulation20260928'
backup.mkdir(parents=True, exist_ok=True)
source = root / 'Content/HeavensDivide/Blueprints/PlayerCharacters/ABP_Ninja.uasset'
if not (backup / source.name).exists():
    shutil.copy2(source, backup / source.name)

bp = u.load_asset('/Game/HeavensDivide/Blueprints/PlayerCharacters/ABP_Ninja')
early = u.load_object(None, bp.get_path_name() + ':AnimGraph.AnimGraphNode_AnimDynamics_1')
final = u.load_object(None, bp.get_path_name() + ':AnimGraph.AnimGraphNode_AnimDynamics_2')
assert early and final
patch = u.SwapVFXSetupLibrary.set_property_text
# Both flags stay off even if the exposed Alpha pin is subsequently changed.
assert patch(early, 'Node', '(Alpha=0,bDoUpdate=False,bDoEval=False)')
assert patch(early, 'NodeComment', 'Disabled duplicate ponytail simulation. Final output node owns hair physics.')

bodies = []
for index, swing in enumerate([12., 18., 23., 28., 32.]):
    # Retain the original inertia and joint centers. Slender bodies proved unstable.
    bodies.append('(BoundBone=(BoneName="Ponytail%d"),BoxExtents=(X=10,Y=10,Z=10),LocalJointOffset=(X=0,Y=0,Z=0),ConstraintSetup=(LinearAxesMin=(X=0,Y=0,Z=0),LinearAxesMax=(X=0,Y=0,Z=0),AngularLimitsMin=(X=-%f,Y=-%f,Z=-18),AngularLimitsMax=(X=%f,Y=%f,Z=18)))' % (index+1, swing, swing, swing, swing))

# Damping must stay in [0,1]: AnimDynamics uses pow(1-damping, dt).
settings = '(BoundBone=(BoneName="Ponytail1"),ChainEnd=(BoneName="Ponytail5"),SimulationSpace=Component,bDoUpdate=True,bDoEval=True,bChain=True,GravityScale=0.5,bOverrideLinearDamping=True,LinearDampingOverride=0.85,bOverrideAngularDamping=True,AngularDampingOverride=0.9,bAngularSpring=False,AngularSpringConstant=0,NumSolverIterationsPreUpdate=8,NumSolverIterationsPostUpdate=2,ComponentLinearAccScale=(X=0,Y=0,Z=0),ComponentLinearVelScale=(X=0,Y=0,Z=0),SimSpaceSettings=(SimSpaceAngularAlpha=0),PhysicsBodyDefinitions=(%s))' % ','.join(bodies)
assert patch(final, 'Node', settings)
assert patch(final, 'NodeComment', 'Single ponytail simulation: component space, locked linear constraints, damped motion. Duplicate cached-pose simulation is disabled.')
u.BlueprintEditorLibrary.compile_blueprint(bp)
assert u.CharacterSimulationSetupCommandlet.verify_simulation('Ninja')
assert u.EditorAssetLibrary.save_loaded_asset(bp, False)
(out / 'validated_hair_settings.txt').write_text(settings)
task = u.AssetExportTask()
task.object = bp
task.filename = str(out / 'ABP_Ninja_configured.copy')
task.automated = True
task.prompt = False
task.replace_identical = True
assert u.Exporter.run_asset_export_task(task)
u.log('NINJA_HAIR_SETUP_PASS')
