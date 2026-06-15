# CPU_SPI — SPI 通信硬件平台

## 概述

`CPU_SPI` 是在 `CPU_INT_TIMER` 基础上增加 **AXI Quad SPI** 模块的 MicroBlaze 硬件平台，支持 SPI 主从通信。在保留中断控制、定时器和 GPIO 外设的基础上，扩展了 SPI 同步串行通信能力。

---

## 硬件配置

| 外设 | 功能 | 通道1 | 通道2 |
|------|------|-------|-------|
| **AXI GPIO 0** | 16 位拨码开关输入 + 16 位 LED 输出 | 开关（输入，16位） | LED（输出，16位） |
| **AXI GPIO 1** | 8 位数码管位选 + 8 位数码管段码输出 | 位选（输出，8位） | 段码（输出，8位） |
| **AXI GPIO 2** | 5 位按键输入 | 按键（输入，5位） | — |
| **AXI Timer 0** | 定时器，用于数码管动态扫描 | — | — |
| **AXI INTC 0** | 中断控制器，管理中断源 | — | — |
| **AXI Quad SPI** | SPI 主从通信模块 | — | — |

> 相比 CPU_INT_TIMER，增加了 AXI Quad SPI 模块。

---

## XSA 文件

| 文件 | 路径 |
|------|------|
| `CPU_SPI_wrapper.xsa` | `hw/` 目录下 |

---

## 硬件特性

- **中断控制器**：✔ 含 AXI INTC
- **定时器**：✔ 含 AXI Timer
- **SPI**：✔ 含 AXI Quad SPI，支持主/从模式、多从机选择
- **驱动方式**：中断驱动
- **适用处理器**：MicroBlaze

---

## 对应应用项目

目前尚无对应应用工程，预留 SPI 通信示例开发。

---

## 平台升级路线

```
CPU_INT_TIMER (增强中断平台)
       │
       ├── + AXI UARTLite×2 → CPU_UART (UART 通信平台)
       │
       └── + AXI Quad SPI   → CPU_SPI  (SPI 通信平台)
```

---

## 使用场景

- 学习 MicroBlaze SPI 同步串行通信
- SPI 主从设备间的数据交互
- 将并行 IO 控制与 SPI 通信结合的综合性实验