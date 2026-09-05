$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot

Push-Location $root
try {
    & uv run --no-project --with ziglang python -m ziglang cc -target aarch64-linux-musl -O2 -flto -fdata-sections -ffunction-sections -fno-asynchronous-unwind-tables -static -s "-Wl,--gc-sections" "-Wl,--build-id=none" "-Wl,-z,norelro" -Wall -Wextra -Werror "src/EasyKey.c" "src/core.c" -o "module/EasyKey"
    if ($LASTEXITCODE -ne 0) { throw "EasyKey 编译失败" }
} finally {
    Pop-Location
}
