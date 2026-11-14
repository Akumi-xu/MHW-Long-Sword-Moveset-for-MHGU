#Requires -Version 5.1
<#
.SYNOPSIS
    构建斩味条的三个版本（底座 / 掌机 / 可调整），并打包成只含插件本体的 MOD。

.DESCRIPTION
    版面坐标不同，靠构建期环境变量选择：

        SHARPNESS_LAYOUT=handheld   -> 掌机 720p 版
        不设 / 其他                  -> 底座 1080p 版
        SHARPNESS_ADJ=1             -> 可调整版（自带手柄菜单）

    产物：
        02_build\插件本体\sharpness_hud_docked.nro
        02_build\插件本体\sharpness_hud_handheld.nro
        02_build\插件本体\sharpness_hud_adj.nro

        02_build\成品MOD\斩味条HUD\斩味条-*.zip（只含插件本体）

    加载器（forge 前置）是单独一个包：02_build\成品MOD\forge前置\forge前置.zip，
    由 06_tools\patches\pack_forge.py 生成。三个斩味条版本只能装一个。

.EXAMPLE
    .\make_release.ps1
#>
[CmdletBinding()]
param(
    [string]$Root
)

$ErrorActionPreference = 'Stop'

if (-not $Root) { $Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot) }

$proj    = 'sharpness_hud'
$projDir = Join-Path $Root "01_plugins\$proj"
$variDir = Join-Path $Root '02_build\插件本体'
$buildPs = Join-Path $Root '06_tools\build.ps1'

if (-not (Test-Path $buildPs)) { throw "找不到 $buildPs（用 -Root 指定工作区根目录）" }
New-Item -ItemType Directory -Force -Path $variDir | Out-Null

# 变体：名字 / 输出文件 / 构建环境变量
$variants = @(
    [pscustomobject]@{
        Key = 'docked'; Nro = 'sharpness_hud_docked.nro'
        Env = @{ SHARPNESS_LAYOUT = 'docked' }
        Desc = '底座/主机 1080p，固定位置'
    },
    [pscustomobject]@{
        Key = 'handheld'; Nro = 'sharpness_hud_handheld.nro'
        Env = @{ SHARPNESS_LAYOUT = 'handheld' }
        Desc = '掌机 720p，固定位置'
    },
    [pscustomobject]@{
        Key = 'adjust'; Nro = 'sharpness_hud_adj.nro'
        Env = @{ SHARPNESS_LAYOUT = 'docked'; SHARPNESS_ADJ = '1' }
        Desc = '可调整版，L+十字键下+Y 开菜单'
    }
)

$results = @()

foreach ($v in $variants) {
    Write-Host ''
    Write-Host ("========== 构建 {0}（{1}）==========" -f $v.Key, $v.Desc) -ForegroundColor Cyan

    & $buildPs -Project $proj -NoCopy -EnvVars $v.Env
    if ($LASTEXITCODE -ne 0) { throw "$($v.Key) 版构建失败" }

    $built = Join-Path $projDir "$proj.nro"
    if (-not (Test-Path $built)) { throw "没找到 $built" }

    $dest = Join-Path $variDir $v.Nro
    Copy-Item $built $dest -Force
    $results += [pscustomobject]@{
        Variant = $v.Key
        Path    = $dest
        Size    = (Get-Item $dest).Length
        Sha256  = (Get-FileHash $dest -Algorithm SHA256).Hash.ToLower()
    }
}

Write-Host ''
Write-Host '========== 打包（只含插件本体）==========' -ForegroundColor Cyan

# 打包逻辑统一在 Python 里，避免和 06_tools/package.ps1 的"带加载器"模式混淆
$packPy = Join-Path $Root '06_tools\patches\pack_sharpness.py'
& 'python' $packPy
if ($LASTEXITCODE -ne 0) { throw '斩味条打包失败' }

Write-Host ''
Write-Host '========== 汇总 ==========' -ForegroundColor Cyan
$results |
    Select-Object Variant, @{n = 'KB'; e = { '{0:N1}' -f ($_.Size / 1KB) } }, @{n = 'SHA256(前16)'; e = { $_.Sha256.Substring(0, 16) } } |
    Format-Table -AutoSize | Out-String | Write-Host

Write-Host ''
Write-Host '========== 成品（02_build\成品MOD）==========' -ForegroundColor Cyan
$finDir = Join-Path $Root '02_build\成品MOD'
Get-ChildItem $finDir -Recurse -Filter *.zip -ErrorAction SilentlyContinue | Sort-Object Directory, Name | ForEach-Object {
    Write-Host ("  {0,-14} {1,-34} {2,6:N0} KB" -f $_.Directory.Name, $_.Name, ($_.Length / 1KB)) -ForegroundColor Green
}

Write-Host ''
Write-Host '装哪个：' -ForegroundColor Yellow
Write-Host '  先装一次 forge 前置：02_build\成品MOD\forge前置\forge前置.zip' -ForegroundColor Yellow
Write-Host '  主机/底座  -> 斩味条-主机模式.zip' -ForegroundColor Yellow
Write-Host '  掌机       -> 斩味条-掌机模式.zip' -ForegroundColor Yellow
Write-Host '  现场微调   -> 斩味条L + 十字键下 + Y可调整版.zip（纯手柄）' -ForegroundColor Yellow
Write-Host '三个斩味条只能装一个。模拟器放 load 目录，实机合并进 atmosphere\contents\title id。' -ForegroundColor Yellow
exit 0
