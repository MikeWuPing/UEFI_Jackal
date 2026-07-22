"""iNES 解析与 bank 切分。ROM 事实见计划 Global Constraints。"""
from pathlib import Path

BANK_SIZE = 0x4000
HEADER_EXPECTED = {
    'prg_banks': 8, 'chr_banks': 0, 'mapper': 2, 'vertical_mirroring': True,
}

class RomHeaderError(Exception):
    pass

class Rom:
    def __init__(self, header, banks):
        self.header = header
        self.banks = tuple(banks)

def parse_header(raw):
    if len(raw) < 16 or raw[:4] != b'NES\x1a':
        raise RomHeaderError(f'bad magic: expected 4E 45 53 1A, got {raw[:4].hex()}')
    h = {
        'prg_banks': raw[4],
        'chr_banks': raw[5],
        'mapper': (raw[6] >> 4) | (raw[7] & 0xF0),
        'vertical_mirroring': bool(raw[6] & 0x01),
    }
    for key, want in HEADER_EXPECTED.items():
        if h[key] != want:
            raise RomHeaderError(f'{key}: expected {want}, got {h[key]}')
    return h

def load_rom(path):
    data = Path(path).read_bytes()
    header = parse_header(data[:16])
    prg = data[16:]
    want = header['prg_banks'] * BANK_SIZE
    if len(prg) != want:
        raise RomHeaderError(f'prg size: expected {want}, got {len(prg)}')
    banks = [prg[i*BANK_SIZE:(i+1)*BANK_SIZE] for i in range(header['prg_banks'])]
    return Rom(header, banks)
