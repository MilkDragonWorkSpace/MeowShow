# MeowShow — STM32 OLED 猫猫动画播放器

## 项目概览

MeowShow 是一个运行在 STM32F103C8T6 微控制器上的嵌入式固件项目。它的核心功能是将一张猫猫 GIF 动画（meow.gif）转换并播放在一块 128×64 像素的 CH1116 OLED 显示屏上。

项目基于 STM32CubeMX HAL 库生成底层外设配置，使用 CMake + ARM GCC（arm-none-eabi-gcc）工具链构建，运行在 STM32_KIT 学习板上。

---

## 硬件平台

| 项目 | 规格 |
|------|------|
| 主控芯片 | STM32F103C8T6（Cortex-M3, 64KB Flash, 20KB SRAM） |
| 开发板 | STM32_KIT 学习板 |
| 显示屏 | CH1116 OLED, 128×64 像素, I2C 接口 |
| I2C 地址 | **0x3D**（7 位地址；文档常以 8 位写地址 0x7A 标注） |
| I2C 引脚 | PB6=SCL, PB7=SDA |
| I2C 速率 | 400 kHz（Fast Mode） |
| I2C 超时 | 100 ms（防止 OLED 无应答时 MCU 死等） |
| 外部晶振 | 8 MHz HSE |
| 系统时钟 | 72 MHz（PLL ×9, 来源 HSE；HSE 失败时回退 HSI 64 MHz） |
| 调试接口 | ST-LINK（PA14=SWCLK, PA13=SWDIO） |

---

## 软件架构

### 文件结构

```
MeowShow/
├── .clangd                       # clangd 语言服务器配置
├── .gitignore                    # Git 忽略规则
├── CMakeLists.txt                # 顶层 CMake 构建配置
├── CMakePresets.json             # CMake 预设（Debug/Release, Ninja/VS）
├── MeowShow.ioc                  # STM32CubeMX 项目文件
├── STM32F103XX_FLASH.ld          # 链接脚本
├── startup_stm32f103xb.s         # 启动汇编
├── meow.gif                      # 原始 GIF 动画素材（240×240, 65 帧）
├── PROJECT_DESCRIPTION.md        # 本文档
├── stm32_kit_schematic_bried.md  # 学习板原理图说明
├── cmake/
│   ├── gcc-arm-none-eabi.cmake   # ARM GCC 工具链文件
│   ├── starm-clang.cmake         # ARM Clang 工具链文件（备用）
│   └── stm32cubemx/              # CubeMX 生成的 CMake 子项目
├── Core/
│   ├── Inc/
│   │   ├── main.h                # 主头文件（含 I2C 句柄声明）
│   │   ├── gpio.h                # GPIO 配置
│   │   ├── oled.h                # OLED 驱动 API
│   │   ├── gif_animation.h       # GIF 动画播放引擎 API
│   │   ├── meow_anim.h           # 自动生成的猫猫动画帧数据（32 帧）
│   │   ├── stm32f1xx_hal_conf.h  # HAL 模块配置（已启用 I2C）
│   │   └── stm32f1xx_it.h
│   ├── Src/
│   │   ├── main.c                # 主程序入口
│   │   ├── gpio.c                # GPIO 初始化（含 GPIOB 时钟）
│   │   ├── oled.c                # OLED 驱动实现
│   │   ├── gif_animation.c       # 动画播放引擎实现
│   │   ├── font.c                # 位图字库（8x6/12x6/16x8/16x16）
│   │   ├── stm32f1xx_hal_msp.c   # HAL MSP（含 I2C1 MSP 初始化）
│   │   ├── stm32f1xx_it.c
│   │   ├── system_stm32f1xx.c
│   │   ├── syscalls.c
│   │   └── sysmem.c
├── Drivers/
│   ├── CMSIS/                    # ARM CMSIS 核心头文件
│   ├── STM32F1xx_HAL_Driver/     # STM32 HAL 驱动库（含 I2C）
├── tools/
│   ├── gif_to_c.py               # GIF → C 数组转换器（支持帧合成与抽帧）
│   ├── auto_gif.py               # 一键自动化：meow.gif → meow_anim.h
│   ├── gen_font_c.py             # 字库 C 代码生成器
│   └── debug_frames/             # 调试预览帧（已加入 .gitignore）
└── build/                        # 构建输出（已加入 .gitignore）
```

### 模块依赖关系

```
main.c
  ├── HAL (HAL_Init, SystemClock_Config, GPIO, I2C)
  ├── oled.c/h          → HAL I2C (hi2c1)
  │   └── font.c        → 提供 4 种位图字库
  ├── gif_animation.c/h → oled.c (绘制 API + 帧缓冲)
  └── meow_anim.h       → gif_animation.h (GifAnimation 结构)
       └── 32 帧 × 1024 字节位图数据
```

---

## 核心模块详细设计

### 1. OLED 驱动（oled.c / oled.h）

**职责**：驱动 CH1116 OLED（指令集兼容 SSD1306），提供绘图和字体渲染 API。

**帧缓冲布局**：
- 大小：1024 字节（8 页 × 128 列）
- 排布：页优先（page-first），即 `buffer[page][col]`
- 每个字节代表同一列内 8 个垂直像素，Bit 0 = 顶部像素
- 与 CH1116 页寻址模式下的传输顺序一致

**CH1116 初始化流程**：
1. 等待 50ms（OLED 上电速度慢于 STM32 启动）
2. 发送 Display OFF（0xAE）
3. 配置振荡频率、复用比（64 行）、显示偏移
4. 启用电荷泵（charge pump, 0x8D + 0x14）
5. 设置**页寻址模式**（0x20 + 0x02）— 比水平寻址模式更可靠
6. 配置段重映射（0xA1）和 COM 扫描方向（0xC8）
7. 配置 COM 引脚硬件（0xDA + 0x12）、对比度、预充电周期、VCOMH
8. 恢复正常显示、解除反转、停用滚动
9. 再等 100ms 后发送 Display ON（0xAF）
10. 清空帧缓冲并刷新

**关键设计决策 — 逐页传输**：
`OLED_ShowFrame()` 按页（8 页 × 128 字节）逐页发送数据，而非一次性发送全部 1024 字节。原因是：
- STM32F1 的 I2C 外设在 `HAL_I2C_Mem_Write` 中存在隐式的传输大小上限（约 255 字节）
- 一次性发送 1024 字节会导致仅前 512 字节被写入，后 4 页保留上电随机数据（表现为底部雪花白屏）
- 逐页发送：每页设置页地址（0xB0+page）、列归零（0x00, 0x10）、发送 128 字节，避免任何 HAL 传输限制

**I2C 数据发送**：
`OLED_WriteData()` 使用栈上 129 字节缓冲区（1 字节控制码 0x40 + 128 字节像素数据），通过 `HAL_I2C_Master_Transmit` 一次性发送。不再使用 `HAL_I2C_Mem_Write`（其大小限制不可靠）。

**绘图 API**：
- `OLED_SetPixel(x, y, color)` — 单像素
- `OLED_DrawLine(x1, y1, x2, y2, color)` — Bresenham 直线
- `OLED_DrawRectangle / DrawFilledRectangle` — 矩形 / 填充矩形
- `OLED_DrawCircle / DrawFilledCircle` — Midpoint 圆 / 填充圆
- `OLED_DrawTriangle / DrawFilledTriangle` — 三角形 / 填充三角形
- `OLED_PrintString(x, y, str, font, color)` — 字符串渲染
- `OLED_DrawImage(x, y, img, color)` — 位图图像渲染
- `OLED_DrawFullBitmap(data[1024])` — 全帧快速 blit（直接 memcpy）
- `OLED_NewFrame()` — 清空帧缓冲
- `OLED_ShowFrame()` — 逐页发送帧缓冲到 OLED

**字库**（font.c，由 `tools/gen_font_c.py` 生成）：
- `font8x6` — 8×6 像素，95 个可打印 ASCII 字符
- `font12x6` — 12×6 像素
- `font16x8` — 16×8 像素
- `font16x16` — 16×16 像素（适合汉字或大标题）
- 字库数据采用列优先存储

---

### 2. GIF 动画引擎（gif_animation.c / gif_animation.h）

**职责**：提供通用动画播放框架，支持两种模式。

**模式一：程序化绘制（Procedural）**

不消耗额外 Flash，每帧通过回调函数实时绘制。内置三个演示动画：

| 动画 | 帧数 | 描述 |
|------|------|------|
| Blink Face | 5 帧 | 笑脸逐帧眨眼动画 |
| Heart Beat | 2 帧 | 心跳缩放动画 |
| Loading Spinner | 8 帧 | 旋转加载指示器 |

API：`GifAnim_PlayProc(frames, count, name, loop_count)` — 阻塞式播放
API：`GifAnim_ShowDemo()` — 顺序播放所有内置演示动画

**模式二：预渲染位图（Bitmap）**

将 GIF 的每一帧预先转换为 1024 字节的 CH1116 格式位图数组，存储在 Flash 中。播放时直接从数组 blit 到帧缓冲。

核心结构：
```c
typedef struct {
    const uint8_t *bitmap;   // 1024 字节 CH1116 格式位图
    uint16_t       delay_ms; // 该帧显示时长（毫秒）
} GifFrame;

typedef struct {
    const GifFrame *frames;
    uint16_t        frame_count;
    uint16_t        loop_count; // 0 = 无限循环
    const char     *name;
} GifAnimation;
```

API：
- `GifAnim_Play(anim, loop_forever)` — 阻塞式播放，带底部标签栏
- `GifAnim_Start(anim)` — 非阻塞启动
- `GifAnim_Tick()` — 非阻塞帧更新（放入主循环或定时器中断中调用）

每帧渲染流程：
1. `OLED_NewFrame()` 清空帧缓冲
2. `OLED_DrawFullBitmap(bitmap)` 快速 blit 帧数据
3. 绘制底部分隔线和动画名称（font8x6 字体）
4. `OLED_ShowFrame()` 逐页发送到 OLED
5. 忙等待至帧延迟结束

---

### 3. 主程序（main.c）

**启动流程**：

1. `HAL_Init()` — 初始化 HAL 库
2. `SystemClock_Config()` — 配置系统时钟：
   - 首选方案：HSE 8MHz → PLL ×9 = 72MHz
   - 回退方案：若 HSE 启动失败 → HSI 8MHz/2 → PLL ×16 = 64MHz
   - **重要**：STM32F103C8T6 无 HSE 预分频器，不可设置 `HSEPredivValue`
   - 时钟配置后显式调用 `HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq() / 1000U)` 确保 `HAL_Delay()` 时序正确
3. `MX_GPIO_Init()` — 使能 GPIOA + GPIOB 时钟
4. `MX_I2C1_Init()` — 配置 I2C1：
   - PB6=SCL, PB7=SDA
   - 开漏输出, AF 模式
   - 400kHz Fast Mode
5. `HAL_Delay(50)` — 等待 OLED 上电
6. `OLED_Init()` — CH1116 初始化

**启动画面**（约 1 秒）：
- 绘制笑脸：圆形脸部轮廓（半径 18）、两只实心圆眼睛、抛物线微笑嘴
- 底部显示 "Loading Meow..."（font12x6 字体）

**主循环**：
- 无限循环调用 `GifAnim_Play(&gif_meow_anim, 1)` — 循环播放猫猫动画

**I2C 引脚配置**（在 HAL_I2C_MspInit 中）：
- 使能 GPIOB 和 I2C1 时钟
- PB6/PB7 配置为开漏 AF 模式，高速

---

### 4. GIF 转 C 数组工具链

#### gif_to_c.py

将任意 GIF 文件转换为 C 头文件，包含可直接被 gif_animation.h API 使用的数据结构。

**处理流程**：
1. 使用 Pillow 逐帧读取 GIF
2. **帧合成（Compositing）**：GIF 帧通常是增量差量（delta），仅包含变化的像素。工具将每帧的 RGBA 数据（保留透明度）逐个叠加到白色画布上——透明像素不覆盖画布。这正确重建了完整画面
3. 合成后的画面缩放到 128×64（LANCZOS 插值）
4. **二值化**：灰度值 < 阈值（默认 140）→ OLED 点亮；≥ 阈值 → OLED 熄灭
5. 按 CH1116 页优先 / 列优先布局重组位数据
6. **智能抽帧**：从原始帧中均匀采样 N 帧，跳过内容过少的空白帧，调整帧延迟以保持动画节奏
7. 生成 C 代码：`frame_N[1024]` 数组 + `GifFrame[]` + `GifAnimation` 结构体

**命令行选项**：
```bash
python tools/gif_to_c.py <gif> [name] [--threshold N] [--bg white|black]
                          [--max-frames N] [--debug-dir DIR]
```

**Flash 预算**：STM32F103C8T6 共 64KB Flash，帧数据默认最多 32KB（32 帧），为代码留出约 32KB。

#### auto_gif.py

一键自动化脚本，串联整个流程：

1. 读取 `meow.gif`
2. 调用 `gif_to_c.py` 的逻辑进行帧合成、抽帧、二值化
3. 写 `Core/Inc/meow_anim.h`
4. 保存调试预览帧到 `tools/debug_frames/`（oled_*.png 为 OLED 实际显示效果预览）
5. 输出 Flash 用量统计

#### gen_font_c.py

字库 C 代码生成器。手工设计 95 个 ASCII 字符的点阵字形，以列优先格式输出为 `Core/Src/font.c`。

---

### 5. 当前动画数据（meow_anim.h）

| 属性 | 值 |
|------|-----|
| 来源文件 | meow.gif（240×240, 65 帧） |
| 输出帧数 | 32 帧（均匀采样，跳过空白帧） |
| 每帧大小 | 1024 字节 |
| 总 Flash 占用 | 32,768 字节 (~32 KB) |
| 帧延迟范围 | 50–1000 ms（经抽帧延迟调整） |
| 循环模式 | 无限循环 |
| 动画名称 | "Meow!" |

动画数据由 `auto_gif.py` 自动生成，帧数据以 `static const uint8_t meow_frame_N[1024]` 的形式存储在 Flash 中。

---

## 构建与烧录

### 前置条件

- ARM GCC 工具链（arm-none-eabi-gcc）
- CMake ≥ 3.22
- Ninja 构建系统
- Python 3 + Pillow（用于 GIF 转换工具）

### 构建步骤

```bash
# 配置（Debug 构建）
cmake --preset ninja-debug

# 编译
cmake --build build/ninja-debug

# 或使用 Release 构建
cmake --preset ninja-release
cmake --build build/ninja-release
```

### 更换动画

```bash
# 方法一：替换 meow.gif 后运行自动化脚本
python tools/auto_gif.py

# 方法二：手动转换任意 GIF（可调参数）
python tools/gif_to_c.py your_animation.gif anim_name --threshold 140 --max-frames 32

# 调优建议：
#   --threshold 100  更少的点亮像素（更暗的画面）
#   --threshold 160  更多的点亮像素（更亮的画面）
#   --bg black       深色背景的 GIF 使用黑色画布合成
#   --debug-dir DIR  保存调试帧以预览 OLED 效果
```

### 烧录到设备

通过 ST-LINK 接口（PA14=SWCLK, PA13=SWDIO）使用 STM32CubeProgrammer 或 OpenOCD 烧录生成的 ELF/HEX 文件。

---

## Flash 与 RAM 使用分析

| 资源 | 用量 | 备注 |
|------|------|------|
| Flash（位图帧） | ~32 KB | 32 帧 × 1024 字节 |
| Flash（代码） | ~12–15 KB | HAL 驱动 + OLED 驱动 + 动画引擎 + 字库 |
| Flash（总计） | ~44–47 KB | 占 64KB 的 ~72%，有余量 |
| RAM（帧缓冲） | 1 KB | 静态分配的 oled_buffer[8][128] |
| RAM（栈/堆） | ~2–4 KB | 取决于 HAL 和编译器设置 |
| RAM（总计） | ~3–5 KB | 占 20KB 的 ~25%，充足 |

---

## 关键技术决策与踩坑记录

1. **I2C 地址 0x3D vs 0x7A**：数据手册常以 8 位写地址标注（0x7A = 0x3D << 1）。HAL 期望的是 7 位地址左移 1 位，即 `(0x3D << 1) = 0x7A`。若误将 0x7A 当作 7 位地址再次左移（→ 0xF4），OLED 完全不响应，屏幕全黑。

2. **逐页 I2C 传输**：`HAL_I2C_Mem_Write` 在 STM32F1 上存在隐式传输大小上限。一次性发送 1024 字节仅前 ~512 字节被写入，后 4 页保留上电随机数据，表现为**屏幕下半部分雪花白屏**。改为 8 次独立的 128 字节传输 + 页寻址模式彻底解决。

3. **页寻址模式**：页寻址模式（0x02）比水平寻址模式（0x00）在逐页传输时更可靠——每次写完 128 字节后列指针自动归零，无需依赖窗口地址设置。

4. **GIF 帧合成（Compositing）**：GIF 帧不是完整图像，而是增量差量——每帧仅包含变化的像素区域。必须将每帧叠加到累积画布上（透明像素不覆盖）才能重建完整画面。源 GIF 为透明背景的 Web 图片，需使用**白色画布**合成，否则猫在黑色画布上完全不可见。

5. **抽帧策略**：简单取前 N 帧会包含开头的空白帧（GIF 编码占位帧），导致动画循环时猫"消失"一下。均匀采样 + 跳过内容低于 3% 的帧 + 调整延迟，保证动画流畅且无缝循环。

6. **二值化阈值**：阈值 140 对于深色线条/填充在浅色背景上的 GIF 效果最佳。`tools/debug_frames/oled_*.png` 可以直接预览 OLED 实际显示效果，便于调优。

7. **HSE 预分频器**：STM32F103C8T6 无 HSE 预分频器。设置 `HSEPredivValue` 可能导致 `HAL_RCC_OscConfig()` 返回错误，触发 HSI 回退或时钟异常。

8. **SysTick 显式重配置**：`HAL_RCC_ClockConfig` 在部分 HAL 版本中不会自动更新 SysTick 频率。时钟切换后显式调用 `HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq() / 1000U)` 确保 `HAL_Delay()` 正确。

9. **I2C 有限超时**：使用 `HAL_MAX_DELAY`（无限等待）意味着 OLED 无应答时 MCU 永久死等。改为 100ms 超时，即使 I2C 通信失败也能继续运行。

10. **Python 工具而非编译时生成**：选择运行时运行 Python 脚本而非 CMake 的 `add_custom_command`，因为工具链环境不一定有 Python/Pillow，且动画数据很少变动，预生成更简单可靠。

---

## 未来扩展方向

- 支持通过按键（PB12/PB13）切换多个动画
- 利用非阻塞播放 API 实现动画与传感器交互（如 AHT20 温湿度显示叠加）
- 利用 WS2812C 灯带（PB4）同步播放氛围灯效
- 通过蓝牙串口（PB10/PB11）无线更新 GIF 数据
- 使用外部 SPI Flash 存储更多/更大的动画帧，突破 64KB 内置 Flash 限制
