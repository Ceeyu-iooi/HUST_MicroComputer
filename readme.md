# Vitis 工作区项目总览

本工作区 (`d:/Vivado/Vitis/Projects`) 包含 MicroBlaze 嵌入式平台的 Platform 项目与 Application 项目。

## Platform 项目（硬件平台）

| 项目 | XSA 文件 | 硬件配置说明 |
|------|---------|-------------|
| `CPU_INT` | `CPU_Int_wrapper.xsa` | 基础中断平台：含 INTC（中断控制器）、AXI GPIO×3、AXI Timer。用于需要通过中断方式驱动开关/按键/数码管的场景。 |
| `CPU_INT_TIMER` | `CPU_INT_wrapper.xsa` | 增强中断平台：与 CPU_INT 硬件结构类似（含 INTC、AXI GPIO×3、AXI Timer），XSA 来源不同（来自 CPU_noInt 工程）。用于需要定时器+中断联合控制的复杂交互场景。 |
| `CPU_noint` | `CPU_noint_wrapper.xsa` | 无中断平台：仅含 AXI GPIO×3（开关、LED、按键、数码管），**无 INTC、无 AXI Timer**。适合仅用轮询方式读取外设的简单应用。 |

所有 Platform 均使用 `microblaze_0` 处理器，OS 为 `standalone`，domain 为 `standalone_microblaze_0`。

## Application 项目（软件应用）

| Application | 对应 Platform | 驱动方式 | 功能说明 |
|-------------|-------------|---------|---------|
| `CPU_qq` | `CPU_INT` | 轮询 | **最简单的 GPIO 测试程序**。主循环轮询 GPIO ISR 寄存器检测 16 位拨码开关状态变化，一旦变化则同步点亮对应 LED 并通过 UART 打印开关值。不涉及按键、数码管、定时器或中断服务程序。 |
| `TASK_FAST_INT` | `CPU_INT_TIMER` | 中断 | **完整的中断驱动交互程序**（与 TASK_INT_0 功能基本相同）。通过 INTC 统一管理三类中断：① 拨码开关中断（GPIO_0 → 点亮 LED + UART 打印）；② 按键中断（GPIO_2 → 解析按键，将字符 C/U/L/R/d 的段码滚动存入显示缓冲区）；③ 定时器中断（Timer_0 → 每 ~10µs 扫描 1 位数码管，8 位循环实现动态显示）。ISR 中按键松手检测使用忙等待（会短暂阻塞其他中断）。 |
| `TASK_INT_0` | `CPU_INT_TIMER` | 中断 | **中断驱动交互程序**。功能与 TASK_FAST_INT 基本相同（开关中断 + 按键中断 + 定时器扫描 8 位数码管），区别在于按键处理逻辑：按键松开时不做处理（直接返回），仅按键按下时处理并更新显示缓冲区。 |
| `TASK_NOINT_` | `CPU_noint` | 轮询 | **全轮询方式交互程序**。不使用任何硬件中断和定时器。主循环中通过轮询 GPIO 数据寄存器检测按键按下，软件延时循环扫描 8 位数码管，在延时间隙中轮询 GPIO ISR 寄存器检测开关变化。按键按下期间数码管停止显示（等松手后才恢复扫描）。 |

## 项目类型判断依据

- `vitis-comp.json` 中 `"type": "PLATFORM"` → 硬件平台项目
- `vitis-comp.json` 中 `"type": "HOST"` → 软件应用项目
- `platform/` 目录当前为空