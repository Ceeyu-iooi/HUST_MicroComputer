# CPU_INT — 基础中断硬件平台

## 概述

`CPU_INT` 是含**中断控制器（INTC）** 和 **AXI Timer** 的 MicroBlaze 硬件平台，支持**快速中断（Fast Interrupt）** 模式。通过中断方式驱动开关、按键和数码管，实现高效的外设响应。适用于需要低延迟中断响应的应用场景。

---

## 硬件配置

| 外设 | 功能 | 通道1 | 通道2 |
|------|------|-------|-------|
| **AXI GPIO 0** | 16 位拨码开关输入 + 16 位 LED 输出 | 开关（输入，16位） | LED（输出，16位） |
| **AXI GPIO 1** | 8 位数码管位选 + 8 位数码管段码输出 | 位选（输出，8位） | 段码（输出，8位） |
| **AXI GPIO 2** | 5 位按键输入 | 按键（输入，5位） | — |
| **AXI Timer 0** | 定时器，用于数码管动态扫描 | — | — |
| **AXI INTC 0** | 中断控制器，管理 3 个中断源 | — | — |

---

## XSA 文件

| 文件 | 路径 |
|------|------|
| `CPU_Int_wrapper.xsa` | `hw/` 目录下 |

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

- **中断控制器**：✔ 含 AXI INTC，支持最多 3 个中断源
- **定时器**：✔ 含 AXI Timer
- **中断模式**：支持**快速中断（Fast Interrupt）**——每个 ISR 使用 `__attribute__((fast_interrupt))` 独立声明，硬件通过 IVAR 向量跳转，无需软件分发
- **驱动方式**：中断驱动
- **适用处理器**：MicroBlaze

---

## 对应应用项目

| 应用工程 | 说明 |
|---------|------|
| `TASK_FAST_INT` | 快速中断交互程序，使用 fast_interrupt 实现低延迟中断响应 |
| `CPU_qq` | 最简单的 GPIO 测试程序（轮询方式，基于此平台） |

> 应用详情见 [TASK_FAST_INT/readme.md](../TASK_FAST_INT/readme.md)。

---

## 快速中断模式说明

- 每个 ISR 独立声明 `__attribute__((fast_interrupt))`，硬件直接通过 IVAR 向量跳转
- 无需软件读取 INTC ISR 寄存器进行中断分发，中断响应延迟更低
- MicroBlaze 需配置为支持快速中断模式

---

## 使用场景

- 需要低延迟中断响应的实时控制系统
- 学习快速中断（Fast Interrupt）模式在 MicroBlaze 上的应用
- 中断方式驱动开关/按键/数码管的场景