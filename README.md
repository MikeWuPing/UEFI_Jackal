# 赤色要塞 UEFI 复刻（Jackal UEFI）

[English](README_en.md) | 中文

由Kimi K3 LLM编写。在 UEFI Shell 环境下用 C 语言完整复刻红白机游戏《赤色要塞》（Jackal）：不模拟 NES 硬件，而是把原游戏的 6502 反汇编逐行翻译成平台无关的 C 核心，再以 UEFI GOP 做图形输出、UEFI 服务做输入与定时，在 QEMU（或真实 UEFI 环境）中原生运行。最终 app 无声音——音效调用点保留等价的状态/时间行为，但不引入任何音频依赖。
<img width="512" height="480" alt="intro" src="https://github.com/user-attachments/assets/e6d96822-c5be-4d65-8680-71fd7ab53187" />
<img width="512" height="480" alt="gameplay_turret" src="https://github.com/user-attachments/assets/f73d49c7-0c24-4335-b69d-63f10882d197" />
<img width="512" height="480" alt="boss_fight" src="https://github.com/user-attachments/assets/aa3ec6a8-6ab4-401e-baab-bb5f27181754" />
<img width="512" height="480" alt="ending" src="https://github.com/user-attachments/assets/68c8d8d4-2231-46f3-9a53-98b299656691" />


本仓库包含**全部原创源代码与工具链**（UEFI 应用 + ROM 资源提取器 + 测试/验证工具），
**不包含**原游戏 ROM 及由其提取的数据产物（版权原因，见文末）。

## 保真原则

游戏逻辑**不凭印象重写**，全部对照反汇编还原：

- 逐帧主循环与原版 NMI 驱动模型一致（每帧一次的节拍，非自由运行循环）；
- 多层状态机（`GameControlState` / `GamePlayMode`）状态编号与迁移条件跟汇编一致；
- 32 槽逻辑精灵系统、OAM 生成顺序与槽位旋转策略（`SpriteSlotRotation`）原样保留；
- 坐标/速度的高低字节定点行为、碰撞盒与 HP/死亡状态、关卡 screen 推进与 Boss 计数逐行还原；
- 变量、函数、状态机命名贴近反汇编标签（`Label1096`、`subProcessObjectLogic`、`tblSpriteLogic`…），方便回查；
- 任何与原版不一致之处，代码附近用中文注释说明原因。

保真优先级：帧节拍与状态迁移 > 精灵生成/消亡 > 定点坐标 > 碰撞与 HP > 关卡推进 > 调色板与图案更新 > 渲染缩放。

## 仓库结构

```
JackalPkg/Application/JackalUefi/   UEFI 应用与游戏本体
  core/           平台无关 NES 核心：RAM 镜像、状态机、精灵系统、敌人 AI、
                  碰撞、滚动装载、武器、POW、Continue/结局机
  platform/       GOP 帧缓冲、键盘输入、帧节拍等 UEFI 平台层
  JackalUefi.c    入口与主循环（含验收用调试宏，见下）
JackalPkg/Generated/                ROM 提取产物（由工具生成，不入库）
tools/
  rom_extract/    ROM 资源提取器：iNES 解析、bank 切分、pattern/metasprite/
                  layout/palette 解码，输出 C 数组 + bin + manifest + 抽样 PNG
  core_test/      宿主端单元测试（cl 编译 core + Generated + tests 直接运行）
  qemu_capture.py QEMU 运行 + monitor screendump 截图 + 按键注入
  verify_phase*.py 串口日志端到端断言（标题/入场/全程通关/续关/结局）
  Build-Jackal.ps1 / Run-JackalQemu.ps1 / JackalConfig.ps1  构建与运行脚本
```

参考用全注释反汇编（RayofJay，MIT No Attribution）未随仓库分发，
其公开发布版本可检索 "NES Jackal Disassembly Fully Commented" 获取。

## 构建与运行

### 环境依赖

- Windows + Visual Studio 2019（C 工具链）
- EDK2 源码树（本机克隆即可；脚本默认 `D:\Work\Code\edk2`，
  路径不同请改 `tools/Build-Jackal.ps1` 里的 `WORKSPACE`/`EDK2_DIR`）
- QEMU（默认 `C:\Program Files\qemu`）与 OVMF 固件
  （在 `tools/JackalConfig.ps1` 配置）
- Python 3（提取器与测试/验证工具链）

### 步骤

1. 将**合法获得**的原版 `orgrom.nes` 放入 `ref/`（本仓库不分发，原因见文末）。
2. 提取资源：`python tools/rom_extract/run_extract.py`
   （生成 `assets/` 与 `JackalPkg/Generated/`）。
3. 构建：`powershell -NoProfile -ExecutionPolicy Bypass -File tools/Build-Jackal.ps1`
   （自动递增版本号，产物同步到 `run/hda/`）。
4. 运行验证：`python tools/qemu_capture.py --seconds 40`
   （截图与串口日志落在 `snapshot/`、`run_logs/`），
   或直接用 QEMU 加载 `run/hda` 游玩（SDL 窗口）。

### 按键映射

| NES 手柄 | 键盘 |
|---|---|
| 方向键 DPad | 方向键 |
| A（主武器） | X |
| B（机枪） | Z |
| Start | 回车 |
| Select | Backspace |
| 退出 app | Esc（或 Select+Start） |

## 测试

- 宿主单元测试：`python tools/core_test/run_tests.py`
  （约 160 项：状态机、AI、碰撞、滚动、武器、建筑、结局机等）
- 提取器回归：`cd tools/rom_extract && python -m unittest discover -s tests`
- 端到端断言（需验证构建，见下）：
  - `python tools/verify_phase3.py --mode autostart`（Chinook 入场 + 首批生成）
  - `python tools/verify_phase4.py --mode autostart`（Level 1 全程通关）
  - `python tools/verify_phase6.py --mode autostart` / `--mode continue`（过场/续关）
  - `python tools/verify_phase7.py --mode force-ending`（结局机回环）

## 调试构建宏（`-VerifyDefine`，非原版行为，仅验收/试玩用）

- `JACKAL_DEBUG_AUTO_START`：自动按 Start 进游戏
- `JACKAL_DEBUG_AUTOSCROLL`：模拟玩家贴顶推进（配合 boss 战辅助驾驶器）
- `JACKAL_DEBUG_FORCE_ENDING`：直驱结局机
- `JACKAL_DEBUG_MAX_WEAPON`：双吉普主武器恒满级 + 命数恒 99（试玩便利）

示例：`powershell -File tools/Build-Jackal.ps1 -VerifyDefine JACKAL_DEBUG_AUTO_START,JACKAL_DEBUG_AUTOSCROLL`

## 技术要点

- **核心/平台分层**：`core/` 镜像原 RAM 布局（`$0500-$074F` 精灵数组、碰撞图 `$0300/$0400`、OAM shadow、`$0770` PPU 更新队列），可在 PC 上用 cl 直接编译运行全部逻辑测试；`platform/` 只负责呈现、输入、定时。
- **资源提取而非内嵌**：CHR ROM size 为 0（UxROM），图案数据在 PRG bank 中、运行时解压入 CHR RAM；提取器跟踪 `subLoadNewPatternTable` 与各图形/布局表，把 2bpp tile、palette、metasprite、screen layout 转成 C 数组，全部产物可重复生成并在注释中记录汇编表名与 bank 偏移。
- **碰撞体系**：2-bit×4/字节、8px 粒度、六级阈值分类，与原算法逐字节一致；NearLookAhead/FarLookAhead 前眺碰撞逐行还原。
- **测试即证据**：每个还原的关键机制（如滚动协同、父子生成、溅射振荡）都有宿主端锚定测试；端到端用 QEMU 串口断言覆盖完整通关链。

## 版权与许可

- 本仓库的**原创代码与工具**（`JackalPkg/`、`tools/`）以 MIT License 发布（见仓库根 `LICENSE`）。
- 参考用全注释反汇编为 RayofJay 的作品（MIT No Attribution），未随本仓库分发。
- **《赤色要塞》游戏本体（ROM、图像、关卡数据等）是 Konami 的知识产权**。
  本仓库**不包含也不分发**原 ROM 及由 ROM 提取的资源产物：
  `ref/orgrom.nes`、`assets/`、`JackalPkg/Generated/` 均已列入 `.gitignore`，
  请使用者自行合法获取 ROM 后用自带工具链生成。
  本项目仅为技术学习目的的非商业复刻，与 Konami 无关，亦未获其授权。

## 致谢

- RayofJay 的全注释反汇编（MIT No Attribution），是本项目逐行还原的基石。
- EDK2、QEMU 开源社区。
