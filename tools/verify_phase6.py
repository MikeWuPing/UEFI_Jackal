"""verify_phase6.py：Phase 6 串口日志断言（Task 6.5 总验收）。
对 qemu_capture.py 产生的 serial log 逐项核对：
- APP_VERSION=<run/expected_version.txt> 出现且无 ERROR 行；
- 全通链（--mode autostart）：STATE 序列含 GCS 5→7（L1 击破）后接 GCS 7→5
  （GCS7 关卡过渡 → 次关），且其后 GPM 回到 0/1（次关 intro/装载）；
- Continue 链（--mode continue）：STATE 序列含 GCS 5→6（Game Over）→ 6→8
  （有可续关 → Continue 屏）。
退出码 0=全过；失败项逐条打印。零第三方依赖。
用法：python tools/verify_phase6.py [--serial run_logs/xxx_serial.log] --mode autostart|continue
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
    ap.add_argument('--mode', choices=['autostart', 'continue'], required=True)
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

    if args.mode == 'autostart':
        check('GCS 5->7 (Level 1 boss 击破)', ('GCS=5', 'GCS=7') in gcs_pairs)
        check('GCS 7->5 (GCS7 关卡过渡 → 次关)', ('GCS=7', 'GCS=5') in gcs_pairs)
        idx5 = [i for i, p in enumerate(gcs_pairs) if p == ('GCS=7', 'GCS=5')]
        ok = False
        if idx5:
            tail = seq[gcs_pairs.index(('GCS=7', 'GCS=5')) + 1:]
            ok = any(s in ('GCS=5 GPM=0', 'GCS=5 GPM=1') for s in tail[:2])
        check('次关 intro 进入（GPM 0/1）', ok)
    else:
        check('GCS 5->6 (Game Over)', ('GCS=5', 'GCS=6') in gcs_pairs)
        check('GCS 6->8 (有可续关 → Continue 屏)', ('GCS=6', 'GCS=8') in gcs_pairs)

    print(f'state seq: {" -> ".join(seq)}')
    if fails:
        print(f'FAILED {len(fails)}')
        return 1
    print('OK all')
    return 0


if __name__ == '__main__':
    sys.exit(main())
