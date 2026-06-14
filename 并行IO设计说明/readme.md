# 并行IO设计说明

## 概述

本目录包含与 **并行IO（General Purpose I/O）** 实验相关的设计文档和流程图。并行IO指直接通过 AXI GPIO IP 核与 MicroBlaze 处理器相连的开关、LED、按键、数码管等外设的输入输出操作。

本实验围绕三种不同的驱动方式展开，对应三个软件工程项目：

| 软件工程 | 驱动方式 | 对应硬件平台 | 核心特点 |
|---------|---------|-------------|---------|
| `TASK_NOINT_` | 轮询（Polling） | `CPU_noint`（无INTC、无Timer） | 主循环for扫描数码管，软件延时，轮询GPIO ISR检测开关变化 |
| `TASK_INT_0` | 普通中断（Normal Interrupt） | `CPU_INT_TIMER` | 统一ISR入口 `My_ISR()`，软件读取INTC ISR寄存器分发中断 |
| `TASK_FAST_INT` | 快速中断（Fast Interrupt） | `CPU_INT` | 每个ISR独立声明 `__attribute__((fast_interrupt))`，硬件直接向量跳转 |

---

## 目录结构

| 文件 | 类型 | 说明 |
|------|------|------|
| `三种代码设计思路对比.docx` | Word文档 | 轮询/普通中断/快速中断三种驱动方式的代码设计思路详细对比，包括初始化流程、ISR结构、中断响应路径等 |
| `CPU_INT_TIMER_关键头文件总结.docx` | Word文档 | CPU_INT_TIMER 平台使用的关键头文件及其层次关系总结（xil_io.h、xgpio_l.h、xtmrctr_l.h、xintc_l.h、mb_interface.h、xparameters.h 等） |
| `TASK_FAST_INT_流程图_Mermaid.txt` | Mermaid文本 | TASK_FAST_INT 快速中断项目的程序流程图，使用 Mermaid 语法描述 |
| `TASK_INT_0_流程图_Mermaid.txt` | Mermaid文本 | TASK_INT_0 普通中断项目的程序流程图，使用 Mermaid 语法描述 |
| `TASK_NOINT_流程图_Mermaid.txt` | Mermaid文本 | TASK_NOINT_ 轮询项目的程序流程图，使用 Mermaid 语法描述 |

---

## 三种驱动方式对比

### 1. 轮询方式（TASK_NOINT_）

- **硬件平台**：`CPU_noint` — **无 INTC、无 AXI Timer**
- **开关检测**：主循环中轮询 GPIO ISR 寄存器（即使中断功能未使能，ISR 寄存器仍记录边沿变化）
- **按键检测**：直接读取 GPIO 数据寄存器，忙等待松手
- **数码管扫描**：`for(i=0→7)` 循环 + 软件延时（`for(j=0→999)`），在延时间隙中查询开关
- **关键特点**：
  - 不使用任何硬件中断和定时器
  - 按键按下期间数码管暂停扫描（显示静止/熄灭）
  - 开关检测依赖于数码管扫描的延时循环，实时性较差

### 2. 普通中断方式（TASK_INT_0）

- **硬件平台**：`CPU_INT_TIMER` — **含 INTC、AXI Timer**
- **中断入口**：统一 `My_ISR()` 函数，声明为 `__attribute__((interrupt_handler))`
- **中断分发**：读取 INTC ISR 寄存器 → 多个 `if` 判断中断源 → 调用对应处理函数
- **数码管扫描**：定时器 T0 每 ~10µs 触发中断，每次 ISR 扫描 1 位数码管（8 位循环）
- **关键特点**：
  - 通过硬件定时器实现精确的数码管动态扫描
  - 按键松手检测采用忙等待（阻塞其他中断，已注释但功能保留）
  - segcode 缓冲区初始值为 `0xc0`（数字 0）

### 3. 快速中断方式（TASK_FAST_INT）

- **硬件平台**：`CPU_INT` — **含 INTC（快速中断模式已使能）**
- **中断入口**：每个 ISR 独立声明为 `__attribute__((fast_interrupt))`，**无需主分发函数**
- **中断分发**：INTC 通过 IMR 寄存器标记快速中断，直接读取 IVAR 向量地址 → 硬件跳转到对应 ISR
- **数码管扫描**：同普通中断方式，定时器驱动
- **关键特点**：
  - 响应延迟更低（省去软件分发开销）
  - 按键松手直接 `return`（而非忙等待）
  - 需配置 IMR（中断模式寄存器）和 IVAR（中断向量地址寄存器）
  - 硬件上需要 INTC 和 MicroBlaze 同时启用快速中断支持

---

## 三种方式的适用场景

| 驱动方式 | 优点 | 缺点 | 适用场景 |
|---------|------|------|---------|
| 轮询 | 硬件要求最低，无需 INTC/Timer | CPU 占用高，实时性差，按键长按时数码管停显 | 简单演示、硬件资源受限 |
| 普通中断 | 实时性好，代码结构清晰 | 中断分发有软件开销，需 INTC 支持 | 一般嵌入式应用 |
| 快速中断 | 响应最快，无分发开销 | 硬件要求高，ISR 内需谨慎（不可调用 xil_printf） | 对中断响应时间有严格要求的场景 |

---

## 相关工程 README

各软件工程的完整硬件设计要点、代码结构、寄存器配置说明见对应项目目录下的 `readme.md`：

- `TASK_NOINT_/readme.md` — 轮询方式详细说明
- `TASK_INT_0/readme.md` — 普通中断方式详细说明
- `TASK_FAST_INT/readme.md` — 快速中断方式详细说明