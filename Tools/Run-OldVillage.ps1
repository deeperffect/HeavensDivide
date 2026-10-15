param([string]$Script='import_old_village.py',[switch]$Render,[switch]$Audio,[switch]$Play)
$projectRoot = Split-Path -Parent $PSScriptRoot
$engineExe = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$arguments = @((Join-Path $projectRoot 'HeavensDivide.uproject'),'-unattended','-nosound','-nop4','-nosplash','-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities','-ddc=InstalledNoZenLocalFallback',('-LocalDataCachePath='+$projectRoot+'\Saved\OldVillage\DDC'),('-abslog='+$projectRoot+'\Saved\OldVillage_'+[IO.Path]::GetFileNameWithoutExtension($Script)+'.log'))
$arguments += ('-ShaderWorkingDir='+$projectRoot+'\Saved\OldVillage\Shaders')
if($Audio){$arguments = @($arguments | Where-Object {$_ -ne '-nosound'}); $arguments += '-AudioMixer'}
if($Render -or $Play){
    $arguments += @('/Game/Maps/Lvl_OldVillage',('-ExecutePythonScript='+($projectRoot -replace '\\','/')+'/Tools/'+$Script))
    if($Render){$arguments += @('-RenderOffscreen','-noraytracing')}
    else{$arguments += '-nullrhi'}
}
else{$arguments += @('-nullrhi','-run=pythonscript',('-script='+($projectRoot -replace '\\','/')+'/Tools/'+$Script))}
& $engineExe @arguments *> (Join-Path $projectRoot ('Saved\OldVillage_'+[IO.Path]::GetFileNameWithoutExtension($Script)+'_stdout.log'))
exit $LASTEXITCODE
