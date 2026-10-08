param([switch]$Apply)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$contentRoot = [IO.Path]::GetFullPath((Join-Path $projectRoot 'Content'))
$archiveRoot = [IO.Path]::GetFullPath((Join-Path $projectRoot 'Saved/EncounterExpansion/UnusedMaps'))
$inspection = Get-Content -LiteralPath (Join-Path $projectRoot 'Saved/EncounterExpansion/Inspection.json') -Raw | ConvertFrom-Json
$setup = Get-Content -LiteralPath (Join-Path $projectRoot 'Saved/EncounterExpansion/Setup.json') -Raw | ConvertFrom-Json
$keep = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach ($package in $setup.kept_maps) { [void]$keep.Add($package) }
do {
    $changed = $false
    foreach ($map in $inspection.maps.PSObject.Properties) {
        foreach ($referencer in $map.Value.references) {
            $required = $keep.Contains($referencer)
            foreach ($keptPackage in @($keep)) {
                $relativePackage = $keptPackage.Substring(6)
                if ($referencer.StartsWith('/Game/__ExternalActors__/' + $relativePackage + '/') -or
                    $referencer.StartsWith('/Game/__ExternalObjects__/' + $relativePackage + '/')) { $required = $true }
            }
            if ($required -and $keep.Add($map.Name)) { $changed = $true }
        }
    }
} while ($changed)
$plan = @()
foreach ($map in $inspection.maps.PSObject.Properties) {
    if ($keep.Contains($map.Name)) { continue }
    $relativeMap = $map.Name.Substring(6)
    $candidates = @(($relativeMap + '.umap'), ($relativeMap + '_BuiltData.uasset'),
        ('__ExternalActors__/' + $relativeMap), ('__ExternalObjects__/' + $relativeMap))
    foreach ($relative in $candidates) {
        $source = [IO.Path]::GetFullPath((Join-Path $contentRoot $relative))
        $destination = [IO.Path]::GetFullPath((Join-Path $archiveRoot $relative))
        if (-not $source.StartsWith($contentRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -or
            -not $destination.StartsWith($archiveRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Archive path escaped the verified project directories.' }
        if (Test-Path -LiteralPath $source) {
            $resolvedSource = (Resolve-Path -LiteralPath $source).Path
            if (-not $resolvedSource.StartsWith($contentRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Resolved source escaped Content.' }
            $plan += [pscustomobject]@{package=$map.Name;source=$resolvedSource;archive=$destination;references=$map.Value.references}
        }
    }
}
if ($Apply) {
    New-Item -ItemType Directory -Path $archiveRoot -Force | Out-Null
    $manifest = Join-Path $archiveRoot 'Manifest.json'
    if (Test-Path -LiteralPath $manifest) { throw 'Existing manifest: inspect it before repeating this archive.' }
    [pscustomobject]@{retained=@($keep);archived=$plan} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $manifest -Encoding UTF8
    foreach ($entry in $plan) {
        if (Test-Path -LiteralPath $entry.archive) { throw "Archive target already exists: $($entry.archive)" }
        New-Item -ItemType Directory -Path (Split-Path $entry.archive -Parent) -Force | Out-Null
        Move-Item -LiteralPath $entry.source -Destination $entry.archive
    }
}
[pscustomobject]@{apply=$Apply.IsPresent;map_count=@($plan | Where-Object {$_.source.EndsWith('.umap')}).Count;entry_count=$plan.Count;retained=@($keep)} | ConvertTo-Json -Depth 3
