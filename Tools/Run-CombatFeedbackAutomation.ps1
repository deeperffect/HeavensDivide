param([string]$Tests='HeavensDivide.ImpactFeedback+HeavensDivide.Combat.SamuraiBuilds+HeavensDivide.Combat.NinjaBuilds',[switch]$Render)
$projectRoot=Split-Path -Parent $PSScriptRoot
$engineExe='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
New-Item -ItemType Directory -Force -Path (Join-Path $projectRoot 'Saved\CombatFeedback') | Out-Null
$arguments=@((Join-Path $projectRoot 'HeavensDivide.uproject'),'/Engine/Maps/Entry','-unattended','-nosound','-nop4','-nosplash','-ddc=InstalledNoZenLocalFallback',('-LocalDataCachePath='+$projectRoot+'\Saved\OldVillage\DDC'),('-ShaderWorkingDir='+$projectRoot+'\Saved\OldVillage\Shaders'),('-abslog='+$projectRoot+'\Saved\CombatFeedback\Automation.log'),('-ExecCmds=Automation RunTests '+$Tests),'-TestExit=Automation Test Queue Empty',('-ReportExportPath='+$projectRoot+'\Saved\CombatFeedback\Automation'))
if($Render){$arguments+=@('-RenderOffscreen','-noraytracing')}else{$arguments+='-nullrhi'}
& $engineExe @arguments *> (Join-Path $projectRoot 'Saved\CombatFeedback\Automation_stdout.log')
exit $LASTEXITCODE
