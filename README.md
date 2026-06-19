# MeowShow 🐱

> STM32F103C8T6 驱动的 CH1116 OLED 猫猫 GIF 动画播放器

在 128×64 像素的 OLED 小屏幕上循环播放猫猫动画，基于 STM32 HAL + ARM GCC 构建。

## 硬件需求

| 组件 | 规格 |
|------|------|
| 主控 | STM32F103C8T6（Cortex-M3, 64KB Flash, 20KB SRAM） |
| 开发板 | STM32_KIT 学习板 |
| 屏幕 | CH1116 OLED, 128×64, I2C 接口 |
| 调试器 | ST-LINK（SWD 接口） |

**I2C 接线：**

| STM32 引脚 | OLED 引脚 |
|------------|-----------|
| PB6 (SCL) | SCL |
| PB7 (SDA) | SDA |
| 3.3V | VCC |
| GND | GND |

## 快速开始

### 1. 安装依赖

- **ARM GCC 工具链**：[arm-none-eabi-gcc](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain)
- **CMake** ≥ 3.22 + **Ninja**
- **Python 3** + Pillow（仅更换动画时需要）

```bash
pip install pillow
```

### 2. 构建固件

```bash
# 配置（Debug）
cmake --preset ninja-debug

# 编译
cmake --build build/ninja-debug

# 或 Release（优化体积）
cmake --preset ninja-release
cmake --build build/ninja-release
```

构建产物在 `build/ninja-debug/MeowShow.elf`。

### 3. 烧录

使用 ST-LINK 通过 SWD 接口烧录（PA14=SWCLK, PA13=SWDIO）：

```bash
# STM32CubeProgrammer
STM32_Programmer_CLI -c port=SWD -w build/ninja-debug/MeowShow.elf -v

# 或 OpenOCD
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
        -c "program build/ninja-debug/MeowShow.elf verify reset exit"
```

### 4. 启动效果

烧录后 OLED 将依次显示：

1. **启动画面**（~1 秒）：笑脸 + "Loading Meow..."
2. **猫猫动画**：循环播放 32 帧猫猫动画

## 更换动画

### 一键替换

```bash
# 1. 将你的 GIF 放到项目根目录
# 2. 运行自动脚本（支持任意路径的 GIF）
python tools/auto_gif.py meow1.gif -n "Meow!"

# 3. 重新构建烧录
cmake --build build/ninja-debug
```

`auto_gif.py` 会自动：
- 合成 GIF 帧（处理透明度和 disposal）
- 缩放到 128×64
- 二值化并生成 C 头文件
- 保存调试预览帧到 `tools/debug_frames/`

### 参数说明

```bash
python tools/auto_gif.py <gif_path> [options]

参数：
  gif_path               源 GIF 文件路径（必填）
  -n, --name NAME        动画显示名称（默认从文件名推导）
  -o, --output PATH      输出头文件路径（默认 Core/Inc/<name>_anim.h）
  -t, --threshold N      二值化阈值 0-255，越低像素越少（默认 140）
  -m, --max-frames N     目标帧数，受 Flash 预算限制（默认 32）
  --bg {white,black}     合成画布背景色（默认 white）
  --no-debug             不生成调试 PNG

示例：
  python tools/auto_gif.py cat.gif
  python tools/auto_gif.py cat.gif -n "MyCat" -t 120 -m 24
  python tools/auto_gif.py dark.gif --bg black --no-debug
```

### 手动调优

如果默认效果不理想，可以用 `gif_to_c.py` 手动精细控制：

```bash
# 调整二值化阈值（默认 140，越高画面越亮）
python tools/gif_to_c.py meow.gif meow --threshold 100 --debug-dir tools/debug_frames

# 深色背景 GIF 使用黑色画布合成
python tools/gif_to_c.py meow.gif meow --bg black --debug-dir tools/debug_frames

# 调整帧数（Flash 预算：最多约 48 帧）
python tools/gif_to_c.py meow.gif meow --max-frames 40 --debug-dir tools/debug_frames
```

调试预览帧保存在 `tools/debug_frames/`：
- `rgb_*.png` — 合成后的彩色帧
- `oled_*.png` — OLED 实际显示效果预览（黑白）

### GIF 要求建议

| 属性 | 建议 |
|------|------|
| 分辨率 | 任意（会自动缩放到 128×64） |
| 内容 | 深色线条/角色 + 浅色背景效果最佳 |
| 帧数 | 无限制（会自动抽帧到 Flash 预算内） |
| 背景 | 透明或白色背景（默认白色画布合成） |

## 项目结构

```
MeowShow/
├── Core/
│   ├── Inc/                     # 头文件
│   │   ├── oled.h               # OLED 驱动 API
│   │   ├── gif_animation.h      # 动画引擎 API
│   │   └── meow_anim.h          # 生成的动画帧数据
│   └── Src/
│       ├── main.c               # 主程序
│       ├── oled.c               # CH1116 OLED 驱动
│       ├── gif_animation.c      # 动画播放引擎
│       └── font.c               # 位图字库
├── Drivers/                     # STM32 HAL + CMSIS
├── cmake/                       # CMake 构建配置
├── tools/
│   ├── auto_gif.py              # 一键 GIF 转换
│   ├── gif_to_c.py              # GIF → C 数组转换器
│   └── gen_font_c.py            # 字库生成器
├── meow.gif                     # 原始 GIF 素材
├── CMakeLists.txt               # 顶层 CMake
├── CMakePresets.json            # CMake 预设
├── STM32F103XX_FLASH.ld         # 链接脚本
├── startup_stm32f103xb.s        # 启动汇编
├── PROJECT_DESCRIPTION.md       # 详细设计文档
└── README.md                    # 本文件
```

## 常用命令

```bash
# 查看 Flash 用量
arm-none-eabi-size build/ninja-debug/MeowShow.elf

# 生成编译数据库（供 clangd 代码补全使用）
cmake --preset ninja-debug  # 自动生成 compile_commands.json

# 清理构建
rm -rf build/
```

## 故障排除

| 现象 | 可能原因 | 解决 |
|------|---------|------|
| 屏幕全黑 | I2C 地址不匹配 | 检查 OLED 地址：多数模块为 0x3C 或 0x3D。修改 `oled.c` 中 `OLED_I2C_ADDR` |
| 屏幕下半部分花屏 | I2C 传输截断 | 确认 `OLED_ShowFrame()` 使用逐页传输（已修复） |
| 动画显示不正常 | GIF 合成画布颜色不对 | 尝试 `--bg white` 或 `--bg black` |
| 动画太暗/太亮 | 二值化阈值不适合 | 调整 `--threshold`（默认 140） |
| 编译失败（Flash 溢出） | 帧数太多 | 减少 `--max-frames` |
| `HAL_Delay` 不准确 | SysTick 未更新 | 确认 `SystemClock_Config()` 末尾有 `HAL_SYSTICK_Config()` |

## License

本项目基于 STM32CubeMX HAL 库构建。CMSIS 和 HAL 驱动代码版权归 STMicroelectronics 所有。

---

🤖 Generated with [Claude Code](https://claude.com/claude-code)
