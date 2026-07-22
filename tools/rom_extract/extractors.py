"""NES 数据格式解码器（纯函数）。metasprite 格式证据：Bank1.ASM:426-428 表头注释
"mimics the OADDMA structure: (Y,Table ID,Attributes,X)" + subDrawObjectSprites
（byte0→OAM_Y、byte3→OAM_X）：{yOffset(signed), tileIndex, attributes, xOffset(signed)}
四字节/精灵，前置数量字节。attr bit6/bit7 的 H/V 翻转锚点补偿（ASM 对 x 取负减 8、
对 y 取负减 16）在渲染时进行，不在提取时。"""

def decode_tile(tile16):
    if len(tile16) != 16:
        raise ValueError('tile must be 16 bytes')
    rows = []
    for r in range(8):
        lo, hi = tile16[r], tile16[8 + r]
        rows.append([(((hi >> (7 - c)) & 1) << 1) | ((lo >> (7 - c)) & 1) for c in range(8)])
    return rows

def decode_tiles(data):
    if len(data) % 16:
        raise ValueError(f'tile data length {len(data)} not multiple of 16')
    return [decode_tile(data[i:i+16]) for i in range(0, len(data), 16)]

def _s8(b):
    return b - 256 if b >= 128 else b

def parse_metasprite(data):
    count = data[0]
    if len(data) < 1 + 4 * count:
        raise ValueError(f'metasprite truncated: count {count}, have {len(data)}')
    sprites = []
    for i in range(count):
        o = 1 + 4 * i
        sprites.append({'y': _s8(data[o]), 'tile': data[o+1], 'attr': data[o+2], 'x': _s8(data[o+3])})
    return {'count': count, 'sprites': sprites}

def parse_palette(data):
    return list(data)

def parse_metatile32(data):
    if len(data) != 16:
        raise ValueError('32x32 metatile must be 16 tile indexes')
    return list(data)

def flip_h(tile):
    return [list(reversed(row)) for row in tile]

def flip_v(tile):
    return list(reversed(tile))

def decode_nametable_rle(data):
    """Label925（Bank7.ASM:2383-2452）nametable RLE 流解码。
    流 = 若干段 {dw ppuAddr, RLE…}：段内 bit7=1 字面 (b&0x7F) 字节、bit7=0 重复下一字节
    (b&0x7F) 次（计数 0=256）；$7F=本段结束读下一段，$FF=全流结束。
    返回 [(addr, bytes), ...]；调用方应已按表长度切片（长度由 walk_nametable_rle 测定）。"""
    segs = []
    off = 0
    while True:
        addr = data[off] | (data[off + 1] << 8)
        off += 2
        out = bytearray()
        while True:
            b = data[off]
            off += 1
            if b == 0xFF:
                segs.append((addr, bytes(out)))
                return segs
            if b == 0x7F:
                segs.append((addr, bytes(out)))
                break
            if b & 0x80:
                n = b & 0x7F
                out += data[off:off + n]
                off += n
            else:
                n = b if b else 256
                out += data[off:off + 1] * n
                off += 1

def walk_nametable_rle(bank_bytes, off):
    """从 off 起按 Label925 语义扫描 nametable 流，返回总长度（含末 $FF）。
    用于测定 REGISTRY 条目长度（数据在 ASM 中被省略，只能按终止标记走）。"""
    start = off
    while True:
        off += 2  # dw ppuAddr
        while True:
            b = bank_bytes[off]
            off += 1
            if b == 0xFF:
                return off - start
            if b == 0x7F:
                break
            if b & 0x80:
                off += b & 0x7F
            else:
                off += 1

def walk_ppu_stream(bank_bytes, off):
    """Label152 文本/图形更新流（{addrHi, addrLo, data…, $FE|$FF}）长度扫描，含终止符。"""
    start = off
    off += 2
    while bank_bytes[off] not in (0xFE, 0xFF):
        off += 1
    return off + 1 - start
