param([string]$Tests='HeavensDivide.Collection+HeavensDivide.Encounters.BossCharge+HeavensDivide.Encounters.BossSpellPressure+HeavensDivide.Combat.SamuraiBuilds+HeavensDivide.Meta+HeavensDivide.Progression.RunPreparation+HeavensDivide.UI.RunPreparationAndMinimap+HeavensDivide.UI.LevelUpPresentation+HeavensDivide.UI.UpgradeCardLayout',[switch]$Render)
$projectRoot=Split-Path -Parent $PSScriptRoot
$engineExe='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$arguments=@((Join-Path $projectRoot 'HeavensDivide.uproject'),'/Engine/Maps/Entry','-unattended','-nosound','-nop4','-nosplash','-MetaSkillScreenshot','-ddc=InstalledNoZenLocalFallback',('-LocalDataCachePath='+$projectRoot+'\Saved\OldVillage\DDC'),('-ShaderWorkingDir='+$projectRoot+'\Saved\OldVillage\Shaders'),('-abslog='+$projectRoot+'\Saved\CollectionCombat\Automation.log'),('-ExecCmds=Automation RunTests '+$Tests),'-TestExit=Automation Test Queue Empty',('-ReportExportPath='+$projectRoot+'\Saved\CollectionCombat\Automation'))
if($Render){$arguments+=@('-RenderOffscreen','-noraytracing')}else{$arguments+='-nullrhi'}
& $engineExe @arguments *> (Join-Path $projectRoot 'Saved\CollectionCombat\Automation_stdout.log')
exit $LASTEXITCODE
