"""全管线入口：parse → validate → emit → samples。任一步失败退出非零，不产出物。"""
import sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from ines import load_rom
from tables import REGISTRY, by_name, entry_bytes
from validate import validate_registry
from pattern_loader import load_scene
from emit_c import emit_all
from extractors import parse_metasprite
from render_png import render_metasprite, render_chr_page, render_level_screen, render_title_screen

def main():
    rom = load_rom(ROOT / 'ref' / 'orgrom.nes')
    errors = validate_registry(rom, REGISTRY)
    if errors:
        for e in errors:
            print('VALIDATE:', e)
        return 1
    scene = by_name('tblLevel_ScenePPUPatternTableHeaderAddress')
    chr0 = load_scene(rom, scene.cpu, 0)
    if not any(chr0[0:0x2000]):
        print('VALIDATE: scene 0 pattern region empty')
        return 1
    paths = emit_all(rom, REGISTRY, ROOT)
    samples = ROOT / 'assets' / 'samples'
    samples.mkdir(parents=True, exist_ok=True)
    render_chr_page(chr0, (0x0F, 0x16, 0x21, 0x30), samples / 'level1_chr_bg.png')
    render_metasprite(chr0, parse_metasprite(entry_bytes(rom, by_name('tblJeepUpDown'))),
                      samples / 'jeep.png')
    render_level_screen(rom, REGISTRY, 1, samples / 'level1_screen.png')
    render_title_screen(rom, REGISTRY, samples / 'title_screen.png')
    print('OK:', paths['c'], paths['manifest'])
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
