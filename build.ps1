$ErrorActionPreference = "Stop"
Push-Location "$PSScriptRoot/01_plugins/mhw_ls_rebuild"
try {
    $env:ML_WORLD_COMBAT="1"
    $env:ML_WORLD_INPUT_VM="1"
    $env:ML_WORLD_PRESENTATION="1"
    foreach ($name in @("ML_ANIMATION_PREVIEW","ML_ATTACHMENT_PREVIEW","ML_WORLD_AUDIO_PROBE","ML_WORLD_FX_PROBE")) { Remove-Item "Env:$name" -ErrorAction SilentlyContinue }
    cargo forge build --release
    if ($LASTEXITCODE -ne 0) { throw "Build failed" }
    python ../../tools/check_relocations.py mhw_ls_rebuild.nro
    if ($LASTEXITCODE -ne 0) { throw "Relocation check failed" }
} finally { Pop-Location }
