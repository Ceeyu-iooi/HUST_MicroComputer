# TASK_FAST_INT - MicroBlaze 快速中断（Fast Interrupt）模式程序

## 概述

本程序基于 MicroBlaze 软核处理器，通过 **AXI GPIO**、**AXI Timer** 和 **AXI Interrupt Controller (INTC)** 的 **快速中断（Fast Interrupt）模式** 实现以下功能：

1. **开关（Switch）中断**：读取 16 位拨码开关状态，控制 LED 显示，并通过 UART 打印
2. **按键（Button）中断**：读取 5 个独立按键，将对应的字符段码存入显示缓冲区，实现数码管滚动显示
3. **定时器中断**：以固定间隔扫描 8 位数码管，实现动态显示刷新

> ⚠ **与 TASK_INT_0（普通中断模式）的核心区别**：
> - **普通中断模式**：所有中断共用一个主 ISR 入口 `My_ISR()`，由软件读取 INTC 状态寄存器判断中断源再分发
> - **快速中断模式**：硬件通过 IVAR（中断向量地址寄存器）直接跳转到对应 ISR，无需软件分发，响应更快

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

### 2. AXI Timer 配置

| 配置项 | 必须值 | 说明 |
|--------|--------|------|
| Timer Mode | 能产生中断的模式 | 必须能产生中断 |
| Load Register Width | ≥10 位 | 初值 998 需要至少 10 位宽度（可配置为 32 位） |
| Generate Interrupt | ✔ 勾选 | 定时器溢出/到达初值时触发中断 |

### 3. AXI INTC 配置（⭐ 快速中断关键）

| 配置项 | 必须值 | 说明 |
|--------|--------|------|
| Number of Interrupt Inputs | ≥3 | 需要接收 GPIO_0、GPIO_2、Timer_0 共 3 个中断源 |
| **Enable Fast Interrupts** | **✔ 必须勾选** | ⭐ 启用快速中断模式，这是与普通中断的关键区别 |

**中断连接关系（必须正确对接）：**

```
AXI GPIO 0 .ip2intc_irpt  →  AXI INTC 0 .Intr[0]   (掩码 0x1, ID=0)
AXI GPIO 2 .ip2intc_irpt  →  AXI INTC 0 .Intr[1]   (掩码 0x2, ID=1)
AXI Timer 0 .Interrupt    →  AXI INTC 0 .Intr[2]   (掩码 0x4, ID=2)
AXI INTC 0 .Intr          →  MicroBlaze .INTERRUPT
```

> ⚠ **中断位号必须与代码中的 `GPIO_0_INTC_ID`、`GPIO_2_INTC_ID`、`TIMER_0_INTC_ID` 定义一致**，否则 IVAR 写入的向量地址与中断源不匹配，会跳转到错误的 ISR。

### 4. MicroBlaze 配置

| 配置项 | 推荐值 | 说明 |
|--------|--------|------|
| Enable Interrupt Controller | ✔ 推荐 | 使能外部中断引脚 `INTERRUPT` |
| **Enable Fast Interrupts** | **✔ 推荐勾选** | 使能 MicroBlaze 的快速中断支持 |
| Enable AXI Peripherals | ✔ 使能 | 通过 AXI Interconnect 连接所有 AXI 外设 |

### 5. 时钟、复位与 UART

| 信号 | 连接 | 说明 |
|------|------|------|
| 系统时钟 | 连接到所有 AXI 外设和 MicroBlaze | 频率决定定时器实际周期 |
| 复位信号 | 连接到所有 AXI 外设的 `s_axi_aresetn` | 低电平复位 |
| UART IP | AXI UART Lite / AXI UART 16550 | `xil_printf()` 通过 UART 输出调试信息 |

---

## 硬件资源映射

| 外设 | 地址基址 | 功能 |
|------|---------|------|
| AXI GPIO 0 | `XPAR_AXI_GPIO_0_BASEADDR` | 通道1：16位拨码开关(输入)；通道2：16位LED(输出) |
| AXI GPIO 1 | `XPAR_AXI_GPIO_1_BASEADDR` | 通道1：8位数码管位选(输出)；通道2：8位数码管段码(输出) |
| AXI GPIO 2 | `XPAR_AXI_GPIO_2_BASEADDR` | 通道1：5位按键(输入)，低5位有效 |
| AXI Timer 0 | `XPAR_AXI_TIMER_0_BASEADDR` | 定时器，用于数码管动态扫描 |
| AXI INTC 0 | `XPAR_AXI_INTC_0_BASEADDR` | 中断控制器，管理3个中断源（快速中断模式） |
| UART | - | 串口打印调试信息 |

---

## 代码结构

### 1. 宏定义与全局变量

```c
// 定时器初值
#define RESET_VALUE0  1000 - 2       // T0 初值，实际值 = 998
#define RESET_VALUE1  100000 - 2     // 本程序中未使用
#define STEP_PACE 10000000            // 本程序中未使用

// 中断掩码（对应 INTC 的硬件中断输入位）
#define XPAR_AXI_GPIO_0_IP2INTC_IRPT_MASK  0x1  // INTC 第0位
#define XPAR_AXI_GPIO_2_IP2INTC_IRPT_MASK  0x2  // INTC 第1位
#define XPAR_AXI_TIMER_0_INTERRUPT_MASK    0x4  // INTC 第2位

// ⭐ 快速中断模式下每个外设的中断编号（对应 IP2INTC 掩码的 bit 位）
#define GPIO_0_INTC_ID   0   // 0x1 → bit 0
#define GPIO_2_INTC_ID   1   // 0x2 → bit 1
#define TIMER_0_INTC_ID  2   // 0x4 → bit 2
```

- `segtable[5]`：5 个字符的段码表：`C(0xc6)` `U(0xc1)` `L(0xc7)` `R(0x88)` `d(0xa1)`
- `segcode[8]`：8 位数码管显示缓冲区，初始全部显示 `0xc0`（数字 0）→ 与普通中断模式的 `0xff`（全灭）不同
- `poscode[8]`：8 位数码管位码，**低电平选中**，从左到右依次为 `0x7F` ~ `0xFE`
- `pos`：当前扫描的数码管位置 0~7
- `mask`：记录按下的按键索引（0~4）

### 2. main() 主函数

```c
main()
  ├── 打印启动信息 "Running GPIO Test(Fast Interrupt)"
  ├── Initialization()  // 初始化所有外设 + 快速中断配置
  └── while(1);         // 等待中断
```

### 3. 函数声明——快速中断属性

```c
// ⭐ 每个中断服务程序直接声明为快速中断（fast_interrupt），而非 interrupt_handler
void switch_handle(void) __attribute__((fast_interrupt));
void button_handle(void) __attribute__((fast_interrupt));
void timer_handle(void) __attribute__((fast_interrupt));
```

> ⚠ **关键区别**：
> - **普通中断模式**：`void My_ISR() __attribute__ ((interrupt_handler))` — 统一入口 + 软件分发
> - **快速中断模式**：每个 ISR 独立声明 `__attribute__((fast_interrupt))` — 硬件直接跳转，无需主分发函数

### 4. Initialization() 初始化函数

**① GPIO 方向配置**（与普通中断模式相同）
- GPIO 0 通道1 → 开关（输入）；通道2 → LED（输出）
- GPIO 1 通道1 → 数码管位选（输出）；通道2 → 数码管段码（输出）
- GPIO 2 通道1 → 按键（输入）

**② GPIO 中断使能**（与普通中断模式相同）
- 清除 GPIO 中断标志（写 ISR 寄存器）
- 使能 GPIO 中断（写 IER 寄存器）
- 使能 GPIO 全局中断（写 GIE 寄存器）

**③ 定时器 T0 初始化**（与普通中断模式相同）
1. 关闭定时器 → 2. 写入初值 → 3. 加载初值 → 4. 配置中断使能、自动重载、减计数、启动

**④ ⭐ 快速中断配置（核心差异）**

```c
// 步骤1: 配置 IMR（Interrupt Mode Register）将 GPIO_0、GPIO_2、TIMER_0 设为快速中断模式
Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IMR_OFFSET,
          XPAR_AXI_GPIO_0_IP2INTC_IRPT_MASK |
          XPAR_AXI_GPIO_2_IP2INTC_IRPT_MASK |
          XPAR_AXI_TIMER_0_INTERRUPT_MASK);

// 步骤2: 配置 IVAR（Interrupt Vector Address Register）为每个中断设置独立的向量地址
Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IVAR_OFFSET + (GPIO_0_INTC_ID * 4),
          (u32)switch_handle);
Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IVAR_OFFSET + (GPIO_2_INTC_ID * 4),
          (u32)button_handle);
Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IVAR_OFFSET + (TIMER_0_INTC_ID * 4),
          (u32)timer_handle);

// 步骤3: 初始化 INTC IER/IAR（清除并使能中断）
Xil_Out32(..., XIN_IAR_OFFSET, ...);  // 清除中断标志
Xil_Out32(..., XIN_IER_OFFSET, ...);  // 使能中断

// 步骤4: 使能 INTC（Master Enable + Hardware Enable）
Xil_Out32(..., XIN_MER_OFFSET, XIN_INT_MASTER_ENABLE_MASK | XIN_INT_HARDWARE_ENABLE_MASK);
```

**关键寄存器说明：**

| 寄存器 | 偏移宏 | 作用 |
|--------|--------|------|
| **IMR** | `XIN_IMR_OFFSET` | 中断模式寄存器：bit=1 → 对应中断设为快速中断；bit=0 → 普通中断 |
| **IVAR** | `XIN_IVAR_OFFSET` | 中断向量地址寄存器数组：每个快速中断在此存储其 ISR 函数地址 |

**快速中断响应流程：**

```
中断源触发 → INTC 识别中断 → 查 IMR（判断是否快速中断）
    │
    ├─ IMR bit=0（普通中断）
    │   └→ MicroBlaze 跳转到统一的中断入口 → 软件读 ISR 分发
    │
    └─ IMR bit=1（快速中断，本程序的方式）⭐
        └→ INTC 直接从 IVAR 读取 ISR 地址 → 硬件跳转到对应函数
            ├→ switch_handle()
            ├→ button_handle()
            └→ timer_handle()
```

**⑤ CPU 中断使能**
- 调用 `microblaze_enable_interrupts()` 开启 MicroBlaze 全局中断

---

### 5. switch_handle() 开关快速中断服务程序

```c
switch_handle()
  ├── 读取 16 位开关状态
  ├── 输出到 16 位 LED（一一对应）
  ├── if (sw != 0)    // 开关按下时打印
  │   └── UART 打印开关值
  └── 写 ISR 清除 GPIO_0 中断标志
```

> ⚠ **注意**：开关在**按下和释放**时均会触发中断，代码通过判断 `sw != 0` 仅在开关按下时打印，避免释放时重复输出。快速中断模式下，该函数由硬件直接跳转调用，没有额外的软件分发开销。

### 6. button_handle() 按键快速中断服务程序

```c
button_handle()
  ├── 读取按键值（仅取低 5 位 &0x1f）
  ├── if (button == 0) → 清除标志并 return（松开引起的中断不处理）
  ├── UART 打印按键编码
  ├── 遍历 5 位，找到被按下的按键索引 → 存入 mask
  ├── 更新显示缓冲区：
  │   ├── 将 segcode[1]~segcode[7] 左移一位 → segcode[0]~segcode[6]
  │   └── segcode[7] = segtable[mask]  // 新字符放在最右侧
  └── 写 ISR 清除 GPIO_2 中断标志
```

> ⚠ **与普通中断模式的重要区别**：本程序中按键松手检测采用 **直接 return** 而非忙等待。`while((读取值 & 0x1f) != 0)` 已被注释掉。这样设计避免了在快速中断 ISR 内部长时间阻塞。

### 7. timer_handle() 定时器快速中断服务程序

```c
timer_handle()
  ├── 消影：
  │   ├── 位选输出 0xFF（所有位不选）
  │   └── 段码输出 0xFF（所有段熄灭）
  ├── 输出当前位的位码 → 数码管位选引脚
  ├── 输出当前位的段码 → 数码管段码引脚
  ├── pos++    // 指向下一位
  ├── if (pos == 8) pos = 0  // 循环
  └── 写 TCSR 清除 Timer_0 中断标志
```

**动态扫描原理**：
- 定时器以固定频率触发中断（约 10µs，取决于系统时钟）
- 每次中断仅点亮 1 位数码管
- 8 位数码管循环扫描，利用人眼视觉暂留效应呈现稳定显示
- 消影操作（先关闭所有位）可防止位切换时的残影

---

## 快速中断 vs 普通中断：代码差异对照

| 特性 | TASK_INT_0（普通中断） | TASK_FAST_INT（快速中断）⭐ |
|------|----------------------|---------------------------|
| 主 ISR 函数 | `My_ISR()` 统一入口 | **不需要** — 硬件直接分发 |
| ISR 函数属性 | `__attribute__((interrupt_handler))` | `__attribute__((fast_interrupt))` |
| INTC 分发方式 | 软件读 ISR → if 判断 → 调用处理函数 | IMR + IVAR → 硬件向量跳转 |
| IMR 寄存器 | 不配置（默认全0=普通中断） | 配置为 0x7（三个中断均为快速中断） |
| IVAR 寄存器 | 不配置 | 写入每个 ISR 的函数地址 |
| 中断处理延迟 | 需要经过主 ISR 入口 + 多次 if 判断 | 硬件直接跳转到目标 ISR，延迟更低 |
| GPIO_0_INTC_ID 等宏 | 不需要（不配置 IVAR） | 必须定义，用于计算 IVAR 偏移地址 |
| 按键松手处理 | 支持忙等待（已注释） | 直接 return |
| segcode 初始值 | `0xff`（全灭） | `0xc0`（数字 0） |

---

## 中断优先级与中断号

中断掩码宏定义（按位对应 INTC 中断输入）：

| 中断源 | 掩码 | INTC ID | 描述 |
|--------|------|---------|------|
| `XPAR_AXI_GPIO_0_IP2INTC_IRPT_MASK` | 0x1 | 0 | 开关中断（INTC 第 0 位） |
| `XPAR_AXI_GPIO_2_IP2INTC_IRPT_MASK` | 0x2 | 1 | 按键中断（INTC 第 1 位） |
| `XPAR_AXI_TIMER_0_INTERRUPT_MASK` | 0x4 | 2 | 定时器中断（INTC 第 2 位） |

> ⚠ **IMR 寄存器写入值**：`0x1 | 0x2 | 0x4 = 0x7`，表示 INTC 的前 3 个中断输入均配置为快速中断模式。
>
> ⚠ **IVAR 偏移计算**：`XIN_IVAR_OFFSET + (INTC_ID × 4)`，每个 IVAR 占 4 字节（32 位地址）。

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

### 1. 快速中断模式的硬件依赖

快速中断模式需要 **INTC 和 MicroBlaze 同时启用快速中断支持**。如果 Vivado Block Design 中未勾选 "Enable Fast Interrupts"，则：
- IMR/IVAR 寄存器可能不存在或不可写
- 即使写入也不生效，中断会回退到普通中断模式
- 此时所有快速中断 ISR 不会被调用，程序表现为"中断不响应"

### 2. IMR 掩码与 IVAR 索引必须匹配

```c
// IMR 写给 INTC 第 0/1/2 位 → 对应 IVAR 偏移 +0、+4、+8
Xil_Out32(..., XIN_IMR_OFFSET, 0x1 | 0x2 | 0x4);
Xil_Out32(..., XIN_IVAR_OFFSET + (0 * 4), (u32)switch_handle);  // 对应 0x1
Xil_Out32(..., XIN_IVAR_OFFSET + (1 * 4), (u32)button_handle);  // 对应 0x2
Xil_Out32(..., XIN_IVAR_OFFSET + (2 * 4), (u32)timer_handle);   // 对应 0x4
```

如果 Vivado 中 GPIO_2 错误地连到了 INTC 的第 0 位（而非第 1 位），则按键中断发生时会错误地跳转到 `switch_handle()`。

### 3. 快速中断 ISR 中不能调用 xil_printf

`xil_printf()` 内部可能涉及 UART 驱动程序的中断或忙等待操作，在快速中断上下文中调用可能导致以下问题：
- **竞态条件**：如果 UART 也使用了中断，快速中断可能打断 UART 的发送过程
- **死锁**：UART 忙等待期间如果有其他中断触发（因快速中断可能屏蔽其他中断），可能导致死锁
- **建议**：快速中断 ISR 应尽可能简短，避免 I/O 打印。调试完成后建议移除 ISR 中的 `xil_printf()` 调用，或改为在 main() 循环中轮询处理

### 4. 快速中断 ISR 中各自清除外设级中断标志

与普通中断模式不同，快速中断模式**没有主 ISR 来清除 INTC 的 IAR**。因此需要注意：
- 每个 ISR 必须自行清除对应外设的中断标志（如 GPIO 的 ISR、Timer 的 TCSR 中断标志位）
- INTC 硬件在快速中断模式下可能会自动管理中断确认（取决于具体 IP 版本），但仍建议在外设层清除

### 5. 定时器初值偏差

`RESET_VALUE0 = 1000 - 2` 在 C 语言中实际等于 998。注释中写的是 `0.000001s`（1µs），但实际约为 10µs（100MHz时钟）。原因可能有二：
- FPGA 系统时钟并非 100MHz（可能为 10MHz 或 1MHz）→ 此时 1µs 成立
- 注释描述有误

### 6. 开关双边沿触发

开关 GPIO 中断在**上升沿和下降沿**都会触发。`switch_handle()` 用 `if(sw)` 过滤掉释放状态，但如果开关抖动可能导致多次触发（未做硬件/软件消抖）。

### 7. 消影时序

`timer_handle()` 中先输出 `0xFF`（熄灭所有段）再输出新的位码/段码，可有效消除位切换时的残影。这是数码管动态扫描的标准做法。

### 8. 显示缓冲区左移

每次按键按下，显示缓冲区向左移位，新字符插入最右侧，实现类似"滚动输入"的效果。初始显示 `0xc0`（数字 0），按键后新字符从右侧移入。

### 9. 未使用资源

- `RESET_VALUE1`（100000-2）和 `STEP_PACE`（10000000）已定义但未使用
- 仅使用了 Timer 0，Timer 1 未初始化

---

## 程序执行流程示例

```
上电 → main() → Initialization()
    │               ├── 配置 GPIO 方向
    │               ├── 使能 GPIO 中断
    │               ├── 初始化定时器 T0
    │               ├── ⭐ 配置 IMR（快速中断模式）
    │               ├── ⭐ 配置 IVAR（ISR 向量地址）
    │               ├── 使能 INTC（IER + MER）
    │               └── microblaze_enable_interrupts()
    │
    └── while(1) 等待中断
         │
         ├─ [定时器每 10µs 触发]
         │   └→ INTC 识别中断 → 查 IMR[bit2]=1 → 读 IVAR[2] → 硬件跳转到 timer_handle()
         │       ├── 消影（全灭）
         │       ├── 输出位码 + 段码（点亮当前位）
         │       ├── pos = (pos+1) % 8
         │       └── 清除 Timer 中断标志
         │
         ├─ [用户拨动开关]
         │   └→ INTC 识别中断 → 查 IMR[bit0]=1 → 读 IVAR[0] → 硬件跳转到 switch_handle()
         │       ├── 读取开关值 → 输出到 LED
         │       ├── if (sw != 0) → UART 打印开关值
         │       └── 清除 GPIO_0 中断标志
         │
         └─ [用户按下按键 2]
             └→ INTC 识别中断 → 查 IMR[bit1]=1 → 读 IVAR[1] → 硬件跳转到 button_handle()
                 ├── 读取按键值 → if (0) return（松手不处理）
                 ├── 打印 "The pushed button's code is 0x04"
                 ├── mask = 1（找到 button 的 bit 位）
                 ├── segcode 左移：[A,B,C,D,E,F,G,H] → [B,C,D,E,F,G,H,segtable[1]]
                 │                    ↑ 旧显示            ↑ 新字符 'U' 出现在最右侧
                 └── 清除 GPIO_2 中断标志
```

---

## 快速中断模式的优势与局限

### 优势
- **响应延迟更低**：硬件向量跳转避免了软件读寄存器 + 多次 if 判断的开销
- **代码结构更清晰**：每个 ISR 独立，无需维护集中的主分发函数
- **可扩展性好**：新增中断源只需配置 IMR + IVAR，不修改已有 ISR

### 局限
- **硬件依赖**：必须 INTC 和 MicroBlaze 同时支持快速中断
- **中断源数量限制**：快速中断向量数受硬件 IVAR 数量限制
- **ISR 内需谨慎操作**：快速中断可能打断其他中断处理，全局变量访问需考虑原子性
- **xil_printf 使用风险**：如前述，在 ISR 中使用打印需格外谨慎