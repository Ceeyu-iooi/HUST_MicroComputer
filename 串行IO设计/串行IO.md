# 串行IO设计

## 概述

本目录包含与 **串行IO（Serial I/O）** 实验相关的设计文档。串行IO指通过 UART（通用异步收发器）等串行通信协议，在 MicroBlaze 处理器之间或 MicroBlaze 与上位机之间进行数据传输。

本实验的核心是 **CPU_INT_TIMER 到 CPU_UART 的硬件升级**，即在原有并行 IO 中断实验平台（含 INTC、AXI GPIO×3、AXI Timer）的基础上，增加 AXI UARTLite IP 核，实现双 MicroBlaze 之间的串口通信。

---

## 目录结构

| 文件 | 类型 | 说明 |
|------|------|------|
| `CPU_INT_TIMER到CPU_UART硬件升级说明.md` | Markdown文档 | CPU_INT_TIMER 平台升级到 CPU_UART 平台的详细说明，包括：UART IP 核添加步骤、Vivado Block Design 配置、中断连接关系、地址映射变化、UARTLite 寄存器说明、双 UART 通信协议设计等 |

---

## 硬件升级概要

从 `CPU_INT_TIMER` 升级到 `CPU_UART` 的主要变更：

### 新增硬件模块

| IP 核 | 实例名 | 功能 |
|-------|--------|------|
| AXI UARTLite | UART1 | 发送端：开关中断触发时通过 UART1 TX → UART2 RX 发送 16 位开关值 |
| AXI UARTLite | UART2 | 接收端/发送端：通过 UART2 TX → UART1 RX 发送按键值，并接收开关值更新 LED |

### 中断扩展

- INTC 中断输入从 **3 个** 扩展为 **5 个**：
  - `Intr[0]` = GPIO_0（开关中断）
  - `Intr[1]` = GPIO_2（按键中断）
  - `Intr[2]` = Timer_0（定时器中断）
  - `Intr[3]` = UART1（接收按键码）
  - `Intr[4]` = UART2（接收开关值）

### 通信协议

```
开关值: GPIO_0 → ISR → UART1 TX → (硬件连线) → UART2 RX → 组装16位 → LED显示
按键值: GPIO_2 → ISR → UART2 TX → (硬件连线) → UART1 RX → 解析段码 → 数码管显示
```

---

## 相关工程

对应的软件工程项目位于 `UART/` 目录，该工程基于 `CPU_UART` 硬件平台，使用普通中断方式驱动。

详细说明见：`UART/readme.md`