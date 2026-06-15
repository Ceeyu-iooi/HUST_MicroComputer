# CPU_UART — UART 通信硬件平台

## 概述

`CPU_UART` 是在 `CPU_INT_TIMER` 基础上增加 **AXI UARTLite×2** 的 MicroBlaze 硬件平台，支持双 MicroBlaze 串口通信。在保留中断控制、定时器和 GPIO 外设的基础上，扩展了 UART 异步串行通信能力。

---

## 硬件配置

| 外设 | 功能 | 通道1 | 通道2 |
|------|------|-------|-------|
| **AXI GPIO 0** | 16 位拨码开关输入 + 16 位 LED 输出 | 开关（输入，16位） | LED（输出，16位） |
| **AXI GPIO 1** | 8 位数码管位选 + 8 位数码管段码输出 | 位选（输出，8位） | 段码（输出，8位） |
| **AXI GPIO 2** | 5 位按键输入 | 按键（输入，5位） | — |
| **AXI Timer 0** | 定时器，用于数码管动态扫描 | — | — |
| **AXI INTC 0** | 中断控制器，管理中断源 | — | — |
| **AXI UARTLite 1** | 串口 1（TX/RX） | — | — |
| **AXI UARTLite 2** | 串口 2（TX/RX） | — | — |

> 相比 CPU_INT_TIMER，增加了 AXI UARTLite×2（UART1 + UART2）。

---

## XSA 文件

| 文件 | 路径 |
|------|------|
| `CPU_UART_wrapper.xsa` | `hw/` 目录下 |

---

## 硬件特性

- **中断控制器**：✔ 含 AXI INTC
- **定时器**：✔ 含 AXI Timer
- **UART**：✔ 含 AXI UARTLite×2，支持全双工串口通信
- **驱动方式**：中断驱动
- **适用处理器**：MicroBlaze

---

## 对应应用项目

| 应用工程 | 说明 |
|---------|------|
| `UART` | 双 UART 通信程序：开关中断→UART1 TX→UART2 RX→LED；按键中断→UART2 TX→UART1 RX→数码管 |

> 应用详情见 [UART/README.md](../UART/README.md)。

---

## 平台升级路线

```
CPU_INT_TIMER (增强中断平台)
       │
       ├── + AXI UARTLite×2 → CPU_UART (UART 通信平台)
       │
       └── + AXI Quad SPI   → CPU_SPI  (SPI 通信平台)
```

> 硬件升级详细说明见 `串行IO设计/CPU_INT_TIMER到CPU_UART硬件升级说明.md`。

---

## 使用场景

- 学习 MicroBlaze UART 串口通信
- 双 MicroBlaze 系统之间的数据交互
- 将并行 IO 控制与串行通信结合的综合性实验