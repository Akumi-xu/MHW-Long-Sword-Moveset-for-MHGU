#Requires -Version 5.1
<#
.SYNOPSIS
    编译 MHGU 插件工程 → 实机/模拟器安全检查 → 收集到 02_build。

.DESCRIPTION
    1. 读工程的目标 JSON，判断用哪个链接器：
       - rust-lld（工程自己指定，例如链接预编译 C 库时）
       - arm-none-eabi-gcc（cargo forge 新模板，实机构建配置）
       用 gcc 时自动找到工具链并设置 CARGO_TARGET_ARMV7A_NONE_EABI_LINKER。
    2. `cargo forge build [--release]`
    3. 对产出的 .nro 跑 06_tools\check_relocations.py：只要有任何重定位写到
       只读段就直接判失败（模拟器不报错，实机会崩）。
    4. 复制到 02_build\latest，另存一份带时间戳的到 02_build\history。

.PARAMETER Project
    工程目录名（01_plugins 下）。省略时：只有一个工程就用它，多个则列出来让你指定。

.PARAMETER Dev
    编译调试版（默认 release）。

.PARAMETER NoCopy
    只编译，不复制到 02_build。

.PARAMETER NoCheck
    跳过重定位检查（不推荐，只用于临时排障）。

.PARAMETER Deploy
    编译检查通过后调用 deploy.ps1 部署。

.PARAMETER ModName
    配合 -Deploy：MOD 目录名。

.PARAMETER Target
    配合 -Deploy：eden（默认）/ switch。

.PARAMETER Loader
    配合 -Deploy：hardware（默认）/ none。

.PARAMETER EnvVars
    构建期环境变量（hashtable），编译完自动还原。用于分版本构建，
    例如 sharpness_hud 的 `-EnvVars @{ SHARPNESS_LAYOUT = 'handheld' }`。

.EXAMPLE
    .\build.ps1
    .\build.ps1 -Project my_plugin
    .\build.ps1 -Deploy -Target switch -ModName MHGU_MY_PLUGIN
#>
[CmdletBinding()]
param(
    [string]$Project = '',
    [switch]$Dev,
    [switch]$NoCopy,
    [switch]$NoCheck,
    [switch]$Deploy,
    [string]$ModName = 'MHGU_PLUGIN_DEV',
    [ValidateSet('eden', 'switch')]
    [string]$Target = 'eden',
    [ValidateSet('hardware', 'none')]
    [string]$Loader = 'hardware',
    # 构建期环境变量，例如 @{ SHARPNESS_LAYOUT = 'handheld' }
    [hashtable]$EnvVars
)

$ErrorActionPreference = 'Stop'

$root    = Split-Path -Parent $PSScriptRoot
$projDir = Join-Path $root '01_plugins'

# ---------------------------------------------------------------- 定位工程
if (-not $Project) {
    $all = @(Get-ChildItem $projDir -Directory -ErrorAction SilentlyContinue |
             Where-Object { Test-Path (Join-Path $_.FullName 'Cargo.toml') })
    if ($all.Count -eq 1) {
        $Project = $all[0].Name
    } elseif ($all.Count -eq 0) {
        Write-Host '01_plugins 下还没有插件工程。新建一个：' -ForegroundColor Yellow
        Write-Host '  cd C:\gumod\01_plugins'
        Write-Host '  cargo forge new my_plugin'
        exit 1
    } else {
        Write-Host '有多个工程，请用 -Project 指定：' -ForegroundColor Yellow
        $all | ForEach-Object { Write-Host ("  {0}" -f $_.Name) }
        exit 1
    }
}

$candidate = Join-Path $projDir $Project
if (-not (Test-Path (Join-Path $candidate 'Cargo.toml'))) {
    $hits = @(Get-ChildItem $projDir -Directory -ErrorAction SilentlyContinue |
              Where-Object { $_.Name -like "$Project*" -and (Test-Path (Join-Path $_.FullName 'Cargo.toml')) })
    if ($hits.Count -eq 1) {
        $candidate = $hits[0].FullName
    } elseif ($hits.Count -eq 0) {
        Write-Host "找不到工程: $Project （在 $projDir 下）" -ForegroundColor Red
        Get-ChildItem $projDir -Directory -ErrorAction SilentlyContinue |
            Where-Object { Test-Path (Join-Path $_.FullName 'Cargo.toml') } |
            ForEach-Object { Write-Host ("  可用工程: {0}" -f $_.Name) }
        exit 1
    } else {
        Write-Host "工程名 $Project 有多个匹配，请写完整: $($hits.Name -join ', ')" -ForegroundColor Red
        exit 1
    }
}
$candidate = (Resolve-Path $candidate).Path

$manifest = Get-Content (Join-Path $candidate 'Cargo.toml') -Raw
if ($manifest -notmatch '(?m)^\s*name\s*=\s*"([^"]+)"') {
    Write-Host 'Cargo.toml 里找不到 package.name' -ForegroundColor Red
    exit 1
}
$pkgName = $Matches[1]
$nroName = ($pkgName -replace '-', '_') + '.nro'
$profile = if ($Dev) { 'debug' } else { 'release' }

# ---------------------------------------------------------------- 链接器
$targetJson = Get-ChildItem $candidate -Filter '*.json' -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -like '*eabi*.json' -or $_.Name -like 'arm*.json' } |
    Select-Object -First 1
$linkerKind = 'lld'
if ($targetJson) {
    $tj = Get-Content $targetJson.FullName -Raw
    if ($tj -match '"linker"\s*:\s*"([^"]+)"') { $linkerName = $Matches[1] } else { $linkerName = '?' }
    if ($linkerName -match 'gcc') { $linkerKind = 'gcc' }
} else {
    $linkerName = '(未找到目标 JSON)'
}

function Find-ArmGcc {
    $cmd = Get-Command arm-none-eabi-gcc -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    $patterns = @(
        'D:\tools\arm-none-eabi-gcc\*\bin\arm-none-eabi-gcc.exe',
        'C:\devkitPro\devkitARM\bin\arm-none-eabi-gcc.exe',
        'D:\devkitPro\devkitARM\bin\arm-none-eabi-gcc.exe',
        'C:\Program Files (x86)\Arm GNU Toolchain*\bin\arm-none-eabi-gcc.exe',
        'C:\Program Files\Arm GNU Toolchain*\bin\arm-none-eabi-gcc.exe'
    )
    foreach ($p in $patterns) {
        $hit = Get-ChildItem $p -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($hit) { return $hit.FullName }
    }
    return $null
}

Write-Host ''
Write-Host '=== 编译插件 ===' -ForegroundColor Cyan
Write-Host ("工程  : {0}" -f $candidate)
Write-Host ("包名  : {0}    模式: {1}" -f $pkgName, $profile)
Write-Host ("目标  : {0}" -f ($(if ($targetJson) { $targetJson.Name } else { '默认' })))
Write-Host ("链接器: {0} ({1})" -f $linkerName, $linkerKind)

if ($linkerKind -eq 'gcc') {
    $gcc = Find-ArmGcc
    if (-not $gcc) {
        Write-Host ''
        Write-Host '这个工程用 arm-none-eabi-gcc 链接（cargo forge 的实机构建模板），但没找到它。' -ForegroundColor Red
        Write-Host '装一个再试： https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack' -ForegroundColor Yellow
        Write-Host '或设置环境变量 CARGO_TARGET_ARMV7A_NONE_EABI_LINKER 指向 arm-none-eabi-gcc.exe' -ForegroundColor Yellow
        exit 1
    }
    $env:CARGO_TARGET_ARMV7A_NONE_EABI_LINKER = $gcc
    Write-Host ("gcc   : {0}" -f $gcc)
} else {
    # 老工程用 rust-lld；顺手确认系统里没有残留的 gcc 覆盖变量
    if ($env:CARGO_TARGET_ARMV7A_NONE_EABI_LINKER) {
        Write-Host ("注意  : 环境里已有 CARGO_TARGET_ARMV7A_NONE_EABI_LINKER={0}，本次会先清掉" -f $env:CARGO_TARGET_ARMV7A_NONE_EABI_LINKER) -ForegroundColor Yellow
        Remove-Item Env:CARGO_TARGET_ARMV7A_NONE_EABI_LINKER
    }
}
Write-Host ''

# 构建期环境变量
$savedEnv = @{}
if ($EnvVars) {
    foreach ($k in $EnvVars.Keys) {
        $savedEnv[$k] = [Environment]::GetEnvironmentVariable($k, 'Process')
        Set-Item -Path ("Env:" + $k) -Value ([string]$EnvVars[$k])
        Write-Host ("环境  : {0}={1}" -f $k, $EnvVars[$k])
    }
}

Push-Location $candidate
try {
    $argList = @('forge', 'build')
    if (-not $Dev) { $argList += '--release' }

    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    & cargo @argList
    $code = $LASTEXITCODE
    $sw.Stop()
} finally {
    Pop-Location
    foreach ($k in $savedEnv.Keys) {
        if ($null -eq $savedEnv[$k]) {
            Remove-Item -Path ("Env:" + $k) -ErrorAction SilentlyContinue
        } else {
            Set-Item -Path ("Env:" + $k) -Value $savedEnv[$k]
        }
    }
}

Write-Host ''
if ($code -ne 0) {
    Write-Host ("编译失败 (exit {0})，耗时 {1:N1}s" -f $code, $sw.Elapsed.TotalSeconds) -ForegroundColor Red
    exit $code
}

$nroPath = Join-Path $candidate $nroName
if (-not (Test-Path $nroPath)) {
    Write-Host "编译返回成功但没找到 $nroName。" -ForegroundColor Red
    exit 1
}

$nro  = Get-Item $nroPath
$hash = (Get-FileHash $nroPath -Algorithm SHA256).Hash.ToLower()

Write-Host ("编译成功  {0:N1}s" -f $sw.Elapsed.TotalSeconds) -ForegroundColor Green
Write-Host ("NRO     : {0}" -f $nro.FullName)
Write-Host ("大小    : {0:N0} 字节 ({1:N1} KB)" -f $nro.Length, ($nro.Length / 1KB))
Write-Host ("SHA256  : {0}" -f $hash)

# ---------------------------------------------------------------- 实机安全检查
if (-not $NoCheck) {
    Write-Host ''
    Write-Host '=== 实机加载检查 ===' -ForegroundColor Cyan
    $checker = Join-Path $PSScriptRoot 'check_relocations.py'
    $py = Get-Command python -ErrorAction SilentlyContinue
    if (-not (Test-Path $checker)) {
        Write-Host "缺少 check_relocations.py，跳过检查" -ForegroundColor Yellow
    } elseif (-not $py) {
        Write-Host "没找到 python，跳过检查" -ForegroundColor Yellow
    } else {
        # python 在中文 Windows 下按 GBK 输出，PowerShell 5.1 又按控制台代码页解码，
        # 中文会变乱码。强制两边都走 UTF-8，读完再还原。
        $oldEnc = [Console]::OutputEncoding
        try {
            [Console]::OutputEncoding = [System.Text.Encoding]::UTF8
            $env:PYTHONIOENCODING = 'utf-8'
            $out = & python $checker $nroPath 2>&1 | Out-String
            $checkExit = $LASTEXITCODE
        } finally {
            [Console]::OutputEncoding = $oldEnc
            Remove-Item Env:PYTHONIOENCODING -ErrorAction SilentlyContinue
        }
        if ($checkExit -ne 0) {
            Write-Host $out -ForegroundColor Red
            Write-Host '检查未通过：这个 NRO 有写到只读段的重定位，模拟器可能没事，实机会崩。' -ForegroundColor Red
            Write-Host '排查方向：链接脚本把某段放错位置、或链接了非 PIC 的静态库。' -ForegroundColor Yellow
            exit 2
        }
        ($out -split "`n" | Where-Object { $_ -match '重定位|结果|格式|段' }) | ForEach-Object {
            Write-Host ("  {0}" -f $_.Trim())
        }
        Write-Host '  实机加载检查通过' -ForegroundColor Green
    }
}

# ---------------------------------------------------------------- 收集产物
if (-not $NoCopy) {
    $outDir  = Join-Path $root '02_build\latest'
    $histDir = Join-Path $root '02_build\history'
    New-Item -ItemType Directory -Force -Path $outDir, $histDir | Out-Null
    $stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
    $destLatest = Join-Path $outDir $nroName
    $destStamp  = Join-Path $histDir ("{0}_{1}_{2}.nro" -f $stamp, $pkgName, $profile)
    Copy-Item $nroPath $destLatest -Force
    Copy-Item $nroPath $destStamp  -Force
    Write-Host ''
    Write-Host ("已收集  : {0}" -f $destLatest)
    Write-Host ("历史副本: {0}" -f $destStamp)
}

if ($Deploy) {
    $deployScript = Join-Path $PSScriptRoot 'deploy.ps1'
    Write-Host ''
    & $deployScript -Target $Target -ModName $ModName -Loader $Loader -Plugin $nroPath
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

exit 0
