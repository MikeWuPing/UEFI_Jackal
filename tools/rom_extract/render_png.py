"""零依赖 PNG 编码 + 抽样渲染。NES_PALETTE 为 NesDev 2C02 标准 64 色。
attr 语义（NES OAM）：bit6=H 翻转，bit7=V 翻转；ASM 锚点补偿 H 翻转 x=-x-8、V 翻转 y=-y-16。"""
import struct, zlib
from pathlib import Path
from extractors import decode_tile, flip_h, flip_v

NES_PALETTE = [
    ( 84, 84, 84),(  0, 30,116),(  8, 16,144),( 48,  0,136),( 68,  0,100),( 92,  0, 48),( 84,  4,  0),( 60, 24,  0),
    ( 32, 42,  0),(  8, 58,  0),(  0, 64,  0),(  0, 60,  0),(  0, 50, 60),(  0,  0,  0),(  0,  0,  0),(  0,  0,  0),
    (152,150,152),(  8, 76,196),( 48, 50,236),( 92, 30,228),(136, 20,176),(160, 20,100),(152, 34, 32),(120, 60,  0),
    ( 84, 90,  0),( 40,114,  0),(  8,124,  0),(  0,118, 40),(  0,102,120),(  0,  0,  0),(  0,  0,  0),(  0,  0,  0),
    (236,238,236),( 76,154,236),(120,124,236),(176, 98,236),(228, 84,236),(236, 88,180),(236,106,100),(212,136, 32),
    (160,170,  0),(116,196,  0),( 76,208, 32),( 56,204,108),( 56,180,204),(  0,  0,  0),(  0,  0,  0),(  0,  0,  0),
    (236,238,236),(168,204,236),(188,188,236),(212,178,236),(236,174,236),(236,174,212),(236,180,176),(228,196,144),
    (204,210,120),(180,222,120),(168,226,144),(152,226,180),(160,214,228),(  0,  0,  0),(  0,  0,  0),(  0,  0,  0),
]

def write_png(path, w, h, rgb):
    def chunk(tag, payload):
        c = tag + payload
        return struct.pack('>I', len(payload)) + c + struct.pack('>I', zlib.crc32(c) & 0xFFFFFFFF)
    if len(rgb) != w * h * 3:
        raise ValueError('rgb length mismatch')
    raw = b''.join(b'\x00' + rgb[y*w*3:(y+1)*w*3] for y in range(h))
    png = (b'\x89PNG\r\n\x1a\n'
           + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
           + chunk(b'IDAT', zlib.compress(raw, 9))
           + chunk(b'IEND', b''))
    Path(path).write_bytes(png)

def _canvas(w, h, color=(0, 0, 0)):
    return bytearray(bytes(color) * (w * h))

def _put_px(buf, w, h, x, y, rgb):
    if 0 <= x < w and 0 <= y < h:
        buf[(y * w + x) * 3:(y * w + x) * 3 + 3] = bytes(rgb)

def render_chr_page(chr_ram, palette, out_path, base=0, tiles_x=16, tiles_y=16):
    w, h = tiles_x * 8, tiles_y * 8
    buf = _canvas(w, h)
    for i in range(tiles_x * tiles_y):
        t = decode_tile(chr_ram[(base + i) * 16:(base + i) * 16 + 16])
        ox, oy = (i % tiles_x) * 8, (i // tiles_x) * 8
        for y in range(8):
            for x in range(8):
                _put_px(buf, w, h, ox + x, oy + y, NES_PALETTE[palette[t[y][x]] & 0x3F])
    write_png(out_path, w, h, bytes(buf))

def render_metasprite(chr_ram, ms, out_path, palette=(0x21, 0x11, 0x01, 0x31), size=48, sprite_base=0x1000):
    buf = _canvas(size, size, (40, 40, 40))
    cx, cy = size // 2, size // 2
    for s in ms['sprites']:
        # OAM tile index 相对 sprite pattern 页（Jackal 用 $1000 页）；
        # attr bit6/bit7 是精灵级 OAM 翻转：只镜像 tile 图案，锚点不变
        # （-x-8/-y-16 补偿是对象级 $10 翻转的语义，Bank1.ASM:253-261，与本表 attr 无关）
        t = decode_tile(chr_ram[sprite_base + s['tile'] * 16:sprite_base + s['tile'] * 16 + 16])
        sx, sy = s['x'], s['y']
        if s['attr'] & 0x40:
            t = flip_h(t)
        if s['attr'] & 0x80:
            t = flip_v(t)
        for y in range(8):
            for x in range(8):
                c = t[y][x]
                if c:
                    _put_px(buf, size, size, cx + sx + x, cy + sy + y,
                            NES_PALETTE[palette[c] & 0x3F])
    write_png(out_path, size, size, bytes(buf))

def render_nametable(chr_ram, nametable, palette32, out_path, width=256, height=240):
    """通用 nametable 渲染：0x400 字节（32x30 tile + $3C0 属性表）+ 32 字节调色板 RAM。
    attribute 字节 4 个 16x16 子块各 2-bit 组号（bit0-1=TL,2-3=TR,4-5=BL,6-7=BR）。"""
    if len(nametable) != 0x400:
        raise ValueError('nametable must be 0x400 bytes')
    buf = _canvas(width, height)
    for ty in range(30):
        for tx in range(32):
            t = decode_tile(chr_ram[nametable[ty*32+tx] * 16:nametable[ty*32+tx] * 16 + 16])
            attr = nametable[0x3C0 + (ty // 4) * 8 + (tx // 4)]
            g = (attr >> (((tx // 2) % 2) + ((ty // 2) % 2) * 2) * 2) & 3
            for y in range(8):
                for x in range(8):
                    col = t[y][x]
                    pi = palette32[g * 4 + col] & 0x3F
                    _put_px(buf, width, height, tx*8 + x, ty*8 + y, NES_PALETTE[pi])
    write_png(out_path, width, height, bytes(buf))

def render_title_screen(rom, registry, out_path):
    """标题画面：scene 6 pattern + tblMainTitleScreenNametable（Label925 解码进 $2000）
    + tblTitleScreenPalette（$3F $00 头 + 32 字节调色板 + $FE 尾）。"""
    from tables import by_name, entry_bytes
    from pattern_loader import load_scene
    from extractors import decode_nametable_rle
    scene = by_name('tblLevel_ScenePPUPatternTableHeaderAddress')
    chr_ram = load_scene(rom, scene.cpu, 6)
    segs = decode_nametable_rle(entry_bytes(rom, by_name('tblMainTitleScreenNametable')))
    nametable = bytearray(0x400)
    for addr, data in segs:
        if addr != 0x2000 or len(data) != 0x400:
            raise ValueError(f'title nametable segment ${addr:04X}+{len(data):x} unexpected')
        nametable[:] = data
    pal = entry_bytes(rom, by_name('tblTitleScreenPalette'))
    render_nametable(chr_ram, bytes(nametable), pal[2:34], out_path)

def render_level_screen(rom, registry, level, out_path, width=256, height=240):
    """Level N 首屏近似：连续 layout 行 × 32x32 定义 × 大 tile 调色板合成。
    行→屏的精确滚动语义属 Phase 2 核心逻辑范围，此处 8 行 ≈ 一屏，多模态核对兜底。"""
    from tables import by_name, entry_bytes
    from extractors import parse_metatile32
    idx = level - 1
    layout = entry_bytes(rom, by_name(f'tblLevel{level}ScreenLayout'))
    defs = entry_bytes(rom, by_name(f'tblLevel{level}Layout32x32Definition'))
    pals = entry_bytes(rom, by_name(f'tblLevel{level}Layout32x32DefinitionPalette'))
    scene = by_name('tblLevel_ScenePPUPatternTableHeaderAddress')
    from pattern_loader import load_scene
    chr_ram = load_scene(rom, scene.cpu, idx)
    palette_default = entry_bytes(rom, by_name(f'Level{level}DefaultPalette'))
    row_width = 8  # 256px / 32px
    buf = _canvas(width, height)
    n_mt = len(defs) // 16
    for r in range((height + 31) // 32):
        for c in range(row_width):
            mt = layout[r * row_width + c]
            if mt >= n_mt:
                raise ValueError(f'level {level} metatile index {mt} >= {n_mt}')
            tiles = parse_metatile32(defs[mt*16:(mt+1)*16])
            attr = pals[mt]  # nametable 属性字节：4 个 16x16 子块各 2-bit 组号（bit0-1=TL,2-3=TR,4-5=BL,6-7=BR）
            for ty in range(4):
                for tx in range(4):
                    t = decode_tile(chr_ram[tiles[ty*4+tx] * 16:tiles[ty*4+tx] * 16 + 16])
                    sb = (tx // 2) + (ty // 2) * 2
                    g = (attr >> (sb * 2)) & 3
                    for y in range(8):
                        for x in range(8):
                            col = t[y][x]
                            # 调色板段含 $3F $00 头，BG 组在 +2 起；col 0 用组内背景色，不跳过
                            pi = palette_default[2 + g * 4 + col] & 0x3F
                            _put_px(buf, width, height, c*32 + tx*8 + x, r*32 + ty*8 + y,
                                    NES_PALETTE[pi])
    write_png(out_path, width, height, bytes(buf))
