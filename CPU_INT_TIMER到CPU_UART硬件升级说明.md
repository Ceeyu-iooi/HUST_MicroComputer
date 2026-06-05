# CPU_INT_TIMER → CPU_UART Vivado 硬件设计升级说明

本文档说明从 `CPU_INT_TIMER` 平台升级到 `CPU_UART` 平台所需在 Vivado Block Design 中添加的外设及配置变更。

---

## 一、概述

| 平台 | 说明 |
|------|------|
| CPU_INT_TIMER | 基础中断平台：开关、按键、数码管、定时器、1个UART（USB调试） |
| CPU_UART | 在 CPU_INT_TIMER 基础上增加：2个UART IP核（UART1通信用）+ SPI外设 |

---

## 二、中断控制器（AXI Interrupt Controller）配置修改

### 2.1 增加中断输入数量

| 参数 | CPU_INT_TIMER（原值） | CPU_UART（新值） |
|------|----------------------|-------------------|
| `num-intr-inputs` | 3 | **6** |

### 2.2 中断输入分配

| 中断号 | 连接外设 | 中断类型 | 说明 |
|--------|----------|----------|------|
| 0 | AXI GPIO 0（开关） | 边沿触发 | 开关拨动时产生中断 |
| 1 | AXI GPIO 2（按键） | 边沿触发 | 按键按下时产生中断 |
| 2 | AXI Timer 0 | 电平触发 | 定时器中断，数码管动态扫描 |
| 3 | **AXI UARTLite 1（新增）** | 电平触发 | UART1接收中断 |
| 4 | **AXI UARTLite 2（新增）** | 电平触发 | UART2接收中断 |
| 5 | **AXI Quad SPI 0（新增）** | 边沿触发 | SPI外设中断 |

### 2.3 中断类型配置

| 参数 | CPU_INT_TIMER | CPU_UART | 说明 |
|------|:---:|:---:|------|
| `kind-of-intr` | 0x0 | **0x38** | 中断级别类型位掩码（对应中断输入3/4/5为电平触发） |
| `irq-is-level` | 1 | **0** | 中断总线编号（0=Intr端口，1=Irq端口） |
| `async-intr` | 0xFFFFFFF8 | **0xFFFFFFE0** | 异步中断使能掩码（对应位为0表示使能该输入） |

> **注意**：在 Vivado Block Design 中双击 AXI Interrupt Controller IP，在 "Interrupt Inputs" 选项卡中将 Number of Interrupt Inputs 改为 **6**，然后重新连接中断信号。`kind-of-intr` 等底层参数会自动由工具生成。

---

## 三、新增 AXI UARTLite（UART1 / UART2）

### 3.1 添加 IP 核

在 Block Design 中通过 "+" → IP Catalog 搜索 `AXI UARTLite`，添加 **两个** 实例。

### 3.2 UART1 参数配置

| 参数 | 值 |
|------|-----|
| Name | `axi_uartlite_1` |
| 基地址 | `0x40610000` |
| 波特率 | **9600** |
| 数据位 | 8 |
| 校验位 | 无（No Parity） |
| 中断 | **使能（Enable Interrupt）** |
| Board Interface | Custom（自定义） |

### 3.3 UART2 参数配置

| 参数 | 值 |
|------|-----|
| Name | `axi_uartlite_2` |
| 基地址 | `0x40620000` |
| 波特率 | **9600** |
| 数据位 | 8 |
| 校验位 | 无（No Parity） |
| 中断 | **使能（Enable Interrupt）** |
| Board Interface | Custom（自定义） |

### 3.4 引脚约束（XDC）

UART1 和 UART2 的 TX/RX 引脚需要在 XDC 约束文件中映射到 Nexys4 DDR 的 Pmod 或 GPIO 接口。根据设计要求：

```
# UART1 TX → PMOD JB pin1, UART1 RX → PMOD JB pin2
set_property PACKAGE_PIN E15 [get_ports axi_uartlite_1_tx]
set_property PACKAGE_PIN E16 [get_ports axi_uartlite_1_rx]
set_property IOSTANDARD LVCMOS33 [get_ports axi_uartlite_1_tx]
set_property IOSTANDARD LVCMOS33 [get_ports axi_uartlite_1_rx]

# UART2 TX → PMOD JB pin3, UART2 RX → PMOD JB pin4
set_property PACKAGE_PIN D15 [get_ports axi_uartlite_2_tx]
set_property PACKAGE_PIN C16 [get_ports axi_uartlite_2_rx]
set_property IOSTANDARD LVCMOS33 [get_ports axi_uartlite_2_tx]
set_property IOSTANDARD LVCMOS33 [get_ports axi_uartlite_2_rx]
```

> **外部连接**：上板后将用杜邦线按以下方式连接：
> - UART1 TX → UART2 RX
> - UART1 RX → UART2 TX

---

## 四、新增 AXI Quad SPI

### 4.1 添加 IP 核

在 Block Design 中通过 IP Catalog 添加 `AXI Quad SPI`。

### 4.2 参数配置

| 参数 | 值 |
|------|-----|
| Name | `axi_quad_spi_0` |
| 基地址 | `0x44A00000` |
| 模式 | 主模式（Master） |
| 数据位宽 | 16 bits |
| FIFO深度 | 16 |
| 中断 | **使能** |
| Board Interface | Custom |

> 该 SPI 外设用于 Pmod OLED 显示屏或其他 SPI 设备。

---

## 五、Block Design 连线汇总

完成以上 IP 添加后的连线清单：

```
                    ┌──────────────────┐
                    │  AXI Interconnect │
                    └──┬───┬───┬───┬───┘
                       │   │   │   │
        ┌──────────────┤   │   │   ├──────────────┐
        │              │   │   │   │              │
   ┌────▼────┐  ┌──────▼───▼───▼───▼──────┐  ┌───▼────┐
   │ GPIO_0  │  │     AXI INTC (6ch)      │  │ Timer  │
   │开关+LED │  │  int0←GPIO0(开关)       │  │ #2 INTC│
   │#0 INTC  │  │  int1←GPIO2(按键)       │  └────────┘
   └─────────┘  │  int2←Timer             │
                │  int3←UART1(新增)       │
   ┌─────────┐  │  int4←UART2(新增)       │
   │ GPIO_1  │  │  int5←SPI(新增)         │
   │位选+段码│  └───────────┬─────────────┘
   └─────────┘              │ INTC → MicroBlaze INTERRUPT
                ┌───────────┼───────────┐
   ┌─────────┐  │           │           │
   │ GPIO_2  │  │  ┌────────▼──────┐    │    ┌──────────┐
   │ 按键    │  │  │ UARTLite_1   │    │    │ SPI(新增)│
   │#1 INTC  │  │  │ 新增 #3 INTC │    │    │ #5 INTC  │
   └─────────┘  │  └───────────────┘    │    └──────────┘
                │                       │
                │  ┌────────────────┐    │    ┌──────────┐
                │  │ UARTLite_0    │    │    │ .......  │
                │  │ (USB UART)    │    │    │ .......  │
                │  │ 无中断        │    │    │ .......  │
                │  └────────────────┘    │    └──────────┘
                │                       │
                │  ┌────────────────┐    │
                └──│ UARTLite_2    │────┘
                   │ 新增 #4 INTC  │
                   └────────────────┘
```

---

## 六、地址空间分配

| 外设 | 基地址 | 地址范围 |
|------|--------|----------|
| MicroBlaze LMB BRAM | 0x0000_0000 | 32KB |
| AXI GPIO 0（开关+LED） | 0x4000_0000 | 64KB |
| AXI GPIO 1（位选+段码） | 0x4001_0000 | 64KB |
| AXI GPIO 2（按键） | 0x4002_0000 | 64KB |
| AXI UARTLite 0（USB UART） | 0x4060_0000 | 64KB |
| **AXI UARTLite 1（新增）** | **0x4061_0000** | 64KB |
| **AXI UARTLite 2（新增）** | **0x4062_0000** | 64KB |
| **AXI Quad SPI 0（新增）** | **0x44A0_0000** | 64KB |
| AXI Interrupt Controller | 0x4120_0000 | 64KB |
| AXI Timer 0 | 0x41C0_0000 | 64KB |

---

## 七、操作步骤总结

1. 打开 CPU_INT_TIMER 的 Vivado 工程，另存为 CPU_UART 工程
2. 在 Block Design 中双击 `axi_intc_0`，将中断输入数从 3 改为 **6**，点击 "Run Connection Automation"
3. 添加 **AXI UARTLite** IP（两个实例），配置波特率为 9600，使能中断
4. 添加 **AXI Quad SPI** IP（如需），使能中断
5. 运行 Connection Automation 自动连接 AXI 总线、时钟、复位和中断信号
6. 手动验证中断线连接：
   - UARTLite_1 的 `interrupt` → INTC 的 `intr[3]`
   - UARTLite_2 的 `interrupt` → INTC 的 `intr[4]`
   - SPI 的 `ip2intc_irpt` → INTC 的 `intr[5]`
7. 在 XDC 约束文件中添加 UART1/UART2 的引脚约束
8. 生成 Bitstream，导出 XSA 文件
9. 在 Vitis 中创建新的平台项目，导入 XSA 文件