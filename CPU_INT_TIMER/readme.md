# CPU_INT_TIMER — 增强中断硬件平台（通用考试平台）

## 概述

`CPU_INT_TIMER` 是含**中断控制器（INTC）** 和 **AXI Timer** 的 MicroBlaze 硬件平台，与 `CPU_INT` 硬件结构类似（INTC + AXI GPIO×3 + AXI Timer），但 XSA 来源不同。本平台**不含串行通信模块**，是**涉及全部微机原理实验考试题目的通用硬件平台**。

> 💡 **考试说明**：所有 12 道实验题目 + 2026.6.15 期末考试题的参考答案均基于此平台开发（普通中断模式），见 `微机原理实验备考/` 目录。

---

## 硬件配置

| 外设 | 功能 | 通道1 | 通道2 |
|------|------|-------|-------|
| **AXI GPIO 0** | 16 位拨码开关输入 + 16 位 LED 输出 | 开关（输入，16位） | LED（输出，16位） |
| **AXI GPIO 1** | 8 位数码管位选 + 8 位数码管段码输出 | 位选（输出，8位） | 段码（输出，8位） |
| **AXI GPIO 2** | 5 位按键输入 | 按键（输入，5位） | — |
| **AXI Timer 0** | 定时器，用于数码管动态扫描 | — | — |
| **AXI INTC 0** | 中断控制器，管理 3 个中断源 | — | — |

> ⚠ **不含 UART、不含 SPI**，纯并行 IO 中断控制平台。

---

## XSA 文件

| 文件 | 路径 |
|------|------|
| `CPU_INT_wrapper.xsa` | `hw/` 目录下 |

> 该 XSA 来源与 `CPU_INT` 平台的 XSA 不同（来自不同 Vivado 工程）。

---

## 中断连接关系

```
AXI GPIO 0 .ip2intc_irpt  →  AXI INTC 0 .Intr[0]   (掩码 0x1) ← 开关中断
AXI GPIO 2 .ip2intc_irpt  →  AXI INTC 0 .Intr[1]   (掩码 0x2) ← 按键中断
AXI Timer 0 .Interrupt    →  AXI INTC 0 .Intr[2]   (掩码 0x4) ← 定时器中断
AXI INTC 0 .Intr          →  MicroBlaze .INTERRUPT
```

---

## 硬件特性

- **中断控制器**：✔ 含 AXI INTC
- **定时器**：✔ 含 AXI Timer
- **中断模式**：支持**普通中断（Normal Interrupt）**——统一 ISR 入口 `My_ISR()`，软件读取 INTC ISR 寄存器分发中断
- **驱动方式**：中断驱动
- **适用处理器**：MicroBlaze

---

## 对应应用项目

| 应用工程 | 说明 |
|---------|------|
| `TASK_INT_0` | 普通中断交互程序，统一 ISR 入口分发中断 |
| `Test` | 多功能显示测试程序，5 种模式（C/L/D/R/U） |
| `微机原理实验备考/` 下所有 12 题 + 2026.6.15 期末题 | 考试题目参考答案源码 |

> 应用详情见 [TASK_INT_0/readme.md](../TASK_INT_0/readme.md)。

---

## 与 CPU_INT 的区别

| 特性 | CPU_INT | CPU_INT_TIMER |
|------|---------|---------------|
| XSA 来源 | 独立 Vivado 工程 | 独立 Vivado 工程（不同来源） |
| 硬件结构 | INTC + AXI GPIO×3 + AXI Timer | 同左 |
| 中断模式 | 快速中断（Fast Interrupt） | 普通中断（Normal Interrupt） |
| 对应应用 | TASK_FAST_INT（快速中断示例） | TASK_INT_0（普通中断示例）+ 考试题目 |

---

## 关联平台

- [CPU_INT](../CPU_INT/readme.md) — 相同外设，支持快速中断
- [CPU_UART](../CPU_UART/readme.md) — 在本平台基础上增加 AXI UARTLite×2
- [CPU_SPI](../CPU_SPI/readme.md) — 在本平台基础上增加 AXI Quad SPI

---

## 使用场景

- 微机原理实验考试备考（所有题目均基于此平台）
- 学习普通中断模式（Normal Interrupt）在 MicroBlaze 上的应用
- 需要定时器+中断联合控制的复杂交互场景