# Signal — 简易数字信号源（中断方式）

## 概述

本程序基于 **MicroBlaze 软核处理器 + Nexys4 DDR 开发板**，通过 AXI GPIO + AXI Timer + AXI UART Lite + AXI INTC 实现一个**简易数字信号源**。所有功能采用**普通中断**（非快速中断）方式实现。

### 核心功能

| 功能 | 实现方式 | 说明 |
|------|---------|------|
| **DA 转换** | GPIO_1 8位并行输出 | 连接外部 R-2R 电阻网络 DAC 或 Pmod DA 模块 |
| **按键控制波形切换** | BTNC 中断 | 循环切换：正弦波→三角波→锯齿波→方波→任意波形 |
| **按键调节幅度** | BTNU/BTND 中断 | 每按一次增减 16 级（范围 16~255） |
| **按键调节频率** | BTNR/BTNL 中断 | 每按一次改变相位步进系数（范围 1~50） |
| **键盘输入任意波形** | UART 中断 | 8 字节对应 1 个周期的 8 个采样值 (0x00~0xFF) |
| **LED 状态指示** | GPIO_2 通道1 | LED[2:0]=波形类型，LED[7]=幅度>50%指示 |

---

## Vivado 硬件设计要点（Block Design 配置）

硬件平台应基于 MicroBlaze 最小系统构建，至少需包含以下 AXI 外设：

### 1. AXI GPIO 配置

| 组件 | 配置项 | 必须值 | 说明 |
|------|--------|--------|------|
| **AXI GPIO 0** | GPIO Data Width (Channel 1) | 5 | 5位独立按键输入（BTNU/BTND/BTNL/BTNR/BTNC） |
| | Enable Dual Channel | ✘ 不勾选 | 仅需通道1 |
| | Enable Interrupt | ✔ 勾选 | 按键触发中断 |
| **AXI GPIO 1** | GPIO Data Width (Channel 1) | 8 | 8位DAC并行数据输出 |
| | Enable Dual Channel | ✘ 不勾选 | 仅需通道1 |
| | Enable Interrupt | ✘ 不勾选 | DAC输出无需中断 |
| **AXI GPIO 2** | GPIO Data Width (Channel 1) | 8 | 8位LED状态指示输出 |
| | GPIO Data Width (Channel 2) | 16 | 16位拨码开关输入（预留） |
| | Enable Dual Channel | ✔ 勾选 | 需要两个通道 |
| | Enable Interrupt | ✘ 不勾选 | LED显示无需中断 |

> ⚠ **注意事项**：
> - GPIO_0 的 Channel1 数据宽度设为 5 位，但 AXI GPIO IP 会向上取整到 8 位。代码中读取按键值时需用 `& 0x1f` 屏蔽高 3 位。
> - GPIO_1 连接外部 DAC 模块，输出为 8 位并行数据。如需更高精度可使用 10 位或 12 位 DAC，但代码需相应调整。
> - GPIO_2 Channel2（16位拨码开关）为预留功能，当前代码未使用，可根据需要扩展。

### 2. AXI Timer 配置

| 配置项 | 必须值 | 说明 |
|--------|--------|------|
| Timer Mode | 能产生中断的模式 | 需定时器溢出时触发中断 |
| Load Register Width | 32 位 | 初值约 10000，需要至少 14 位 |
| Generate Interrupt | ✔ 勾选 | 定时器中断触发 DAC 采样 |
| Count Direction 可配置 | ✔ | 代码通过 `XTC_CSR_DOWN_COUNT_MASK` 设为减计数 |

> **定时器周期计算**（系统时钟 = 100 MHz，周期 = 10 ns）：
> ```
> TIMER_RESET_VALUE = 10000 - 2 = 9998
> DAC采样周期 = (9998 + 2) × 10 ns = 100 μs
> DAC采样率 = 1 / 100 μs = 10 kHz
> ```
>
> 输出波形频率与频率系数 `frequency_step`（1~50）的关系：
> ```
> f_out = DAC采样率 × frequency_step / 256
>
> frequency_step=1  → f_out ≈ 39 Hz   （正弦波）
> frequency_step=10 → f_out ≈ 390 Hz
> frequency_step=50 → f_out ≈ 1.95 kHz
> ```
>
> 调节 `TIMER_RESET_VALUE` 可改变 DAC 采样率（初值越小，采样率越高）。

### 3. AXI UART Lite 配置

| 配置项 | 必须值 | 说明 |
|--------|--------|------|
| Baud Rate | **115200** | 串口通信波特率 |
| Data Bits | 8 | 8位数据 |
| Parity | None | 无校验 |
| Enable Interrupt | ✔ 勾选 | UART接收触发中断 |
| AXI CLK Frequency | 与系统时钟一致 | 100 MHz（建议） |

> ⚠ **UART Lite 中断使能配置**：
> UART Lite 的中断使能位于控制寄存器（偏移 0xC）的 bit 4（0x10）。
> 虽然 `XV_EVENT_INTERRUPT_HANDLER` 宏在 UART Lite 中断中无法生效，但可以通过 `xil_io.h` 直接写寄存器的方式使能（如本代码所示）。

### 4. AXI INTC 配置

| 配置项 | 必须值 | 说明 |
|--------|--------|------|
| Number of Interrupt Inputs | ≥ 3 | 需接收 GPIO_0、UART、Timer 共 3 个中断源 |
| Enable Fast Interrupts | **✘ 不勾选** | 使用普通中断模式 |

**中断连接关系（必须严格按此顺序）：**

```
AXI GPIO 0   .ip2intc_irpt   →  AXI INTC 0 .Intr[0]   (掩码 0x1)  — 按键中断
AXI UART Lite .Interrupt     →  AXI INTC 0 .Intr[1]   (掩码 0x2)  — 串口接收中断
AXI Timer 0  .Interrupt      →  AXI INTC 0 .Intr[2]   (掩码 0x4)  — DAC采样定时器中断
AXI INTC 0   .Intr           →  MicroBlaze .INTERRUPT
```

> ⚠ **中断连接顺序必须与代码中掩码定义一致**，否则中断无法正确响应。

### 5. MicroBlaze 配置

| 配置项 | 推荐值 | 说明 |
|--------|--------|------|
| Enable Interrupt Controller | ✔ 使能 | 使能外部中断引脚 INTERRUPT |
| Enable AXI Peripherals | ✔ 使能 | 通过 AXI Interconnect 连接所有 AXI 外设 |
| Clock Frequency | 100 MHz | 建议100MHz，影响定时器周期计算 |

### 6. 时钟与复位

| 信号 | 连接 | 说明 |
|------|------|------|
| 系统时钟 | 连接到所有 AXI 外设和 MicroBlaze | Nexys4 DDR 板载 100 MHz 晶振 |
| 复位信号 | 连接到 Processor System Reset 模块 | 低电平复位 |

### 7. 完整 Block Design 外设清单

| 序号 | IP 核 | 用途 | 中断？ |
|------|-------|------|--------|
| 1 | MicroBlaze | 软核处理器 | — |
| 2 | AXI Interconnect | AXI 总线互联 | — |
| 3 | Processor System Reset | 系统复位模块 | — |
| 4 | Clocking Wizard / MMCM | 时钟管理 | — |
| 5 | AXI GPIO 0 | 5位按键输入 | ✔ |
| 6 | AXI GPIO 1 | 8位DAC并行输出 | ✘ |
| 7 | AXI GPIO 2 | 8位LED + 16位开关 | ✘ |
| 8 | AXI Timer 0 | DAC采样定时器 | ✔ |
| 9 | AXI UART Lite | 串口通信（任意波形输入） | ✔ |
| 10 | AXI INTC 0 | 中断控制器 | — |
| 11 | MDM (MicroBlaze Debug Module) | JTAG调试 | — |

---

## 硬件资源映射

| 外设 | 宏定义 | 功能 | 数据方向 |
|------|--------|------|---------|
| AXI GPIO 0 | `XPAR_AXI_GPIO_0_BASEADDR` | Channel1: 5位按键 | 输入 |
| AXI GPIO 1 | `XPAR_AXI_GPIO_1_BASEADDR` | Channel1: 8位DAC数据 | 输出 |
| AXI GPIO 2 | `XPAR_AXI_GPIO_2_BASEADDR` | Channel1: 8位LED<br>Channel2: 16位拨码开关 | 输出<br>输入 |
| AXI Timer 0 | `XPAR_AXI_TIMER_0_BASEADDR` | DAC采样定时器 | — |
| AXI UART Lite | `XPAR_AXI_UARTLITE_0_BASEADDR` | 串口接收 | — |
| AXI INTC 0 | `XPAR_AXI_INTC_0_BASEADDR` | 中断管理 | — |

---

## 所需外接设备

### 1. DA 转换器（DAC）

本程序输出 **8位并行数字信号**（0x00~0xFF），需外接 DAC 模块将数字量转换为模拟电压。

**推荐方案**：

| 方案 | 说明 | 连接方式 | 注意 |
|------|------|---------|------|
| **R-2R 电阻网络**（推荐） | 使用 8 个电阻搭建 R-2R 梯形网络，成本极低 | 8 根数据线连接 GPIO_1[0:7] 接 Pmod JA 或 JB 口 | 输出电压范围 0~3.3V，输出阻抗较高，建议后接运放跟随器 |
| **Pmod DA3** | Digilent 官方 Pmod DAC 模块（SPI接口） | SPI 通信（SCK=MOSI=MISO=CS），需修改代码使用 SPI 协议 | 12位精度，但需额外编写 SPI 驱动代码 |
| **MCP4725** | I²C 接口 12 位 DAC 模块 | I²C 总线（SCL=SDA），需修改代码使用 I²C 协议 | 需额外编写 I²C 驱动代码 |

> ✅ **推荐使用 R-2R 电阻网络**，与当前代码（8位并行输出）无缝匹配。

**R-2R 电阻网络接线示意**（Nexys4 DDR Pmod JA 口）：

```
GPIO_1[0] → R1 → Vout     （最高位 MSB）
GPIO_1[1] → R2 → Vout
GPIO_1[2] → R3 → Vout
GPIO_1[3] → R4 → Vout
GPIO_1[4] → R5 → Vout
GPIO_1[5] → R6 → Vout
GPIO_1[6] → R7 → Vout
GPIO_1[7] → R8 → Vout     （最低位 LSB）
                    Vout → 示波器探头输入
```

- R 值建议：10kΩ（R 电阻） + 20kΩ（2R 电阻），精度 1%
- Vout 连接到示波器观察输出波形
- 如需驱动低阻抗负载，在 Vout 后加运放电压跟随器（如 LM358 或 MCP6001）

### 2. 串口通信设备

用于键盘输入任意波形数据：

| 设备 | 连接方式 | 说明 |
|------|---------|------|
| **USB-UART 转换器** | Nexys4 DDR 的 USB-UART 口（J13）连接到 PC | 板载 CP210x USB-UART 桥接芯片 |
| **串口终端软件** | PC 端运行 | 如 PuTTY、Tera Term、Minicom、Vitis Serial Terminal |

串口参数配置：
- 波特率：**115200**
- 数据位：8
- 停止位：1
- 校验位：None
- 流控：None

### 3. 输出监测设备

- **示波器**：观察 DAC 输出的模拟波形
- **逻辑分析仪**（可选）：观察 GPIO_1 并行数据输出时序

---

## 代码设计原理

### 1. 整体架构

```
上电 → main()
        ├── 打印启动信息
        ├── 预计算 4 种标准波形查找表（正弦/三角/锯齿/方波，各256点）
        ├── Initialization() — 初始化所有外设 + 使能中断
        │    ├── GPIO 方向配置（按键=输入，DAC/LED=输出）
        │    ├── GPIO_0 按键中断使能
        │    ├── UART Lite 接收中断使能
        │    ├── Timer_0 初始化（DAC采样定时器）
        │    ├── INTC 初始化+中断使能
        │    └── microblaze_enable_interrupts()
        └── while(1) — 等待中断
```

### 2. 中断服务程序（ISR）架构

采用**普通中断模式**（`__attribute__((interrupt_handler))`），与参考项目 TASK_INT_0 的架构一致：

```
My_ISR() — 主中断分发函数
  ├── 读取 INTC 中断状态寄存器 (ISR)
  ├── if (BUTTON_INTC_MASK)  → button_handle()   // 按键处理
  ├── if (UART_INTC_MASK)    → uart_handle()     // 串口接收处理
  ├── if (TIMER_INTC_MASK)   → timer_handle()    // DAC采样输出
  └── 写 IAR 清除 INTC 中断标志
```

> ⚠ **三个判断均使用独立 `if` 而非 `else if`**：多个中断可能同时触发，需要全部处理。

### 3. 波形生成方式

#### 3.1 标准波形（正弦/三角/锯齿/方波）

- 在 `main()` 初始化阶段，调用 `generate_*_table()` 函数**预计算 256 点的波形查找表（LUT）**
- 每个查找表存储一个完整周期的波形数据（0~255，对应 8 位 DAC 满量程）
- 运行时，定时器 ISR 直接从查找表中按索引取值，无需实时计算
- **优点**：CPU 占用极低，适合嵌入式实时系统

#### 3.2 任意波形

- 用户通过串口发送 **8 字节数据**（0x00~0xFF），对应一个周期的 8 个采样点
- 定时器 ISR 中将 8 个点**线性保持**到 256 点：每 32 个采样点对应 1 个用户数据点
- 收满 8 字节后自动重置计数器，可连续发送新数据覆盖旧波形

#### 3.3 正弦波查找表生成

```c
for (i = 0; i < 256; i++)
{
    val = sin(2π × i / 256);          // val ∈ [-1, 1]
    table[i] = (val + 1.0) × 127.5;   // 映射到 [0, 255]
}
```

### 4. 幅度控制原理

```
DAC实际输出 = 查找表值 × amplitude / MAX_AMPLITUDE
            = 查找表值 × amplitude / 255
```

- `amplitude` 范围 16~255，步进 16
- 查找表值范围 0~255
- 幅度控制通过软件缩放实现——相当于对波形做**乘法衰减**

> 📐 **例如**：`amplitude = 128` 时，波形幅度减半（Vpp ≈ 1.65V）

### 5. 频率控制原理

频率通过改变**相位步进（`frequency_step`）**实现：

```
DAC采样率 = 10 kHz（由 TIMER_RESET_VALUE = 9998 决定）
相位步进 = frequency_step（1 ~ 50）
输出频率 = DAC采样率 × frequency_step / 256

frequency_step = 1  → 一次输出完整256点 → f_out ≈ 39 Hz
frequency_step = 50 → 一次跳50个点     → f_out ≈ 1.95 kHz
```

> ⚠ 频率控制是"粗调节"，输出频率为量化值。需要精确频率时，应调整 `TIMER_RESET_VALUE` 来改变 DAC 采样率。

### 6. 按键处理逻辑

为了实现可靠的人机交互，按键处理采用了防抖和自动重复机制：

```
按键按下 → 触发中断 → button_handle()
  ├── 读取按键值（& 0x1f 屏蔽高位）
  ├── if (button == 0) → 释放中断，不处理，清除标志，返回
  ├── 逐一检查5个按键位（独立if，支持组合键）
  │    ├── BTNC → change_waveform()
  │    ├── BTNU → update_amplitude(+16)
  │    ├── BTND → update_amplitude(-16)
  │    ├── BTNR → update_frequency(+1)
  │    └── BTNL → update_frequency(-1)
  └── 清除 GPIO 中断标志
```

> 📐 **组合键支持**：由于各按键判断使用独立 `if` 语句，可同时按下多个键（如 BTNR + BTNU = 同时增大频率和幅度）。

---

## 按键功能分配（Nexys4 DDR）

| 按键 | Nexys4 标识 | 功能 | UART 提示信息 |
|------|------------|------|-------------|
| 中键 | **BTNC** | 切换波形类型（正弦→三角→锯齿→方波→任意→正弦...） | `Waveform: ****` |
| 上键 | **BTNU** | 增大幅度（+16） | `Amplitude: *** / 255` |
| 下键 | **BTND** | 减小幅度（-16） | `Amplitude: *** / 255` |
| 右键 | **BTNR** | 增大频率（+1） | `Frequency step: **` |
| 左键 | **BTNL** | 减小频率（-1） | `Frequency step: **` |

---

## 任意波形输入格式（串口）

### 操作步骤

1. 按 **BTNC** 键，切换到 **ARBITRARY** 模式
2. 通过串口终端软件连接 Nexys4 DDR 的 USB-UART 口（115200-8-N-1）
3. 发送 **8 字节** 数据（每个字节 0x00~0xFF），对应一个周期的 8 个采样点
4. Vitis Serial Terminal / PuTTY 等软件可直接发送原始二进制数据

### 数据格式示例

| 说明 | 字节序列（十六进制） |
|------|---------------------|
| 上升阶梯波 | `00 20 40 60 80 A0 C0 E0` |
| 下降阶梯波 | `E0 C0 A0 80 60 40 20 00` |
| 三角波 | `00 40 80 C0 80 40 00 00` |
| 脉冲波 | `FF 00 00 00 00 00 00 00` |
| 直流（半幅） | `80 80 80 80 80 80 80 80` |

> 💡 **提示**：在串口终端中，可以使用十六进制发送模式。PuTTY 不支持直接发送十六进制，可配合 Python/串口助手等工具发送原始字节。
>
> 使用 Python 发送示例：
> ```python
> import serial
> ser = serial.Serial('COM3', 115200)  # 根据实际串口号修改
> data = bytes([0x00, 0x20, 0x40, 0x60, 0x80, 0xA0, 0xC0, 0xE0])
> ser.write(data)
> ser.close()
> ```

### UART 中断接收流程

```
UART接收中断 → uart_handle()
  ├── 从 RX FIFO 读取1字节
  ├── if (当前是任意波形模式)
  │    ├── 存入 arbitrary_table[count]
  │    ├── UART打印: "Arbitrary[count] = 0x** (***)"
  │    ├── count++
  │    └── if (count == 8)
  │         ├── 打印: ">> Arbitrary waveform loaded! (8/8 points)"
  │         └── count = 0（准备接收下一组数据）
  └── else
       └── 提示切换到 ARBITRARY 模式
```

---

## 注意事项

### 1. 中断标志清除顺序

- **GPIO 中断**：在各 GPIO ISR 内部清除（写 GPIO ISR 寄存器）
- **Timer 中断**：在 `timer_handle()` 末尾清除（写 Timer TCSR 的 INT_OCCURED 位）
- **UART 中断**：**读取 RX FIFO 即自动清除**中断标志，无需额外操作
- **INTC 中断**：在 `My_ISR()` 末尾统一清除（写 INTC IAR 寄存器）

> ⚠ 必须在处理完所有中断源之后再清除 INTC 标志，否则可能导致中断丢失。

### 2. 按键消抖问题

本代码未在软件层面实现按键消抖。Nexys4 DDR 的 5 个独立按键为硬件按钮，按下和释放时均会因机械抖动产生多次中断。实际使用中，建议：
- 在 Vivado 中为按键输入添加 **GPIO Debounce** 电路（Block Design 中配置）
- 或在软件层面添加去抖延迟（但会导致 ISR 执行时间过长）

当前代码通过判断 `button == 0` 过滤释放事件来减少抖动影响。

### 3. DAC 输出精度

- 当前输出为 **8 位**（256 级），精度约 12.9 mV/LSB（3.3V 参考电压下）
- 如需更高精度（如 10 位或 12 位），需：
  - 将 GPIO_1 的数据宽度增加
  - 修改 `MAX_AMPLITUDE` 宏（如 10 位 → 1023）
  - 使用支持对应精度的 DAC 芯片

### 4. 定时器精度

- `TIMER_RESET_VALUE = 10000 - 2`，定时器实际计数值为 9999 个时钟周期
- 系统时钟 100 MHz 时，DAC 采样率 ≈ 10 kHz
- 若系统时钟频率不同，需重新计算：
  - `TIMER_RESET_VALUE = 目标周期(μs) × 时钟频率(MHz) - 2`

### 5. 任意波形的数据保持

- 任意波形 8 个数据点存储在全局数组中，切换波形类型后数据**不会被清除**
- 再次切换回 ARBITRARY 模式时，可继续输出之前设置的波形
- 如果想完全清除任意波形数据，需要重新写入 8 个字节

### 6. UART 波特率匹配

- **必须确保** Vivado 中 AXI UART Lite 的波特率配置为 **115200**
- **必须确保** 串口终端软件的波特率也设置为 **115200**
- 波特率不匹配将导致接收乱码，任意波形数据将不正确

### 7. 内存占用

- 4 个波形查找表各 256 字节，共 **1024 字节**
- 任意波形表 **8 字节**
- 总计约 **1 KB** 波形数据存储
- MicroBlaze 的 BRAM 容量通常为 8 KB~128 KB，满足需求

### 8. 与参考项目的区别

| 对比项 | TASK_INT_0 / TASK_FAST_INT | Signal（本项目） |
|--------|---------------------------|-----------------|
| 中断模式 | 普通中断 / 快速中断 | 普通中断 |
| GPIO_0 用途 | 拨码开关输入（16位） | 按键输入（5位） |
| GPIO_1 用途 | 数码管位选+段码 | DAC数据输出（8位） |
| 定时器用途 | 数码管扫描（~10μs周期） | DAC采样输出（~100μs周期） |
| UART 中断 | 无 | 有（接收任意波形数据） |
| GPIO 中断数量 | 2个（开关+按键） | 1个（按键） |

---

## 程序执行流程示例

```
上电 → 打印启动信息 → 初始化 → while(1)

定时器每100μs触发一次：
  → timer_handle()
  → 计算当前波形/幅度/相位的DAC值
  → 输出到GPIO_1（DAC）
  → 更新LED状态
  → 相位前进

用户按BTNC：
  → button_handle()
  → change_waveform()
  → 切换到下一种波形
  → UART打印当前波形名称

用户按BTNU（在当前是ARBITRARY模式下）：
  → button_handle()
  → update_amplitude(+16)
  → UART打印 "Amplitude: 32 / 255"

用户串口发送 8 字节 [00 20 40 60 80 A0 C0 E0]：
  → uart_handle() 被触发8次
  → 每次打印 "Arbitrary[*] = 0x** (***)"
  → 收满8次后打印 "Arbitrary waveform loaded!"
  → 定时器ISR开始输出对应的阶梯波
```

---

## 扩展可能性

以下功能可在当前代码基础上扩展实现：

1. **频率精确调节**：根据按键动态修改 `TIMER_RESET_VALUE`，实现精确频率控制
2. **波形幅度调制（AM）**：通过另一个低频定时器动态改变 `amplitude`
3. **波形频率调制（FM）**：通过另一个低频定时器动态改变 `frequency_step`
4. **双通道输出**：启用 GPIO_1 Channel2 输出第二路波形（需修改硬件配置）
5. **拨码开关预设**：利用 GPIO_2 Channel2 的 16 位拨码开关读取预设参数（频率/幅度）
6. **SD 卡存储波形**：扩展 SD 卡接口，将任意波形数据保存到文件