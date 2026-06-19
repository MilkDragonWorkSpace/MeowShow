# STM32_KIT 学习板原理图简报

## 板卡概览

- 板卡名称：`STM32_KIT 学习板`
- 主要用途：基于 STM32F103 的综合外设学习板，覆盖 GPIO、ADC、I2C、UART、USB、PWM、电机、继电器、传感器、显示和灯带。
- 主控/核心 IC：`STM32F103C8T6`
- 原理图来源：`学习板电路原理图-271de4846db7e82d32af6195cba1642e.pdf`
- 抽取质量：高，`5/5` 页有可抽取文本；已渲染并视觉复核 5 页。

## 电源架构 

| 电源轨/输入 | 证据 | 相关器件 | 用途 | 注意事项/风险 | 置信度 |
|---|---|---|---|---|---|
| `VBUS -> 5V` | p.2: "VBUS ... 5V"; 视觉页 p.2 标注 Anti-backflow protection | `Q1 AO3401A`, `U3 MMDT5401`, `R1 10K`, `R2 47K` | USB/Type-C 输入经防倒灌到 5V | 双电源输入防倒灌；需注意 USB 供电能力 | 高 |
| `VSWD -> 5V` | p.2: "VBUS VSWD 5V"; 视觉页 p.2 第二路防倒灌 | `Q2 AO3401A`, `U4 MMDT5401`, `R3 10K`, `R4 47K` | SWD/外部调试供电输入经防倒灌到 5V | 与 VBUS 共同接入 5V，依赖防倒灌电路 | 高 |
| `5V -> 3.3V` | p.2: "U1 AP2112K-3.3TRG1", "3.3V Power supply" | `U1 AP2112K-3.3TRG1`, `C1 1uF`, `C2 1uF` | 主控、传感器、逻辑供电 | 线性稳压，热耗散取决于 5V 负载 | 高 |
| `3.3V 指示灯` | p.2: "R25 5.1K", "L12 红灯 0603" | `R25`, `L12` | 3.3V 电源指示 | 仅指示电源存在 | 高 |
| `CR1220 -> VBAT` | p.3: "U6 CR1220"; 视觉页 p.3 连接到 `VBAT` | `U6 CR1220` | RTC/备份域电池 | 文本未显示电池保护细节 | 中 |

## 功能外设

| 外设 | 器件/模块证据 | MCU 引脚或总线 | 供电 | 功能 | 固件启示 | 置信度 |
|---|---|---|---|---|---|---|
| 按键 `KEY 1` | p.4: "KEY 1 PB12"; 视觉页 p.4 有 `R11 10K` 上拉到 3.3V | `PB12` | `3.3V` | 普通按键输入 | 可按低电平触发处理 | 高 |
| 按键 `KEY 2` | p.4: "KEY 2 PB13"; p.4: "K2 has no external pullup" | `PB13` | `3.3V` | 普通按键输入 | 必须启用内部上拉 | 高 |
| NTC 温度 | p.4: "NTC", "PA4"; 视觉页 p.4 标注 `β=3950`, `R1=10K`, `T1=25℃` | `PA4` ADC | `3.3V` | 温度模拟采集 | 配置 ADC，按 10K NTC 参数换算 | 高 |
| 电位器 | p.4: "VOL ... PA5"; 视觉页 p.4 可变电阻接 3.3V/GND | `PA5` ADC | `3.3V` | 电位器模拟量采集 | 配置 ADC | 高 |
| TCRT5000 循迹模块 | p.4: "TCRT5000 ... PB14" | `PB14` | `3.3V` | 红外循迹/反射检测 | GPIO 输入 | 高 |
| HC-SR04 超声波 | p.4: "HC-SR04 ... TRIG ECHO PA10 PA11"; 视觉页 p.4 确认 `PA11 TRIG`, `PA10 ECHO` | `PA11=TRIG`, `PA10=ECHO` | `3.3V` | 超声波测距 | TRIG 输出脉冲，ECHO 输入捕获；注意模块电平兼容性 | 高 |
| 继电器 | p.4: "SRD-05VDC-SL-C", "PB5", "5V"; p.4: "More than 36V is DANGEROUS" | `PB5` | `5V` | 继电器开关输出 | 驱动 `Q3 SS8050`；继电器触点高压危险 | 高 |
| 舵机 | p.4: "SERVO ... PB8", "5V" | `PB8` | `5V` | 舵机 PWM 控制 | `PB8` 输出 PWM | 高 |
| DRV8833 电机驱动 | p.4: "DRV8833", "PA0", "PA1", "5V" | `PA0`, `PA1` | `5V` | 直流电机驱动接口 | 两路方向/PWM 控制 | 高 |
| EC11 旋转编码器 | p.4: "EC11"; 视觉页 p.4 确认 `A=PA8`, `B=PA9`, 按键 `PB15` | `PA8`, `PA9`, `PB15` | `3.3V` | 旋钮 A/B 相和按键 | `PB15` 无外部上拉；`PA8/PA9` 标注 5V tolerant | 高 |
| RGB LED | p.4: "RGB LED", "MHP5050RGBDT"; 视觉页 p.4 颜色标注 | 蓝=`PA6`, 绿=`PA7`, 红=`PB0` | 未知/由电路供电 | 三色 LED | 低电平或限流方式需结合 LED 方向验证；可 PWM 调光 | 高 |
| AHT20 温湿度 | p.4: "AHT20", "I2C 地址 0x70"; 视觉页 p.4 `SCL=PB6`, `SDA=PB7` | `PB6=SCL`, `PB7=SDA`, I2C `0x70` | `3.3V` | 温湿度传感器 | I2C 访问地址 `0x70` | 高 |
| 蓝牙接口 | p.4: "BLUETOOTH", "PB10 U3_TX", "PB11 U3_RX" | `PB10=U3_TX`, `PB11=U3_RX` | `5V` | 外接蓝牙串口模块 | 使用 USART3；注意模块电平 | 高 |
| OLED 显示屏 | p.5: "OLED SCREEN", "CH1116", "I2C address 0x7A"; 视觉页 p.5 `SCL=PB6`, `SDA=PB7` | `PB6=SCL`, `PB7=SDA`, I2C `0x7A` | `3.3V` | OLED 显示 | 驱动芯片 `CH1116`，I2C 地址 `0x7A` | 高 |
| 无源蜂鸣器 | p.5: "Passive Buzzer", "PB9"; p.5: "must be driven by PWM" | `PB9` | `3.3V` | 蜂鸣器发声 | 必须 PWM 驱动，直接给高电平不响 | 高 |
| WS2812C 灯带/阵列 | p.5: "WS2812C", "PB4"; p.5: "PB4 is 5V tolerance PIN", "Set to open-drain output mode" | `PB4` | `5V` | 可寻址 RGB LED 阵列 | `PB4` 配置开漏输出，满足 5V 侧数据驱动约束 | 高 |

## 接口与调试

| 接口 | 证据 | 信号/引脚 | 用途 | 注意事项 | 置信度 |
|---|---|---|---|---|---|
| ST-LINK | p.3: "ST-LINK Connector"; 视觉页 p.3 标注 `PA14 CLK`, `PA13 DIO`, `VSWD`, `GND` | `PA14=SWCLK`, `PA13=SWDIO`, `VSWD`, `GND` | 下载/调试 | 接口也可作为一路供电输入 | 高 |
| BOOT0/BOOT1 跳线 | p.3: "BOOT0 BOOT1 Selector"; 视觉页 p.3 `BOOT0` 带 `R5 10K` 下拉，`BOOT1` 关联 `PB2` | `BOOT0`, `PB2/BOOT1` | 启动模式选择 | 跳线改变启动源 | 高 |
| 自定义 GPIO 口 | p.3: "USER-DEFINED GPIO"; 视觉页 p.3 Header 6x2 | `PB2`, `PA12`, `PA15`, `PB3`, `PB1`, `PC13`, `PB6/SDA`, `PB7/SCL`, `GND` | 扩展 GPIO/I2C | 部分引脚与板载外设复用 | 高 |
| 电源输出口 | p.3: "POWER OUTPUT"; 视觉页 p.3 Header 6x2 | `3.3V`, `5V`, `GND` | 外部供电输出 | 受板上供电能力限制 | 高 |
| Type-C USB | p.4: "TYPE-C"; 视觉页 p.4 `VBUS`, `DP`, `DN`, `CC1/CC2 5.1K` | `VBUS`, `DP`, `DN`, `CC1`, `CC2` | USB 连接与 5V 输入 | 有 ESD/保护二极管，未见 USB-C 高级协商 | 高 |
| CH343P USB 转串口 | p.4: "CH343P USART to USB"; 视觉页 p.4 `TXD/RXD` 到 `PA3/PA2` | `PA2`, `PA3`, `DP`, `DN`, `VBUS` | USB 转 USART | 使用主控 USART2 引脚语义更合理：`PA2=TX`, `PA3=RX`，需固件按实际连线核对方向 | 高 |

## 固件启示

- `PB13` 的 `KEY2` 无外部上拉，需启用内部上拉：p.4 "K2 has no external pullup / Enable internal pullup"。
- `PB15` 的 EC11 按键无外部上拉，需启用内部上拉：p.4 "PB15 has no external pullup / Enable internal pullup"。
- `PA8/PA9` 标注为 5V tolerant，可用于 EC11 A/B 相输入：p.4 "PA8 PA9 is 5V tolerance PIN"。
- `PB4` 驱动 WS2812C 时要求开漏输出：p.5 "PB4 is 5V tolerance PIN / Set to open-drain output mode"。
- 无源蜂鸣器必须 PWM 驱动：p.5 "The passive buzzer must be driven by PWM"。
- `AHT20` 和 `OLED` 共享 `PB6/PB7` I2C，总线地址分别为 `0x70` 与 `0x7A`：p.4/p.5。
- `PA4`、`PA5` 分别用于 NTC 与电位器 ADC：p.4。
- 继电器触点涉及高压警告，超过 36V 危险：p.4 "More than 36V is DANGEROUS"。

## 视觉复核结论

- p.3：ST-LINK 接口视觉确认 `PA14=CLK`、`PA13=DIO`；`USER_GPIO` 同时引出 `PB6/SDA`、`PB7/SCL`。
- p.4：HC-SR04 视觉确认 `PA11=TRIG`、`PA10=ECHO`，文本层顺序容易误读。
- p.4：RGB LED 视觉确认蓝=`PA6`、绿=`PA7`、红=`PB0`，对应限流电阻分别为 `R22 820Ω`、`R23 2.2KΩ`、`R24 1.8KΩ`。
- p.4：EC11 视觉确认 `A=PA8`、`B=PA9`、按键=`PB15`，且 `PB15` 无外部上拉。
- p.5：OLED 视觉确认 `PB6=SCL`、`PB7=SDA`，驱动芯片 `CH1116`，I2C 地址 `0x7A`。
- p.5：WS2812C 视觉确认数据输入来自 `PB4`，5V 供电，并注明 `PB4` 要开漏输出。

## 风险 / 不确定点

- PDF 文本层中文有编码错乱，但渲染图可读；中文说明以视觉页为准。
- Type-C/CH343P 与 MCU 的 USB/串口方向需结合 PCB 或固件命名再复核，尤其 `PA2/PA3` 的 TX/RX 方向。
- RGB LED 的有效电平需按 LED 共端连接方向验证；简报只确认颜色到 MCU 引脚映射。
- HC-SR04 常见模块可能输出 5V ECHO；此图供电标注为 3.3V，但实际接入外部模块时仍需确认电平安全。
- 继电器触点侧有高压风险，原理图明确提示不要接入超过 36V 的危险电压。

## 证据索引

- p.1：标题 `STM32_KIT 学习板`；目录列出 Top & Power、MCU、Peripheral-01、Peripheral-02；外设列表含 RGB、KEY、TCRT5000、Relay、DRV8833、Servo、UltraSonic、DHT20、Passive Buzzer、WS2812、Bluetooth、CH343P、Rotary Encoder、OLED、TYPE-C、RTC Battery。
- p.2：`VBUS/VSWD/5V` 防倒灌；`U1 AP2112K-3.3TRG1` 产生 `3.3V`；`R25/L12` 为 3.3V 指示灯。
- p.3：`STM32F103C8T6` MCU；`ST-LINK Connector`；`BOOT0 BOOT1 Selector`；`USER_GPIO`；`POWER_OUTPUT`；`CR1220` RTC 电池。
- p.4：`KEY1 PB12`、`KEY2 PB13`、`TCRT5000 PB14`、`NTC PA4`、`VOL PA5`、`HC-SR04 PA11/PA10`、`Relay PB5`、`Servo PB8`、`DRV8833 PA0/PA1`、`AHT20 PB6/PB7 0x70`、`Bluetooth PB10/PB11`、`CH343P/TYPE-C`、RGB `PA6/PA7/PB0`、EC11 `PA8/PA9/PB15`。
- p.5：`OLED CH1116`，I2C 地址 `0x7A`，`PB6/PB7`；无源蜂鸣器 `PB9` 且必须 PWM；`WS2812C PB4`，5V tolerant 且需开漏输出。
