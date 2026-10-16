param([string]$Tests='HeavensDivide.RunFixes',[switch]$Render)
$projectRoot=Split-Path -Parent $PSScriptRoot
$engineExe='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$arguments=@((Join-Path $projectRoot 'HeavensDivide.uproject'),'/Engine/Maps/Entry','-unattended','-nosound','-nop4','-nosplash','-ddc=InstalledNoZenLocalFallback',('-LocalDataCachePath='+$projectRoot+'\Saved\OldVillage\DDC'),('-ShaderWorkingDir='+$projectRoot+'\Saved\OldVillage\Shaders'),('-abslog='+$projectRoot+'\Saved\RunFixes\Automation.log'),('-ExecCmds=Automation RunTests '+$Tests),'-TestExit=Automation Test Queue Empty',('-ReportExportPath='+$projectRoot+'\Saved\RunFixes\Automation'))
if($Render){$arguments+=@('-RenderOffscreen','-noraytracing')}else{$arguments+='-nullrhi'}
& $engineExe @arguments *> (Join-Path $projectRoot 'Saved\RunFixes\Automation_stdout.log')
exit $LASTEXITCODE
