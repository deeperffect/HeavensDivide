param([string]$Tests='HeavensDivide.Enemies.NavigationBudget+HeavensDivide.Enemies.BasicMelee+HeavensDivide.Combat.SamuraiPushback+HeavensDivide.ImpactFeedback.SwapCamera')
$projectRoot = Split-Path -Parent $PSScriptRoot
$engineExe = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$arguments = @(
    (Join-Path $projectRoot 'HeavensDivide.uproject'), '/Engine/Maps/Entry',
    '-unattended', '-nullrhi', '-nosound', '-nop4', '-nosplash',
    '-ddc=InstalledNoZenLocalFallback',
    ('-LocalDataCachePath='+$projectRoot+'\Saved\OldVillage\DDC'),
    ('-ShaderWorkingDir='+$projectRoot+'\Saved\OldVillage\Shaders'),
    ('-abslog='+$projectRoot+'\Saved\OldVillage_NavigationCameraAutomation.log'),
    ('-ExecCmds=Automation RunTests '+$Tests),
    '-TestExit=Automation Test Queue Empty',
    ('-ReportExportPath='+$projectRoot+'\Saved\OldVillage\Automation')
)
& $engineExe @arguments *> (Join-Path $projectRoot 'Saved\OldVillage_NavigationCameraAutomation_stdout.log')
exit $LASTEXITCODE
