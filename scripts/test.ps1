$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$selfTest = Join-Path ([IO.Path]::GetTempPath()) "easykey-selftest-$PID.exe"

Push-Location $root
try {
    & uv run --no-project --with ziglang python -m ziglang cc -O2 -UNDEBUG -Wall -Wextra -Werror "tests/core_test.c" "src/core.c" -o $selfTest
    if ($LASTEXITCODE -ne 0) { throw "后端自测编译失败" }
    & $selfTest
    if ($LASTEXITCODE -ne 0) { throw "后端自测失败: $LASTEXITCODE" }

    & node tests/webui_test.js
    if ($LASTEXITCODE -ne 0) { throw "WebUI 回归测试失败" }
    & node tests/module_test.js
    if ($LASTEXITCODE -ne 0) { throw "安装与重载脚本测试失败" }

    $repoText = (Get-Content -Raw -LiteralPath "module/repo.json").Trim()
    $defaultRepoText = (Get-Content -Raw -LiteralPath "module/repo.default.json").Trim()
    $null = $repoText | ConvertFrom-Json
    $null = $defaultRepoText | ConvertFrom-Json
    if ($repoText -ne $defaultRepoText) { throw "默认命令库与用户命令库不一致" }

    $moduleProps = Get-Content -LiteralPath "module/module.prop"
    $version = ($moduleProps | Where-Object { $_ -like "version=*" } | Select-Object -First 1).Substring(8)
    if ($version -ne "v3") { throw "模块版本错误" }
    if ($moduleProps -notcontains "author=MoeShadow") { throw "模块作者错误" }

    & (Join-Path $root "package.ps1")
    $archive = Join-Path $root "dist/EasyKey-$version.zip"
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = [IO.Compression.ZipFile]::OpenRead($archive)
    try {
        $expected = @("EasyKey", "config.ini", "customize.sh", "ind/torch.sh", "module.prop", "repo.default.json", "repo.json", "service.sh", "reload.sh", "webroot/index.html")
        $actual = @($zip.Entries.FullName | Sort-Object)
        if (Compare-Object ($expected | Sort-Object) $actual) { throw "安装包内容不完整" }
    } finally {
        $zip.Dispose()
    }
    Write-Host "All tests passed"
} finally {
    Pop-Location
    if (Test-Path -LiteralPath $selfTest) { Remove-Item -LiteralPath $selfTest -Force }
}
