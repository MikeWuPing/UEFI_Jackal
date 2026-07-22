"""qemu_capture.py：QEMU 运行 + screendump 截图驱动（不依赖窗口置顶）。
原理：QEMU human monitor 走 telnet 端口，周期发 `screendump <ppm>` 命令——
dump 的是模拟显示器的显存内容，与宿主窗口遮挡无关；`-display none` 连窗口都不开。
PPM(P6) 转 PNG 复用 tools/rom_extract/render_png.py 的零依赖 write_png。
用法：python tools/qemu_capture.py [--seconds 35] [--interval 1.0] [--skip-version-check]
      [--keys "21:right:800,25:z:100,29:x:100"]（第 t 秒 sendkey key 按住 holdms）
产物：run_logs/<stamp>_serial.log、snapshot/<stamp>/NNN.png、stdout 打印版本核对结果。"""
import argparse
import socket
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools' / 'rom_extract'))
from render_png import write_png  # noqa: E402

QEMU = Path(r'C:\Program Files\qemu\qemu-system-x86_64.exe')
OVMF = Path(r'D:\Work\Code\tools\ovmf\OVMF_full.fd')
FAT_DIR = ROOT / 'run' / 'hda'
EXPECTED_VERSION = ROOT / 'run' / 'expected_version.txt'
MONITOR_PORT = 44444


def ppm_to_png(ppm_path, png_path):
    data = Path(ppm_path).read_bytes()
    # P6\n<w> <h>\n<max>\n 头（screendump 固定 255）
    parts = data.split(b'\n', 3)
    if parts[0] != b'P6':
        raise ValueError(f'not a P6 ppm: {ppm_path}')
    w, h = (int(v) for v in parts[1].split())
    rgb = parts[3]
    if len(rgb) != w * h * 3:
        raise ValueError(f'ppm payload {len(rgb)} != {w}*{h}*3')
    write_png(png_path, w, h, rgb)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--seconds', type=float, default=35.0)
    ap.add_argument('--interval', type=float, default=1.0)
    ap.add_argument('--skip-version-check', action='store_true')
    ap.add_argument('--keys', default='',
                    help='t:key:holdms 逗号分隔（第 t 秒 sendkey key 按住 holdms），'
                         '如 21:right:800,25:z:100；holdms 省略=单击')
    args = ap.parse_args()

    key_events = []
    for item in (args.keys.split(',') if args.keys else []):
        parts = item.split(':')
        key_events.append((float(parts[0]), parts[1],
                           parts[2] if len(parts) > 2 else None))
    key_events.sort()

    stamp = time.strftime('%Y%m%d_%H%M%S')
    serial = ROOT / 'run_logs' / f'{stamp}_serial.log'
    snap_dir = ROOT / 'snapshot' / stamp
    serial.parent.mkdir(parents=True, exist_ok=True)
    snap_dir.mkdir(parents=True, exist_ok=True)

    qemu_args = [
        str(QEMU), '-machine', 'q35', '-m', '256M', '-vga', 'std',
        '-display', 'none', '-nic', 'none',
        '-bios', str(OVMF),
        '-drive', f'format=raw,file=fat:rw:{FAT_DIR}',
        '-serial', f'file:{serial}',
        '-monitor', f'telnet:127.0.0.1:{MONITOR_PORT},server,nowait',
    ]
    proc = subprocess.Popen(qemu_args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        # 等 monitor 端口起来
        sock = None
        for _ in range(50):
            try:
                sock = socket.create_connection(('127.0.0.1', MONITOR_PORT), timeout=1)
                break
            except OSError:
                time.sleep(0.2)
        if sock is None:
            raise RuntimeError('monitor port not reachable')
        sock.settimeout(0.2)

        def drain():
            try:
                while sock.recv(65536):
                    pass
            except OSError:
                pass

        n = 0
        t0 = time.time()
        key_idx = 0
        while time.time() - t0 < args.seconds:
            t = time.time() - t0
            while key_idx < len(key_events) and key_events[key_idx][0] <= t:
                _, key, hold = key_events[key_idx]
                cmd = f'sendkey {key} {hold}\n' if hold else f'sendkey {key}\n'
                sock.sendall(cmd.encode('ascii'))
                key_idx += 1
            ppm = snap_dir / f'{n:03d}.ppm'
            sock.sendall(f'screendump {ppm}\n'.encode('ascii'))
            time.sleep(args.interval)
            drain()
            if ppm.exists():
                ppm_to_png(ppm, snap_dir / f'{n:03d}.png')
                ppm.unlink()
            n += 1
        sock.close()
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()

    print(f'snapshots -> {snap_dir} ({n} shots)')
    print(f'serial    -> {serial}')
    if not args.skip_version_check:
        expected = EXPECTED_VERSION.read_text(encoding='ascii').strip()
        log = serial.read_bytes().decode('ascii', errors='replace')
        ok_version = f'APP_VERSION={expected}' in log
        has_error = 'ERROR ' in log
        states = [ln.split('STATE ')[1] for ln in log.replace('\r', '').split('\n') if 'STATE ' in ln]
        # STATE 消息可能连写（无换行分隔），按 "STATE " 切片重取
        import re
        states = re.findall(r'STATE (GCS=\d+ GPM=\d+)', log)
        print(f'version match: {ok_version} ({expected})')
        print(f'error lines:   {has_error}')
        print(f'state seq:     {" -> ".join(states)}')
        if not ok_version or has_error:
            raise SystemExit(1)


if __name__ == '__main__':
    main()
