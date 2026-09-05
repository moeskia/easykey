$ErrorActionPreference = "Stop"

$moduleDir = Join-Path $PSScriptRoot "module"
$outputDir = Join-Path $PSScriptRoot "dist"

& (Join-Path $PSScriptRoot "scripts/build.ps1")

$version = (Get-Content -LiteralPath (Join-Path $moduleDir "module.prop") | Where-Object { $_ -like "version=*" } | Select-Object -First 1).Substring(8)
$output = Join-Path $outputDir "EasyKey-$version.zip"
$files = @("EasyKey", "config.ini", "customize.sh", "module.prop", "repo.default.json", "repo.json", "service.sh", "reload.sh", "ind", "webroot")

New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
Push-Location $moduleDir
try {
    Compress-Archive -LiteralPath $files -DestinationPath $output -CompressionLevel Optimal -Force
} finally {
    Pop-Location
}
Write-Host $output
