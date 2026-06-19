# MeowScreen — STM32 OLED 猫猫动画播放器

## 项目概览

MeowScreen 是一个运行在 STM32F103C8T6 微控制器上的嵌入式固件项目。它的核心功能是将一张猫猫 GIF 动画（meow.gif）转换并播放在一块 128×64 像素的 CH1116 OLED 显示屏上。

项目基于 STM32CubeMX HAL 库生成底层外设配置，使用 CMake + ARM GCC（arm-none-eabi-gcc）工具链构建，运行在 STM32_KIT 学习板上。

---

## 硬件平台

| 项目 | 规格 |
|------|------|
| 主控芯片 | STM32F103C8T6（Cortex-M3, 64KB Flash, 20KB SRAM） |
| 开发板 | STM32_KIT 学习板 |
| 显示屏 | CH1116 OLED, 128×64 像素, I2C 接口 |
| I2C 地址 | 0x7A（7 位地址） |
| I2C 引脚 | PB6=SCL, PB7=SDA |
| I2C 速率 | 400 kHz（Fast Mode） |
| 外部晶振 | 8 MHz HSE |
| 系统时钟 | 72 MHz（PLL x9, 来源 HSE；HSE 失败时回退 HSI 64MHz） |
| 调试接口 | ST-LINK（PA14=SWCLK, PA13=SWDIO） |

---

## 软件架构

### 文件结构

```
MeowScreen/
├── CMakeLists.txt              # 顶层 CMake 构建配置
├── CMakePresets.json           # CMake 预设（Debug/Release, Ninja/VS）
├── MeowScreen.ioc              # STM32CubeMX 项目文件
├── STM32F103XX_FLASH.ld        # 链接脚本
├── startup_stm32f103xb.s       # 启动汇编
├── meow.gif                    # 原始 GIF 动画素材
├── cmake/
│   ├── gcc-arm-none-eabi.cmake # ARM GCC 工具链文件
│   └── stm32cubemx/            # CubeMX 生成的 CMake 子项目
├── Core/
│   ├── Inc/
│   │   ├── main.h              # 主头文件
│   │   ├── gpio.h              # GPIO 配置
│   │   ├── oled.h              # OLED 驱动 API
│   │   ├── gif_animation.h     # GIF 动画播放引擎 API
│   │   ├── meow_anim.h         # 自动生成的猫猫动画帧数据
│   │   ├── stm32f1xx_hal_conf.h
│   │   └── stm32f1xx_it.h
│   ├── Src/
│   │   ├── main.c              # 主程序入口
│   │   ├── gpio.c              # GPIO 初始化
│   │   ├── oled.c              # OLED 驱动实现
│   │   ├── gif_animation.c     # 动画播放引擎实现
│   │   ├── font.c              # 位图字库（8x6/12x6/16x8/16x16）
│   │   ├── stm32f1xx_hal_msp.c
│   │   ├── stm32f1xx_it.c
│   │   ├── system_stm32f1xx.c
│   │   ├── syscalls.c
│   │   └── sysmem.c
├── Drivers/
│   ├── CMSIS/                  # ARM CMSIS 核心头文件
│   ├── STM32F1xx_HAL_Driver/   # STM32 HAL 驱动库
├── tools/
│   ├── gif_to_c.py             # GIF → C 数组转换器
│   └── auto_gif.py             # 一键自动化：meow.gif → meow_anim.h → main.c
└── build/                      # 构建输出（已加入 .gitignore）
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
- 排布：列优先（page-first），即 `buffer[page][col]`
- 每个字节代表同一列内 8 个垂直像素，Bit 0 = 顶部像素
- 与 CH1116 I2C 水平寻址模式的传输顺序一致

**CH1116 初始化流程**：
1. 等待 50ms（OLED 上电速度慢于 STM32 启动）
2. 发送 Display OFF（0xAE）
3. 配置振荡频率、复用比（64 行）、显示偏移
4. 启用电荷泵（charge pump）
5. 设置水平寻址模式
6. 配置段重映射（A1）和 COM 扫描方向（C8）
7. 配置 COM 引脚硬件、对比度、预充电周期、VCOMH
8. 恢复正常显示、解除反转、停用滚动
9. 再等 100ms 后发送 Display ON（0xAF）
10. 清空帧缓冲并刷新

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
- `OLED_ShowFrame()` — 将帧缓冲通过 I2C 整体发送到 OLED

**字库**（font.c）：
- `font8x6` — 8×6 像素，95 个可打印 ASCII 字符
- `font12x6` — 12×6 像素
- `font16x8` — 16×8 像素
- `font16x16` — 16×16 像素（适合汉字或大标题）
- 字库数据采用列优先存储

**I2C 通信**：
- 指令发送：`[0x00, cmd]` 两字节
- 数据发送：小量逐字节发 `[0x40, data]`；大量（>32 字节）使用 `HAL_I2C_Mem_Write` 一次性传输

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
3. 绘制底部分隔线和动画名称
4. `OLED_ShowFrame()` 发送到 OLED
5. 忙等待至帧延迟结束

---

### 3. 主程序（main.c）

**启动流程**：

1. `HAL_Init()` — 初始化 HAL 库
2. `SystemClock_Config()` — 配置系统时钟：
   - 首选方案：HSE 8MHz → PLL ×9 = 72MHz
   - 回退方案：若 HSE 启动失败 → HSI 8MHz/2 → PLL ×16 = 64MHz
3. `MX_GPIO_Init()` — 使能 GPIOA 时钟
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
2. 每帧：缩放至 128×64 → 灰度 → 二值化（阈值 128）
3. 按 CH1116 列优先 / 页优先布局重组位数据
4. 提取 GIF 帧延迟（centiseconds → milliseconds）
5. 生成 C 代码：`frame_N[1024]` 数组 + `GifFrame[]` + `GifAnimation` 结构体

**Flash 预算**：STM32F103C8T6 共 64KB Flash，帧数据最多约 52KB（留 12KB 给代码），即最多约 50 帧。

#### auto_gif.py

一键自动化脚本，串联整个流程：

1. 读取 `meow.gif`
2. 调用 `gif_to_c.py` 的逻辑转换为 C 代码
3. 写 `Core/Inc/meow_anim.h`
4. 重写 `Core/Src/main.c`，将主循环指向 `gif_meow_anim`
5. 输出 Flash 用量统计

---

### 5. 当前动画数据（meow_anim.h）

| 属性 | 值 |
|------|-----|
| 来源文件 | meow.gif |
| 帧数 | 32 帧 |
| 每帧大小 | 1024 字节 |
| 总 Flash 占用 | 32,768 字节 (~32 KB) |
| 帧延迟范围 | 80–160 ms |
| 循环模式 | 无限循环 |
| 动画名称 | "Meow!" |

动画数据由 `auto_gif.py` 自动生成，帧数据以 `static const uint8_t meow_anim_frame_N[1024]` 的形式存储在 Flash 中。

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

# 方法二：手动转换任意 GIF
python tools/gif_to_c.py your_animation.gif anim_name
# 然后将生成的 anim_name.h 复制到 Core/Inc/
# 并在 main.c 中添加 #include "anim_name.h"
# 调用 GifAnim_Play(&gif_anim_name, 1);
```

### 烧录到设备

通过 ST-LINK 接口（PA14=SWCLK, PA13=SWDIO）使用 STM32CubeProgrammer 或 OpenOCD 烧录生成的 ELF/HEX 文件。

---

## Flash 与 RAM 使用分析

| 资源 | 用量 | 备注 |
|------|------|------|
| Flash（位图帧） | ~32 KB | 32 帧 × 1024 字节 |
| Flash（代码） | ~12–15 KB | HAL 驱动 + OLED 驱动 + 动画引擎 + 字库 |
| Flash（总计） | ~44–47 KB | 占 64KB 的 ~73%，有余量 |
| RAM（帧缓冲） | 1 KB | 静态分配的 oled_buffer[8][128] |
| RAM（栈/堆） | ~2–4 KB | 取决于 HAL 和编译器设置 |
| RAM（总计） | ~3–5 KB | 占 20KB 的 ~25%，充足 |

---

## 关键技术决策

1. **列优先帧缓冲布局**：与 CH1116 I2C 水平寻址模式中的硬件自动增量顺序一致，使得 `OLED_ShowFrame()` 可以通过一次连续的 I2C 传输将整帧发送出去，无需额外变换。

2. **双模式动画引擎**：程序化模式适合交互式 UI 和小动画（无 Flash 开销），位图模式适合播放预先制作的 GIF 素材。两种模式共享统一的帧播放逻辑。

3. **HSE 回退机制**：`SystemClock_Config()` 在外部晶振启动失败时自动切换到内部 HSI，确保固件在任何硬件条件下都能运行（尽管可能降频）。

4. **auto_gif.py 重写 main.c**：脚本以完全重写的方式更新 main.c，而非编辑已有文件，这样确保输出一致性。原始模板内嵌在脚本中，可根据需要修改。

5. **Python 工具而非编译时生成**：选择运行时运行 Python 脚本而非 CMake 的 `add_custom_command`，因为工具链环境不一定有 Python/Pillow，且动画数据很少变动，预生成更简单可靠。

---

## 未来扩展方向

- 支持通过按键（PB12/PB13）切换多个动画
- 利用非阻塞播放 API 实现动画与传感器交互（如 AHT20 温湿度显示叠加）
- 利用 WS2812C 灯带（PB4）同步播放氛围灯效
- 通过蓝牙串口（PB10/PB11）无线更新 GIF 数据
- 使用外部 SPI Flash 存储更多/更大的动画帧，突破 64KB 内置 Flash 限制
