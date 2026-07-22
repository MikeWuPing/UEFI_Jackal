"""verify_phase7.py：Phase 7 串口日志断言（Task 7.5 总验收）。
对 qemu_capture.py 产生的 serial log 逐项核对：
- APP_VERSION=<run/expected_version.txt> 出现且无 ERROR 行；
- STATE 序列含 GCS=9（结局机入场），且其后出现 GCS 9→5 迁移
  （结局机 State11 → subTransitionFromEndOfGameToLevel1 回环）。
--mode force-ending：JACKAL_DEBUG_FORCE_ENDING 验证构建（直驱结局机）。
退出码 0=全过；失败项逐条打印。零第三方依赖。
用法：python tools/verify_phase7.py [--serial run_logs/xxx_serial.log] --mode force-ending
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
    ap.add_argument('--mode', choices=['force-ending'], required=True)
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
    gcs_pairs = [(a.split(' ')[0], b.split(' ')[0]) for a, b in zip(seq, seq[1:])]
    check('STATE trace present (first is GCS=1)', len(seq) >= 1 and seq[0] == 'GCS=1 GPM=0')
    check('GCS=9 present (结局机入场)', any(s.startswith('GCS=9') for s in seq))
    check('GCS 9->5 (结局回环 Level 1)', ('GCS=9', 'GCS=5') in gcs_pairs)

    print(f'state seq: {" -> ".join(seq)}')
    if fails:
        print(f'FAILED {len(fails)}')
        return 1
    print('OK all')
    return 0


if __name__ == '__main__':
    sys.exit(main())
