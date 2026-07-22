param(
  [int]$IntervalMs = 1000,
  [int]$MaxShots   = 40,
  [switch]$SkipBuild
)
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'JackalConfig.ps1')
if (!$SkipBuild) { & (Join-Path $PSScriptRoot 'Build-Jackal.ps1') | Out-Null }

$FatDir   = Join-Path $Root 'run\hda'
$Expected = Join-Path $Root 'run\expected_version.txt'
foreach ($p in @($JackalQemuExe, $JackalOvmfFd, $FatDir, $Expected)) {
  if (!(Test-Path $p)) { throw "path not found: $p" }
}
if (!(Get-ChildItem -Path $FatDir -Filter *.efi -Recurse | Select-Object -First 1)) {
  throw "no .efi found in $FatDir"
}

$logDir = Join-Path $Root 'run_logs'
$snap   = Join-Path $Root 'snapshot'
New-Item -ItemType Directory -Force -Path $logDir, $snap | Out-Null
$stamp  = Get-Date -Format 'yyyyMMdd_HHmmss'
$serial = Join-Path $logDir ($stamp + '_serial.log')

# -nic none：去掉默认网卡，避免 OVMF 先尝试 PXE 网络启动（超时约 45s+），
# 保证在 MaxShots 截图窗口内已进入 Shell 并跑完 startup.nsh。
$qemuArgs = "-machine q35 -m 256M -vga std -display sdl -nic none " +
  "-bios `"$JackalOvmfFd`" " +
  "-drive format=raw,file=fat:rw:`"$FatDir`" " +
  "-serial file:`"$serial`""
$proc = Start-Process -FilePath $JackalQemuExe -ArgumentList $qemuArgs -PassThru
Start-Sleep -Seconds 3
& (Join-Path $PSScriptRoot 'Watch-WindowSnapshots.ps1') -ProcessName 'qemu-system-x86_64' -SnapshotDir $snap -IntervalMs $IntervalMs -MaxShots $MaxShots
if (!$proc.HasExited) {
  $proc.CloseMainWindow() | Out-Null
  $proc.WaitForExit(5000)
  if (!$proc.HasExited) { $proc.Kill() }
}
& (Join-Path $PSScriptRoot 'Test-AppVersion.ps1') -ExpectedVersionFile $Expected -SerialLog $serial -SnapshotDir $snap
