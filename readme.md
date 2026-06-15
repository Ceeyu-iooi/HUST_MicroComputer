# Vitis 工作区项目总览
#### 备考快速点击==>>[微机原理实验备考文档](./微机原理实验备考/微机原理实验备考文档.md)

### 目录

- - [硬件平台（Platform Projects）](#硬件平台platform-projects)
  - [软件应用（Application Projects）](#软件应用application-projects)
    - [并行 IO 任务（对应三个软件工程）](#并行-io-任务对应三个软件工程)
    - [串行 IO 任务（对应 UART）](#串行-io-任务对应-uart)
    - [其他应用](#其他应用)
  - [各工程 README 索引](#各工程-readme-索引)
    - [并行 IO 任务（三种驱动方式）](#并行-io-任务三种驱动方式)
    - [串行 IO 任务](#串行-io-任务)
  - [软件源码（共用平台源码）](#软件源码共用平台源码)
    - [代码与题目对应表](#代码与题目对应表)
  - [期末备考](#期末备考)
    - [备考文档](#备考文档)
    - [题目源码与说明（12 题 + 2026.6.15 期末题）](#题目源码与说明12-题--2026615-期末题)
    - [备考要点速览](#备考要点速览)
  - [目录结构速览](#目录结构速览)

---

## 硬件平台（Platform Projects）

| 项目 | XSA 文件 | 硬件配置说明 |
|------|---------|-------------|
| `CPU_noint` | `CPU_noint_wrapper.xsa` | **无中断平台**：仅含 AXI GPIO×3（开关、LED、按键、数码管），**无 INTC、无 AXI Timer**。适合纯轮询方式读取外设的简单应用。 |
| `CPU_INT` | `CPU_Int_wrapper.xsa` | **基础中断平台**：含 INTC（中断控制器）、AXI GPIO×3、AXI Timer，支持快速中断模式。用于中断方式驱动开关/按键/数码管的场景。 |
| `CPU_INT_TIMER` | `CPU_INT_wrapper.xsa` | **增强中断平台（通用考试平台）**：与 CPU_INT 硬件结构类似（INTC + AXI GPIO×3 + AXI Timer），XSA 来源不同。**不含串行通信模块（无 UART、无 SPI）**，是**涉及全部微机原理实验考试题目都通用的硬件平台**。所有 12 道实验题目 + 2026.6.15 期末考试的参考答案均基于此平台，见 `微机原理实验备考/` 目录。 |
| `CPU_UART` | `CPU_UART_wrapper.xsa` | **UART 通信平台**：在 CPU_INT_TIMER 基础上增加 AXI UARTLite×2（UART1 + UART2），支持双 MicroBlaze 串口通信。 |
| `CPU_SPI` | `CPU_SPI_wrapper.xsa` | **SPI 通信平台**：在 CPU_INT_TIMER 基础上增加 AXI Quad SPI 模块，支持 SPI 主从通信。 |

---

## 🧭 学习使用方法

本仓库包含三层结构：**硬件平台（Platform）** → **软件应用（Application）** → **备考资料（Exam Prep）**。建议按以下顺序学习和使用：

### 1️⃣ 理解硬件平台差异

| 学习阶段 | 平台 | 说明 |
|---------|------|------|
| 入门 | `CPU_noint` | 最简单的纯轮询平台，理解 GPIO 基本操作：读开关、写 LED、控制数码管 |
| 进阶 | `CPU_INT_TIMER` | 含中断和定时器，学习普通中断模式和动态扫描原理。**所有考试题目均基于此平台** |
| 提高 | `CPU_INT` | 与 `CPU_INT_TIMER` 硬件相同，但使用快速中断模式，了解硬件向量跳转机制 |
| 扩展 | `CPU_UART` / `CPU_SPI` | 增加串行通信模块，用于学习 UART / SPI 通信协议 |

> 每个硬件平台的详细说明见对应目录下的 `readme.md`。

### 2️⃣ 选择对应的应用工程

根据硬件平台选择对应的软件应用工程进行实践：

- **`CPU_noint`** → `TASK_NOINT_`：学习纯轮询方式驱动外设
- **`CPU_INT_TIMER`** → `TASK_INT_0` + `Test`：学习普通中断方式，理解硬件初始化流程
- **`CPU_INT`** → `TASK_FAST_INT`：学习快速中断方式，对比与普通中断的差异
- **`CPU_UART`** → `UART`：学习 UART 串口通信

> 三种驱动方式（轮询/普通中断/快速中断）的详细对比见 `并行IO设计说明/readme.md`。

### 3️⃣ 深入备考（考试重点）

如需备考微机原理实验考试，重点关注 `CPU_INT_TIMER` 平台 + `TASK_INT_0` 普通中断模式：

1. **阅读备考文档**：`微机原理实验备考/微机原理实验备考文档.md` — 包含完整的中断框架、模板代码、核心逻辑总结
2. **逐题练习**：`微机原理实验备考/test1.c` ~ `test12.c`，每题配有题目说明（`.md`）和参考答案（`.c`）
3. **掌握模板**：`微机原理实验备考/template.c` — 标准初始化框架，考试时可直接套用
4. **熟悉硬件资源**：GPIO×3（开关+LED、数码管位选+段码、按键）、Timer×2、INTC
5. **理解三种中断**：开关中断（GPIO0）、按键中断（GPIO2）、定时器中断（Timer0）

### 4️⃣ 代码开发流程

```
Vivado 生成 XSA → Vitis 创建 Platform → Vitis 创建 Application → 编写/调试 C 代码
```

1. 用 Vivado 生成硬件 XSA 文件
2. 在 Vitis 中导入对应 Platform 项目（`CPU_xxx/` 目录）
3. 创建 Application 项目，关联对应 Platform
4. 编写 C 代码并编译调试

> 每个硬件平台的 `readme.md` 中列出了所有兼容的应用工程，详见各平台说明。

---

## 软件应用（Application Projects）

| Application | 对应 Platform | 驱动方式 | 功能说明 |
|-------------|-------------|---------|---------|
| `TASK_NOINT_` | `CPU_noint` | 轮询（Polling） | **全轮询方式交互程序**。不使用任何硬件中断和定时器。主循环中通过轮询 GPIO 数据寄存器检测按键，软件延时循环扫描 8 位数码管，在延时间隙中轮询 GPIO ISR 寄存器检测开关变化。按键按下期间数码管暂停显示。 |
| `TASK_INT_0` | `CPU_INT_TIMER` | 普通中断（Normal Interrupt） | **中断驱动交互程序**。统一 ISR 入口 `My_ISR()`，软件读取 INTC ISR 寄存器分发中断。开关中断控制 LED，按键中断更新数码管缓冲区，定时器中断驱动 8 位数码管动态扫描（~10µs/位）。 |
| `TASK_FAST_INT` | `CPU_INT` | 快速中断（Fast Interrupt） | **快速中断交互程序**。每个 ISR 独立声明 `__attribute__((fast_interrupt))`，硬件直接通过 IVAR 向量跳转，无需软件分发。功能同 TASK_INT_0，但中断响应延迟更低。 |

> 三种驱动方式的详细对比见 `并行IO设计说明/readme.md`。

### 串行 IO 任务（对应 UART）

| Application | 对应 Platform | 驱动方式 | 功能说明 |
|-------------|-------------|---------|---------|
| `UART` | `CPU_UART` | 普通中断 | **双 UART 通信程序**。开关中断触发 → UART1 TX → UART2 RX → LED 显示（16 位分高/低 8 位两字节传输）；按键中断触发 → UART2 TX → UART1 RX → 数码管段码缓冲区更新。定时器驱动 8 位数码管动态扫描。 |

> 串行 IO 设计的详细硬件升级说明见 `串行IO设计/readme.md`。

### 其他应用

| Application | 对应 Platform | 驱动方式 | 功能说明 |
|-------------|-------------|---------|---------|
| `CPU_qq` | `CPU_INT` | 轮询 | 最简单的 GPIO 测试程序。主循环轮询 GPIO ISR 寄存器检测 16 位拨码开关状态变化，同步点亮对应 LED 并通过 UART 打印。不涉及按键、数码管、定时器或中断。 |
| `Test` | `CPU_INT_TIMER` | 中断 | **多功能显示测试程序**。5 个按键对应 5 种模式：C=流水灯（3 档速度）、L=LED 反映开关/循环左移、D=左边 4 位数码管+LED 奇数位亮、R=右边 4 位数码管+LED 偶数位亮、U=低 4 位带符号十进制滚动显示（2 档速度）。 |

---

## 各工程 README 索引

### 并行 IO 任务（三种驱动方式）

| 工程 | README | 流程图 |
|------|--------|--------|
| `TASK_NOINT_` | `TASK_NOINT_/readme.md` | `并行IO设计说明/TASK_NOINT_流程图_Mermaid.txt` |
| `TASK_INT_0` | `TASK_INT_0/readme.md` | `并行IO设计说明/TASK_INT_0_流程图_Mermaid.txt` |
| `TASK_FAST_INT` | `TASK_FAST_INT/readme.md` | `并行IO设计说明/TASK_FAST_INT_流程图_Mermaid.txt` |

> 三种方式的设计思路详细对比（含硬件要求、初始化流程、ISR 结构、中断响应路径、适用场景等）见 `并行IO设计说明/readme.md` 及 `并行IO设计说明/三种代码设计思路对比.docx`。

### 串行 IO 任务

| 工程 | README | 硬件升级说明 |
|------|--------|-------------|
| `UART` | `UART/readme.md` | `串行IO设计/CPU_INT_TIMER到CPU_UART硬件升级说明.md` |

> 串行 IO 设计的完整说明（含 UART IP 核添加步骤、Vivado Block Design 配置、双 UART 通信协议等）见 `串行IO设计/readme.md`。

---

## 软件源码（共用平台源码）

以下源码基于 `CPU_INT_TIMER` 平台，使用普通中断模式 + `xil_io` 寄存器级编程，与实验题目一一对应。源码与对应题目说明见 `微机原理实验备考/` 目录。

### 代码与题目对应表

| 题目 | 对应.c | 对应.md | 功能说明 |
|------|--------|---------|----------|
| 0 | — | — | GPIO 初始化 + INTC 初始化 + Timer 初始化 标准模板 |
| 1 | `test1.c` | `test1.md` | LED 实时 + 全部数码管显示按键字符（C/U/L/R/D） |
| 2 | `test2.c` | `test2.md` | LED 实时 + 数码管依次显示按键字符 |
| 3 | `test3.c` | `test3.md` | 定时器 1 秒 + 数码管 1 位循环 0~F（按键切换位置） |
| 4 | `test4.c` | `test4.md` | 定时器 1 秒 + 数码管第 1 位自动递增 0~F |
| 5 | `test5.c` | `test5.md` | 开关低 4 位十六进制 + 数码管 4 位 16 进制显示 + LED 实时 |
| 6 | `test6.c` | `test6.md` | 开关 16 进制显示 + L 左移 / R 右移 |
| 7 | `test7.c` | `test7.md` | 开关 16 进制显示 + C 高 8 位 / R 低 8 位 |
| 8 | `test8.c` | `test8.md` | 开关 16 进制显示 + C 自左移 / R 自右移（移出变 0） |
| 9 | `test9.c` | `test9.md` | 开关 16 进制显示 + C 左移 / R 右移 / L 低 8 位 / U 停止 |
| 10 | `test10.c` | `test10.md` | 开关 16 进制显示 + C 最低位 / R 自动左右移循环 |
| 11 | `test11.c` | `test11.md` | 开关 16 位拆分为高 8 位左右移 + 低 8 位左右移 |
| 12 | `test12.c` | `test12.md` | 滚动显示"3456" + C 开始 / L 左移 / R 右移 / U 停止 |
| **2026.6.15** | `2026.6题目&代码/test_2026_6_15.c` | `2026.6题目&代码/2026_6_15题目.md` | **期末考试题**：BTNC→LED低8位显示1111 0000；BTNL→三态循环：1Hz左移/1Hz右移/停止 |

---

## 期末备考

> 📁 目录：`微机原理实验备考/`

### 备考文档

| 文件 | 说明 |
|------|------|
| `微机原理实验备考文档.md` | 综合备考总结：硬件平台清单、共阳段码表/位码表、全局变量模板、中断初始化模板、ISR 标准结构、核心 C 语言逻辑、常见 BUG 汇总、考试建议优先级 |
| `微机原理实验题目.md` | 12 道实验题目完整说明（含功能要求、设备清单、评分要点、步骤提示） |
| `template.c` | 标准代码模板（含 GPIO/INTC/Timer 初始化完整代码框架） |

### 题目源码与说明（12 题 + 2026.6.15 期末题）

每题均配有 `.c` 源码和 `.md` 题目说明：

| 题号 | 源码 | 题目说明 | 难度 |
|------|------|---------|------|
| 1 | `test1.c` | `test1.md` | ★★☆ |
| 2 | `test2.c` | `test2.md` | ★★☆ |
| 3 | `test3.c` | `test3.md` | ★★★ |
| 4 | `test4.c` | `test4.md` | ★★☆ |
| 5 | `test5.c` | `test5.md` | ★☆☆ |
| 6 | `test6.c` | `test6.md` | ★★☆ |
| 7 | `test7.c` | `test7.md` | ★★☆ |
| 8 | `test8.c` | `test8.md` | ★★★ |
| 9 | `test9.c` | `test9.md` | ★★★ |
| 10 | `test10.c` | `test10.md` | ★★★ |
| 11 | `test11.c` | `test11.md` | ★★★ |
| 12 | `test12.c` | `test12.md` | ★★★★ |
| **2026.6.15** | `2026.6题目&代码/test_2026_6_15.c` | `2026.6题目&代码/2026_6_15题目.md` | ★★★★（期末考试题） |

### 备考要点速览

- **硬件设备**：GPIO_0（开关 16 位输入 + LED 16 位输出）、GPIO_1（位选 8 位输出 + 段码 8 位输出）、GPIO_2（按键 5 位输入）、Timer_0（定时器）、INTC_0（中断控制器）
- **段码/位码**：共阳极数码管段码 0~F 完整表 + "3 位相同"助记口诀、位码 0x7F~0xFE
- **中断系统**：GPIO 中断使能流程、Timer 初始化流程、INTC 初始化流程、My_ISR 标准结构
- **核心操作**：开关读取、按键消抖（延时+再次确认）、动态扫描、十六进制拆分、移位操作、慢速计数器
- **2026.6.15 期末新增**：BTNC 静态设置 + BTNL 三态循环（`count%3` 状态机控制左移/右移/停止），LED 整体循环移位（`Led_current` 变量维护）

---

## 目录结构速览

```
d:/Vivado/Vitis/Projects/
├── README.md                          # 本文件（总览索引）
│
├── 并行IO设计说明/                     # 并行 IO 设计文档
│   ├── readme.md                      # 三种驱动方式总览与对比
│   ├── 三种代码设计思路对比.docx       # 轮询/普通中断/快速中断对比
│   ├── CPU_INT_TIMER_关键头文件总结.docx
│   ├── TASK_FAST_INT_流程图_Mermaid.txt
│   ├── TASK_INT_0_流程图_Mermaid.txt
│   └── TASK_NOINT_流程图_Mermaid.txt
│
├── 串行IO设计/                        # 串行 IO 设计文档
│   ├── readme.md                      # UART 硬件升级概要
│   └── CPU_INT_TIMER到CPU_UART硬件升级说明.md
│
├── 微机原理实验备考/                   # 备考资料
│   ├── 微机原理实验备考文档.md          # 综合备考总结
│   ├── 微机原理实验题目.md             # 12 道实验题目
│   ├── template.c                     # 标准代码模板
│   ├── test1.c ~ test12.c             # 12 道题目源码
│   ├── test1.md ~ test12.md           # 12 道题目说明
│   ├── 2026.6题目&代码/                # 2026.6期末考试题
│   │   ├── 2026_6_15题目.md            # 题目描述
│   │   ├── test_2026_6_15.c            # 参考答案源码
│   │   └── 实验报告.md                 # 实验报告
│
├── TASK_NOINT_/                       # 轮询方式
│   └── readme.md                      # 详细说明（含消影原理分析）
│
├── TASK_INT_0/                        # 普通中断方式
│   └── readme.md                      # 详细说明（含 Vivado 配置要求）
│
├── TASK_FAST_INT/                     # 快速中断方式
│   └── readme.md                      # 详细说明（含 IVAR/IMR 配置）
│
├── UART/                              # 双 UART 通信
│   └── readme.md                      # 详细说明（含通信协议）
│
├── CPU_noint/                         # 硬件平台：无中断
├── CPU_INT/                           # 硬件平台：基础中断
├── CPU_INT_TIMER/                     # 硬件平台：增强中断
├── CPU_UART/                          # 硬件平台：UART
├── CPU_SPI/                           # 硬件平台：SPI
├── CPU_qq/                            # 应用：GPIO 简易测试
├── Test/                              # 应用：多功能显示测试
│
└── Test/src/                          # 测试代码
    └── test.c