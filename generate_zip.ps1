[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$projectName = 'prog3_parallel_programming_v2026_2'
$sourceFiles = @(
    'include/parallel_transform_reduce.h',
    'include/parallel_copy_if.h',
    'include/parallel_inclusive_scan.h',
    'include/parallel_find_if.h'
)
$projectRoot = Split-Path -Parent $PSCommandPath
$outputPath = Join-Path $projectRoot "$projectName.zip"

foreach ($sourceFile in $sourceFiles) {
    if (-not (Test-Path -LiteralPath (Join-Path $projectRoot $sourceFile) -PathType Leaf)) {
        throw "No se encontro el archivo requerido: $sourceFile"
    }
}
if (Test-Path -LiteralPath $outputPath) { Remove-Item -LiteralPath $outputPath -Force }
Compress-Archive -LiteralPath ($sourceFiles | ForEach-Object { Join-Path $projectRoot $_ }) -DestinationPath $outputPath
Write-Host "Archivo generado: $outputPath"
