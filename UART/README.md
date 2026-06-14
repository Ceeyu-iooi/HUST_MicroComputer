# UART - 双 UART 通信程序

## 概述

本程序基于 **CPU_UART** 硬件平台，使用 **普通中断模式** 实现双 MicroBlaze 之间的串口通信。在原有并行 IO 中断实验平台（含 INTC、AXI GPIO×3、AXI Timer）的基础上，增加了 AXI UARTLite IP 核，实现以下功能：

1. **开关（Switch）中断** → UART1 TX → UART2 RX → LED 显示：开关拨动时，16 位开关值分高/低 8 位两字节通过 UART1 发送，UART2 接收后组装为 16 位并显示在 LED 上
2. **按键（Button）中断** → UART2 TX → UART1 RX → 数码管显示：按键按下时，按键值通过 UART2 发送，UART1 接收后解析按键并更新数码管段码显示缓冲区，实现字符滚动显示
3. **定时器中断**：以固定间隔扫描 8 位数码管，实现动态显示刷新

---

## Vivado 硬件设计要点（Block Design 配置）

### 硬件配置要求

本程序基于 `CPU_UART` 平台，该平台在 `CPU_INT_TIMER` 基础上增加了两个 AXI UARTLite IP 核。

| 组件 | 配置项 | 必须值 | 说明 |
|------|--------|--------|------|
| **AXI GPIO 0** | Data Width Ch1/Ch2 | 16/16 | 开关(输入)+LED(输出)，使能中断 |
| **AXI GPIO 1** | Data Width Ch1/Ch2 | 8/8 | 位选(输出)+段码(输出) |
| **AXI GPIO 2** | Data Width Ch1 | 5 | 按键(输入)，使能中断 |
| **AXI Timer 0** | Generate Interrupt | ✔ | 驱动数码管动态扫描 |
| **AXI INTC 0** | Interrupt Inputs | ≥5 | 管理 5 个中断源 |
| **AXI UARTLite 1** | Baud Rate | 115200 | 发送端：开关值 TX → UART2 RX |
| **AXI UARTLite 2** | Baud Rate | 115200 | 接收/发送端：按键值 TX → UART1 RX |

### 中断连接关系

```
AXI GPIO 0 .ip2intc_irpt  →  AXI INTC 0 .Intr[0]   (掩码 0x01, 开关)
AXI GPIO 2 .ip2intc_irpt  →  AXI INTC 0 .Intr[1]   (掩码 0x02, 按键)
AXI Timer 0 .Interrupt    →  AXI INTC 0 .Intr[2]   (掩码 0x04, 定时器)
AXI UARTLite 1 .Interrupt →  AXI INTC 0 .Intr[3]   (掩码 0x08, UART1接收)
AXI UARTLite 2 .Interrupt →  AXI INTC 0 .Intr[4]   (掩码 0x10, UART2接收)
AXI INTC 0 .Intr          →  MicroBlaze .INTERRUPT
```

---

## 硬件资源映射

| 外设 | 地址基址 | 功能 |
|------|---------|------|
| AXI GPIO 0 | `XPAR_AXI_GPIO_0_BASEADDR` | 通道1：16位开关(输入)；通道2：16位LED(输出) |
| AXI GPIO 1 | `XPAR_AXI_GPIO_1_BASEADDR` | 通道1：8位位选(输出)；通道2：8位段码(输出) |
| AXI GPIO 2 | `XPAR_AXI_GPIO_2_BASEADDR` | 通道1：5位按键(输入) |
| AXI Timer 0 | `XPAR_AXI_TIMER_0_BASEADDR` | 定时器，驱动数码管扫描 |
| AXI INTC 0 | `XPAR_AXI_INTC_0_BASEADDR` | 中断控制器，管理 5 个中断源 |
| AXI UARTLite 1 | `XPAR_AXI_UARTLITE_1_BASEADDR` | 发送开关值(TX) / 接收按键码(RX) |
| AXI UARTLite 2 | `XPAR_AXI_UARTLITE_2_BASEADDR` | 接收开关值(RX) / 发送按键码(TX) |

---

## 代码结构

### 1. 中断掩码定义（5 个中断源）

```c
#define XPAR_AXI_GPIO_0_IP2INTC_IRPT_MASK    0x01  // 开关中断 (bit 0)
#define XPAR_AXI_GPIO_2_IP2INTC_IRPT_MASK    0x02  // 按键中断 (bit 1)
#define XPAR_AXI_TIMER_0_INTERRUPT_MASK      0x04  // 定时器中断 (bit 2)
#define XPAR_AXI_UARTLITE_1_INTERRUPT_MASK   0x08  // UART1接收中断 (bit 3)
#define XPAR_AXI_UARTLITE_2_INTERRUPT_MASK   0x10  // UART2接收中断 (bit 4)
```

### 2. 全局变量

- `segtable[5]`：5 个字符段码表：C(0xc6) U(0xc1) L(0xc7) R(0x88) d(0xa1)
- `segcode[8]`：8 位数码管显示缓冲区，初始全 0xff（全灭）
- `poscode[8]`：8 位位码，低电平选中
- `pos`：当前扫描位置 0~7
- `sw_byte_count`：UART2 接收状态跟踪（0=等待高字节，1=等待低字节）
- `sw_high_byte`：暂存开关值高字节

### 3. 主 ISR：My_ISR()

```c
My_ISR()
  ├── 读取 INTC 中断状态寄存器 (ISR)
  ├── if (GPIO_0 中断)  → switch_handle()     // 开关变化，发送开关值
  ├── if (GPIO_2 中断)  → button_handle()     // 按键按下，发送按键码
  ├── if (UART1 中断)   → uart1_handle()      // 接收按键码，更新数码管
  ├── if (UART2 中断)   → uart2_handle()      // 接收开关值，更新 LED
  ├── if (Timer 中断)   → timer_handle()      // 扫描数码管
  └── 写 IAR 清除 INTC 中断标志
```

### 4. 各中断处理函数

**switch_handle()**：读取 16 位开关值 → 分高 8 位/低 8 位通过 UART1 TX 发送 → 清除 GPIO_0 中断标志

**button_handle()**：读取按键值 → 区别按下/松开（松开不处理）→ 通过 UART2 TX 发送按键码 → 清除 GPIO_2 中断标志

**uart1_handle()**：读取 UART1 接收寄存器 → 若为按键码 → 存入 `button_value` → 更新数码管显示缓冲区（左移一位，新字符在最右侧）→ 清除 UART1 中断标志

**uart2_handle()**：
```
uart2_handle()
  ├── 读取 UART2 接收寄存器 (rx_data)
  ├── if (sw_byte_count == 0)  // 高字节
  │   ├── sw_high_byte = rx_data
  │   └── sw_byte_count = 1
  └── if (sw_byte_count == 1)  // 低字节
      ├── 组装 16 位开关值 = (sw_high_byte << 8) | rx_data
      ├── 输出到 LED
      └── sw_byte_count = 0
```

**timer_handle()**：消影 → 输出当前位位码+段码 → pos++ → pos %= 8

---

## 双 UART 通信协议

```
┌─────────────┐         UART1 TX           ┌─────────────┐
│  MicroBlaze │  ────────────────→  UART2   │  MicroBlaze │
│   发送端    │  发送: 高8位→低8位(UART1 RX)│   接收端    │
│  (UART1)    │  ←────────────────  (UART2) │  (UART2)    │
│             │  接收: 按键码           │             │
└─────────────┘                           └─────────────┘
```

**发送数据格式**：
- 开关值：分 2 字节发送（先高 8 位，后低 8 位），接收端通过 `sw_byte_count` 状态机组装
- 按键码：1 字节直接发送，接收端解析出按键索引（0~4）后查段码表

---

## 注意事项

1. **UART 通信时序**：两个 UART 之间是硬件直连（TX→RX），无需流控制。波特率必须一致（默认 115200）。
2. **开关值分字节接收**：由于 UARTLite 一次只能发送/接收 1 字节（8 位），16 位开关值需要拆分两次发送。接收端通过 `sw_byte_count` 状态机确保高字节和低字节正确配对。如果开关频繁拨动，可能导致高/低字节错位。
3. **中断标志清除**：每个 ISR 内清除各自外设的中断标志（GPIO ISR、Timer TCSR、UART ISR），INTC 的 IAR 在 My_ISR() 末尾统一清除。
4. **数码管扫描**：定时器每约 10µs 触发一次中断，扫描 1 位数码管，8 位循环，刷新率约 12.5kHz。
5. **按键消抖**：代码中未做硬件/软件消抖，可能因按键抖动导致多次触发。