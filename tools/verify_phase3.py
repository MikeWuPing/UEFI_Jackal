"""verify_phase3.py：Phase 3 串口日志断言（Task 3.9）。
对 qemu_capture.py 产生的 serial log 逐项核对：
- APP_VERSION=<run/expected_version.txt> 出现且无 ERROR 行；
- STATE 序列：GCS 1→3→4→5、GPM 0→1→2 且经 4..10 到 3（AUTO_START 全序列）；
- SPAWN 序列：AUTO_START+AUTOSCROLL 滚入 Level 1 screen 1 时
  Block1（$860C）Y=0 两项 ID=01 依序生成 → SPAWN id=01 出现 ≥2 次。
--mode autostart：AUTO_START（或 AUTO_START+AUTOSCROLL）验证构建。
退出码 0=全过；失败项逐条打印。零第三方依赖。
用法：python tools/verify_phase3.py [--serial run_logs/xxx_serial.log] --mode autostart
"""
import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXPECTED_VERSION = ROOT / 'run' / 'expected_version.txt'


def latest_serial():
    logs = sorted((ROOT / 'run_logs').glob('*_serial.log'),
                  key=lambda p: p.stat().st_mtime)
    if not logs:
        raise SystemExit('no serial log under run_logs/')
    return logs[-1]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--serial', type=Path, default=None)
    ap.add_argument('--mode', choices=['autostart'], required=True)
    args = ap.parse_args()

    serial = args.serial or latest_serial()
    log = serial.read_bytes().decode('ascii', errors='replace')
    expected = EXPECTED_VERSION.read_text(encoding='ascii').strip()
    fails = []

    def check(name, cond):
        print(f'{"ok  " if cond else "FAIL"} {name}')
        if not cond:
            fails.append(name)

    print(f'serial: {serial.name}  mode: {args.mode}')
    check('APP_VERSION matches expected_version.txt', f'APP_VERSION={expected}' in log)
    check('no ERROR lines', 'ERROR ' not in log)

    seq = re.findall(r'STATE (GCS=\d+ GPM=\d+)', log)
    trans = [(a, b) for a, b in zip(seq, seq[1:])]
    gcs_pairs = [(a.split(' ')[0], b.split(' ')[0]) for a, b in trans]
    check('STATE trace present (first is GCS=1)', len(seq) >= 1 and seq[0] == 'GCS=1 GPM=0')
    check('GCS 1->3->4->5 (AUTO_START into game)',
          ('GCS=1', 'GCS=3') in gcs_pairs and ('GCS=3', 'GCS=4') in gcs_pairs
          and ('GCS=4', 'GCS=5') in gcs_pairs)
    gpm_seq = [s.split(' ')[1] for s in seq if s.startswith('GCS=5')]
    check('GPM 0->1->2 (intro + first-screen load)',
          gpm_seq[:3] == ['GPM=0', 'GPM=1', 'GPM=2'])
    check('GPM reaches 3 via 4..10 (Chinook) and stays final',
          'GPM=10' in gpm_seq and gpm_seq[-1] == 'GPM=3'
          and gpm_seq.index('GPM=10') < len(gpm_seq) - 1)

    spawns = re.findall(r'SPAWN id=([0-9A-F]{2})', log)
    check('SPAWN id=01 twice (Level 1 Block1 Y=0 pair)', spawns.count('01') >= 2)

    print(f'state seq: {" -> ".join(seq)}')
    print(f'spawn seq: {" ".join(spawns[:12])}')
    if fails:
        print(f'FAILED {len(fails)}')
        return 1
    print('OK all')
    return 0


if __name__ == '__main__':
    sys.exit(main())
