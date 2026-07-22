param(
  [string]$Target = 'DEBUG',
  # 验证构建开关（非原版行为，仅自动验收用）：传入 C 宏名列表，如
  #   -VerifyDefine JACKAL_DEBUG_AUTO_START
  #   -VerifyDefine JACKAL_DEBUG_AUTO_START,JACKAL_DEBUG_AUTOSCROLL
  # 非空时生成 run\JackalPkgVerify.dsc（PLATFORM_NAME/OUTPUT_DIRECTORY 改
  # JackalPkgVerify，追加 [BuildOptions] /D 宏）并以其构建，产物与正常构建隔离。
  [string[]]$VerifyDefine = @()
)
$ErrorActionPreference = 'Stop'
$Root       = Split-Path -Parent $PSScriptRoot
$RunDir     = Join-Path $Root 'run'
$VersionFile = Join-Path $Root 'VERSION.txt'
$VersionH   = Join-Path $Root 'JackalPkg\Application\JackalUefi\Version.h'

# ---- 1. 版本递增 ----
$lines   = Get-Content $VersionFile
$version = (($lines | Where-Object { $_ -match '^VERSION=' }) -replace '^VERSION=', '').Trim()
$build   = [int]((($lines | Where-Object { $_ -match '^BUILD=' }) -replace '^BUILD=', '').Trim()) + 1
if ($version -notmatch '^\d+\.\d+\.\d+$') { throw "bad VERSION in $VersionFile" }
$parts = $version.Split('.')
$ts = Get-Date -Format 'yyyyMMddHHmmss'
$versionString = "$version+$build.$ts"
Set-Content -Path $VersionFile -Value ("VERSION=$version`nBUILD=$build")

$header = @"
#ifndef APP_VERSION_H
#define APP_VERSION_H
#define APP_VERSION_MAJOR $($parts[0])
#define APP_VERSION_MINOR $($parts[1])
#define APP_VERSION_PATCH $($parts[2])
#define APP_BUILD_NUMBER  $build
#define APP_BUILD_TIMESTAMP L"$ts"
#define APP_VERSION_STRING  L"$versionString"
#define APP_VERSION_ASCII   "$versionString"
#endif
"@
Set-Content -Path $VersionH -Value $header

# ---- 2. EDK2 build（显式 X64/VS2019，不依赖 Conf/target.txt）----
$env:WORKSPACE      = 'D:\Work\Code'
$env:EDK2_DIR       = "$env:WORKSPACE\edk2"
$env:PACKAGES_PATH  = "$Root;$env:EDK2_DIR"
$env:EDK_TOOLS_PATH = "$env:EDK2_DIR\BaseTools"

# 验证构建：生成独立 DSC（平台/输出目录加 Verify 后缀，追加 /D 宏），与正常构建隔离
$DscPath = 'JackalPkg\JackalPkg.dsc'
$BuildOutName = 'JackalPkg'
if ($VerifyDefine.Count -gt 0) {
  # -File 调用时 "A,B" 会作为单个字面字符串绑定，这里统一按逗号再拆一次
  $VerifyDefine = $VerifyDefine | ForEach-Object { $_ -split ',' } | Where-Object { $_.Trim() } | ForEach-Object { $_.Trim() }
  $verifyDsc = Join-Path $RunDir 'JackalPkgVerify.dsc'
  New-Item -ItemType Directory -Force -Path $RunDir | Out-Null
  $flags = ($VerifyDefine | ForEach-Object { "/D $_" }) -join ' '
  $lines = Get-Content (Join-Path $Root 'JackalPkg\JackalPkg.dsc')
  $lines = $lines -replace '(?m)^(  PLATFORM_NAME\s*=\s*)JackalPkg$', '$1JackalPkgVerify'
  $lines = $lines -replace '(?m)^(  OUTPUT_DIRECTORY\s*=\s*)Build/JackalPkg$', '$1Build/JackalPkgVerify'
  $lines += ''
  $lines += '[BuildOptions]'
  $lines += "  MSFT:*_*_*_CC_FLAGS = $flags"
  Set-Content -Path $verifyDsc -Value $lines -Encoding ASCII
  $DscPath = 'run\JackalPkgVerify.dsc'
  $BuildOutName = 'JackalPkgVerify'
}

# 注意：EDK2 build 的模块级跳变检测不追踪未列入 INF [Sources] 的头文件，
# 仅重写 Version.h 不会触发 JackalUefi 重编译，会导致 efi 内版本串过旧。
# 这里先删除本模块的构建目录强制完整 AutoGen + 重编译（只删 OUTPUT 会丢失
# AutoGen 生成的 cc_resp_*.txt 应答文件导致 cl D8022 报错），
# 保证 APP_VERSION 与 expected_version.txt 同步。
$ModuleOut = "$env:WORKSPACE\Build\$BuildOutName\${Target}_VS2019\X64\JackalPkg\Application\JackalUefi\JackalUefi"
if (Test-Path $ModuleOut) { Remove-Item $ModuleOut -Recurse -Force }
cmd /c "`"$env:EDK2_DIR\edksetup.bat`" >NUL && build -p $DscPath -a X64 -t VS2019 -b $Target"
if ($LASTEXITCODE -ne 0) { throw "edk2 build failed (exit=$LASTEXITCODE); expected_version.txt NOT updated" }

$Efi = "$env:WORKSPACE\Build\$BuildOutName\${Target}_VS2019\X64\JackalUefi.efi"
if (!(Test-Path $Efi)) { throw "efi missing after build: $Efi" }

# ---- 3. 只有 build 成功才更新运行目录 ----
# 媒体布局：OVMF 进内置 UEFI Shell → 执行 \startup.nsh → 加载 \JackalUefi.efi
New-Item -ItemType Directory -Force -Path "$RunDir\hda" | Out-Null
if (Test-Path "$RunDir\hda\EFI") { Remove-Item -Recurse -Force "$RunDir\hda\EFI" }
Copy-Item $Efi "$RunDir\hda\JackalUefi.efi" -Force
$nsh = "@echo off`r`necho Starting JackalUefi...`r`nJackalUefi.efi`r`n"
[System.IO.File]::WriteAllText("$RunDir\hda\startup.nsh", $nsh, [System.Text.ASCIIEncoding]::new())
Set-Content -Path (Join-Path $RunDir 'expected_version.txt') -Value $versionString
$versionString
