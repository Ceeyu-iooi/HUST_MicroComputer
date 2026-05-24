# TASK_INT_0 - GPIO中断与定时器扫描数码管程序

## 概述

本程序基于 MicroBlaze 软核处理器，通过 **AXI GPIO**、**AXI Timer** 和 **AXI Interrupt Controller (INTC)** 实现以下功能：

1. **开关（Switch）中断**：读取 16 位拨码开关状态，控制 LED 显示，并通过 UART 打印
2. **按键（Button）中断**：读取 5 个独立按键，将对应的字符段码存入显示缓冲区，实现数码管滚动显示
3. **定时器中断**：以固定间隔扫描 8 位数码管，实现动态显示刷新

---

## Vivado 硬件设计要点（Block Design 配置）

要实现本软件功能，Vivado Block Design 中需要正确配置以下硬件参数：

### 1. AXI GPIO 配置

| 组件 | 配置项 | 必须值 | 说明 |
|------|--------|--------|------|
| **AXI GPIO 0** | GPIO Data Width (Channel 1) | 16 | 16位拨码开关输入 |
| | GPIO Data Width (Channel 2) | 16 | 16位LED输出 |
| | Enable Dual Channel | ✔ 勾选 | 需要两个通道 |
| | Enable Interrupt | ✔ 勾选 | 开关触发中断 |
| **AXI GPIO 1** | GPIO Data Width (Channel 1) | 8 | 8位数码管位选输出 |
| | GPIO Data Width (Channel 2) | 8 | 8位数码管段码输出 |
| | Enable Dual Channel | ✔ 勾选 | 需要两个通道 |
| | Enable Interrupt | ✘ 不勾选 | 无需中断 |
| **AXI GPIO 2** | GPIO Data Width (Channel 1) | 5 | 5位按键输入，低5位有效 |
| | Enable Dual Channel | ✘ 不勾选 | 仅需通道1 |
| | Enable Interrupt | ✔ 勾选 | 按键触发中断 |
| | GPIO is Dual Edge | ✘ 不勾选 | 代码仅按单边沿处理（实际双边沿触发，但软件做了过滤） |

> ⚠ **硬件连接注意事项**：
> - GPIO 2 的数据宽度设为 5 位，但 AXI GPIO IP 会向上取整到 8 位，读取时需用 `&0x1f` 屏蔽高 3 位
> - 拨码开关建议硬件上加上拉电阻（或使能 IP 内部的 Pull-Up），确保浮空时读到确定值
> - LED 输出建议加限流电阻（通常 220Ω~1kΩ），数码管段码输出也需匹配对应驱动电路

### 2. AXI Timer 配置

| 配置项 | 必须值 | 说明 |
|--------|--------|------|
| Timer Mode | **Capture / Compare** 模式 **或** **Generate Interrupt** | 必须能产生中断 |
| Load Register Width | ≥10 位 | 初值 998 需要至少 10 位宽度（可配置为 32 位） |
| Generate Interrupt | ✔ 勾选 | 定时器溢出/到达初值时触发中断 |
| Count Direction | 可配置为 Down Count | 代码通过寄存器位设置减计数，硬件侧方向位需允许软件配置 |

> AXI Timer IP 支持两种模式：定时器模式和 Capture/Compare 模式。只要配置为**能产生中断**即可，代码中通过 `XTC_CSR_DOWN_COUNT_MASK` 设置减计数、`XTC_CSR_AUTO_RELOAD_MASK` 使能自动重载。

### 3. AXI INTC 配置

| 配置项 | 必须值 | 说明 |
|--------|--------|------|
| Number of Interrupt Inputs | ≥3 | 需要接收 GPIO_0、GPIO_2、Timer_0 共 3 个中断源 |
| Level Type | Edge / Level 皆可 | 代码通过中断状态寄存器判断，不依赖具体触发方式 |
| Enable Fast Interrupts | 推荐不勾选 | 使用标准中断模式即可 |

**中断连接关系（必须正确对接）：**

```
AXI GPIO 0 .ip2intc_irpt  →  AXI INTC 0 .Intr[0]   (掩码 0x1)
AXI GPIO 2 .ip2intc_irpt  →  AXI INTC 0 .Intr[1]   (掩码 0x2)
AXI Timer 0 .Interrupt    →  AXI INTC 0 .Intr[2]   (掩码 0x4)
AXI INTC 0 .Intr          →  MicroBlaze .INTERRUPT
```

> ⚠ **中断位号必须与代码掩码一致**：GPIO_0 必须连接 INTC 的第 0 位，GPIO_2 必须连接第 1 位，Timer_0 必须连接第 2 位，否则代码中的宏定义掩码（`XPAR_AXI_GPIO_0_IP2INTC_IRPT_MASK = 0x1` 等）与实际中断位不匹配，中断将无法正确响应。

### 4. MicroBlaze 配置

| 配置项 | 推荐值 | 说明 |
|--------|--------|------|
| Enable Interrupt Controller | ✔ 推荐 | 使能外部中断引脚 `INTERRUPT`，连接 INTC 输出 |
| Enable AXI Peripherals | ✔ 使能 | 通过 AXI Interconnect 连接所有 AXI 外设 |
| Debug Enabled | 可选 | 如需 JTAG 调试可勾选（不影响本程序运行） |

### 5. 时钟与复位

| 信号 | 连接 | 说明 |
|------|------|------|
| 系统时钟 | 连接到所有 AXI 外设和 MicroBlaze | 频率决定定时器实际周期（见"定时器周期计算"章节） |
| 复位信号 | 连接到所有 AXI 外设的 `s_axi_aresetn` | 低电平复位，需与 Processor System Reset 模块连接 |

> 💡 **时钟频率选择建议**：
> - 如果系统时钟为 100 MHz，定时器周期约 10 µs，刷新率 12.5 kHz
> - 如果系统时钟为 50 MHz，定时器周期约 20 µs，刷新率 6.25 kHz（仍高于 flicker 阈值）
> - 如果系统时钟为 10 MHz，定时器周期约 100 µs，刷新率 1.25 kHz（可能轻微闪烁）
> - 如需精确 1 µs 周期，应在 100 MHz 时钟下将 `RESET_VALUE0` 改为 `100 - 2 = 98`

### 6. UART 配置

| 配置项 | 必须值 | 说明 |
|--------|--------|------|
| UART IP | AXI UART Lite / AXI UART 16550 | 任意一种即可 |
| Baud Rate | 115200（推荐） | 串口通信波特率，需与上位机终端软件匹配 |
| 连接方式 | RS232 或 USB-UART | 由开发板决定，需确保物理连接正确 |

> `xil_printf()` 通过 UART 输出调试信息，如果硬件中没有包含 UART IP，`xil_printf()` 将无法输出任何内容，但程序其他功能不受影响。

---

## 硬件资源映射

| 外设 | 地址基址 | 功能 |
|------|---------|------|
| AXI GPIO 0 | `XPAR_AXI_GPIO_0_BASEADDR` | 通道1：16位拨码开关(输入)；通道2：16位LED(输出) |
| AXI GPIO 1 | `XPAR_AXI_GPIO_1_BASEADDR` | 通道1：8位数码管位选(输出)；通道2：8位数码管段码(输出) |
| AXI GPIO 2 | `XPAR_AXI_GPIO_2_BASEADDR` | 通道1：5位按键(输入)，低5位有效 |
| AXI Timer 0 | `XPAR_AXI_TIMER_0_BASEADDR` | 定时器，用于数码管动态扫描 |
| AXI INTC 0 | `XPAR_AXI_INTC_0_BASEADDR` | 中断控制器，管理3个中断源 |
| UART | - | 串口打印调试信息 |

---

## 代码结构

### 1. 宏定义与全局变量

```c
#define RESET_VALUE0  1000 - 2   // 定时器初值 → 实际值为 998
#define RESET_VALUE1  100000 - 2 // 本程序中未使用
#define STEP_PACE 10000000       // 本程序中未使用
```

- `segtable[5]`：5 个字符的段码表：`C(0xc6)` `U(0xc1)` `L(0xc7)` `R(0x88)` `d(0xa1)`
- `segcode[8]`：8 位数码管显示缓冲区，初始全部显示 `0xc0`（数字 0）
- `poscode[8]`：8 位数码管位码，**低电平选中**，从左到右依次为 `0x7F` ~ `0xFE`
- `pos`：当前扫描的数码管位置 0~7
- `mask`：记录按下的按键索引（0~4）

### 2. main() 主函数

```c
main()
  ├── 打印启动信息
  ├── Initialization()  // 初始化所有外设
  └── while(1);         // 等待中断
```

### 3. Initialization() 初始化函数

**① GPIO 方向配置**
- GPIO 0 通道1 → 开关（输入）；通道2 → LED（输出）
- GPIO 1 通道1 → 数码管位选（输出）；通道2 → 数码管段码（输出）
- GPIO 2 通道1 → 按键（输入）

**② GPIO 中断使能**
- 清除 GPIO 中断标志（写 ISR 寄存器）
- 使能 GPIO 中断（写 IER 寄存器）
- 使能 GPIO 全局中断（写 GIE 寄存器）
- GPIO 0 和 GPIO 2 均使能中断（注意代码中 GPIO 2 的变量名注释为"开关中断"，实为按键中断）

**③ 定时器 T0 初始化**
1. 关闭定时器
2. 写入初值 `RESET_VALUE0`（998）
3. 加载初值
4. 清除加载位，同时设置：
   - 中断使能 `XTC_CSR_ENABLE_INT_MASK`
   - 自动重载 `XTC_CSR_AUTO_RELOAD_MASK`
   - 减计数模式 `XTC_CSR_DOWN_COUNT_MASK`
   - 清除中断标志 `XTC_CSR_INT_OCCURED_MASK`
   - 启动定时器 `XTC_CSR_ENABLE_TMR_MASK`

**④ 中断控制器 INTC 初始化**
- 清除 3 个中断源的中断标志（写 IAR）
- 使能 3 个中断源（写 IER）
- 使能 INTC 主中断和硬件中断使能（写 MER）

**⑤ CPU 中断使能**
- 调用 `microblaze_enable_interrupts()` 开启 MicroBlaze 全局中断

---

### 4. My_ISR() 主中断服务程序

中断类型：`__attribute__ ((interrupt_handler))`（MicroBlaze 中断属性）

```
My_ISR()
  ├── 读取 INTC 中断状态寄存器 (ISR)
  ├── if (GPIO_0 中断触发) → switch_handle()
  ├── if (GPIO_2 中断触发) → button_handle()
  ├── if (Timer_0 中断触发) → timer_handle()
  └── 写 IAR 清除 INTC 中断标志
```

> ⚠ **注意**：三个中断判断使用独立的 `if` 而非 `else if`，因为可能同时有多个中断源触发，需全部处理。

---

### 5. switch_handle() 开关中断处理

```c
switch_handle()
  ├── 读取 16 位开关状态
  ├── 输出到 16 位 LED（一一对应）
  ├── if (sw != 0)    // 开关按下时打印
  │   └── UART 打印开关值
  └── 写 ISR 清除 GPIO_0 中断标志
```

> ⚠ **注意**：开关在**按下和释放**时均会触发中断，代码通过判断 `sw != 0` 仅在开关按下时打印，避免释放时重复输出。

---

### 6. button_handle() 按键中断处理

```c
button_handle()
  ├── 读取按键值（仅取低 5 位 &0x1f）
  ├── 忙等待按键松开    // while((读取值 & 0x1f) != 0);
  ├── UART 打印按键编码
  ├── 遍历 5 位，找到被按下的按键索引 → 存入 mask
  ├── 更新显示缓冲区：
  │   ├── 将 segcode[1]~segcode[7] 左移一位 → segcode[0]~segcode[6]
  │   └── segcode[7] = segtable[mask]  // 新字符放在最右侧
  └── 写 ISR 清除 GPIO_2 中断标志
```

> ⚠ **潜在问题**：按键松手检测采用**忙等待（busy-wait）**，即在 ISR 内部执行 `while(读取值 != 0)` 循环。这会阻塞其他中断的处理，若按键长时间未松开，定时器中断和开关中断均无法响应。

---

### 7. timer_handle() 定时器中断处理

```c
timer_handle()
  ├── 消影：
  │   ├── 位选输出 0xFF（所有位不选）
  │   └── 段码输出 0xFF（所有段熄灭）
  ├── 输出当前位的位码 → 数码管位选引脚
  ├── 输出当前位的段码 → 数码管段码引脚
  ├── pos++    // 指向下一位
  └── if (pos == 8) pos = 0  // 循环
```

**动态扫描原理**：
- 定时器以固定频率触发中断（约 10µs，见下方计算）
- 每次中断仅点亮 1 位数码管
- 8 位数码管循环扫描，利用人眼视觉暂留效应呈现稳定显示
- 消影操作（先关闭所有位）可防止位切换时的残影

---

## 中断优先级与中断号

中断掩码宏定义（按位对应 INTC 中断输入）：

| 中断源 | 掩码 | 描述 |
|--------|------|------|
| `XPAR_AXI_GPIO_0_IP2INTC_IRPT_MASK` | 0x1 | 开关中断（INTC 第 0 位） |
| `XPAR_AXI_GPIO_2_IP2INTC_IRPT_MASK` | 0x2 | 按键中断（INTC 第 1 位） |
| `XPAR_AXI_TIMER_0_INTERRUPT_MASK` | 0x4 | 定时器中断（INTC 第 2 位） |

在 Vivado 硬件设计中，这些中断源的连接顺序决定了它们对应的 INTC 中断位。

---

## 定时器周期计算

假设系统时钟频率为 **100 MHz**（周期 10 ns）：

```
RESET_VALUE0 = 1000 - 2 = 998
定时器减计数从 998 → 0，共 999 个时钟周期
定时周期 = 999 × 10 ns = 9.99 µs ≈ 10 µs
```

扫描 8 位数码管的一个完整周期：
```
扫描周期 = 10 µs × 8 = 80 µs
刷新率 = 1 / 80 µs = 12.5 kHz
```

> 12.5 kHz 的刷新率远高于 50~60 Hz 的人眼 flicker 阈值，显示效果稳定无闪烁。

---

## 注意事项

1. **定时器初值偏差**：`RESET_VALUE0 = 1000 - 2` 在 C 语言中实际等于 998（减法运算）。注释中写的是 `0.000001s`（1µs），但实际约为 10µs（100MHz时钟）。原因可能有二：
   - FPGA 系统时钟并非 100MHz（可能为 10MHz 或 1MHz）→ 此时 1µs 成立
   - 注释描述有误

2. **按键松手检测阻塞**：`button_handle()` 中的忙等待会阻塞定时器中断，若按键卡住或人为长按，数码管扫描会暂停。

3. **开关双边沿触发**：开关 GPIO 中断在**上升沿和下降沿**都会触发。`switch_handle()` 用 `if(sw)` 过滤掉释放状态，但如果开关抖动可能导致多次触发（未做硬件/软件消抖）。

4. **消影时序**：`timer_handle()` 中先输出 `0xFF`（熄灭所有段）再输出新的位码/段码，可有效消除位切换时的残影。

5. **显示缓冲区左移**：每次按键按下，显示缓冲区向左移位，新字符插入最右侧，实现类似"滚动输入"的效果。此功能运行时需先通过某个按键输入字符，该字符会从右侧移入显示。

6. **未使用资源**：
   - `RESET_VALUE1`（100000-2）和 `STEP_PACE`（10000000）已定义但未使用
   - GPIO 0 的 GPIO 中断相关初始化代码中出现了变量名与硬件不匹配的问题（GPIO 2 初始化时注释写的是"开关中断"，实为按键）

7. **中断标志清除顺序**：各外设的 ISR 在各自的中断处理函数中清除，INTC 的 IAR 在 `My_ISR()` 末尾统一清除。注意必须在处理完所有中断后再清除 INTC 标志，否则可能导致中断丢失。

---

## 程序执行流程示例

```c
上电 → main() → 初始化所有外设 → while(1) 等待中断
│
├─ [定时器每 10µs 触发] → timer_handle() → 扫描下一位数码管
│
├─ [用户拨动开关] → switch_handle() → LED 同步显示开关状态 + UART 打印
│
└─ [用户按下按键 2] → button_handle()
     ├─ 忙等待松手
     ├─ 打印 "The pushed button's code is 0x04"
     ├─ mask = 2
     ├─ segcode 左移：[A,B,C,D,E,F,G,H] → [B,C,D,E,F,G,H,segtable[2]=L]
     │                    ↑ 旧显示            ↑ 新字符 'L' 出现在最右侧
     └─ 下一次扫描时数码管显示滚动更新