# Test12：8位7段数码管显示数字序列3456，按键控制滚动方向

## 功能概述

在8位7段共阳极数码管上显示数字序列 **"3456"**（没有数字的位置全部熄灭 0xff），通过5个独立按键控制滚动行为（按C→U→L→R→D顺序判断）：

| 按键 | 掩码 | Bit位 | 功能 |
|------|------|-------|------|
| **C** | 0x01 | bit0 | 开始自动向左滚动（T1驱动，每秒1位） |
| **U** | 0x02 | bit1 | 预留（当前无功能） |
| **L** | 0x04 | bit2 | 手动循环左移1位（按下立即左移1位） |
| **R** | 0x08 | bit3 | 开始自动向右滚动（T1驱动，每秒1位） |
| **D** | 0x10 | bit4 | 预留（当前无功能） |

初始状态：最左4位熄灭，最右4位显示"3456"，静止。按下C键后开始自动向左滚动。

## 硬件平台

| 外设 | 说明 |
|------|------|
| AXI GPIO 0 | CH1=16位独立开关（输入），CH2=16位LED灯（输出，本测试未使用） |
| AXI GPIO 1 | CH1=8位数码管位选（输出），CH2=8位数码管段码（输出） |
| AXI GPIO 2 | CH1=5位独立按键（输入），支持中断 |
| AXI Timer 0 | T0通道=数码管动态扫描定时器（约10μs），T1通道=滚动节奏定时器（1s） |
| 中断控制器 | GPIO2按键中断（0x2）+ Timer0中断（0x4） |

## 宏定义

```c
/* 按键掩码（按C→U→L→R→D的硬件bit位序） */
#define BTNC_MASK  0x01    // bit0: 中间键 C
#define BTNU_MASK  0x02    // bit1: 上键 U
#define BTNL_MASK  0x04    // bit2: 左键 L
#define BTNR_MASK  0x08    // bit3: 右键 R
#define BTND_MASK  0x10    // bit4: 下键 D

/* 滚动方向 */
#define SCROLL_LEFT  0     // 向左滚动
#define SCROLL_RIGHT 1     // 向右滚动
```

## 中断配置

| 中断源 | 中断掩码 | 功能 | 中断周期 |
|--------|---------|------|---------|
| Timer0 (T0) | 0x4 | 数码管动态扫描 | 约10μs |
| Timer1 (T1) | 0x4（与T0共用同一根中断线） | 滚动控制 | 1s |
| GPIO2 (按键) | 0x2 | 按键检测（C/U/L/R/D） | 按键按下/松开时触发 |

> T0和T1是同一个AXI Timer IP核的两个独立通道，共享同一根中断线（TIMER_0_IRQ_MASK=0x4）。在 `My_ISR` 中通过 `timer_handle()` 分发函数分别读取两个通道的TCSR寄存器来区分。

## 按键处理逻辑（C→U→L→R→D顺序）

```c
void button_handle(void)
{
    char button = Xil_In8(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_DATA_OFFSET) & 0x1f;

    /* 按键松开：仅清除中断标志后返回 */
    if ((button & 0x1f) == 0)
    {
        Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET, ...);
        return;
    }

    /* C(bit0)：开始自动向左滚动 */
    if (button & BTNC_MASK)
        mode = SCROLL_LEFT;

    /* U(bit1)：预留 */
    else if (button & BTNU_MASK)
        /* 未分配功能 */

    /* L(bit2)：手动循环左移1位（立即执行） */
    else if (button & BTNL_MASK)
    {
        char temp = segcode[0];
        for (int i = 0; i < 7; i++)
            segcode[i] = segcode[i + 1];
        segcode[7] = temp;
    }

    /* R(bit3)：开始自动向右滚动 */
    else if (button & BTNR_MASK)
        mode = SCROLL_RIGHT;

    /* D(bit4)：预留 */
    else if (button & BTND_MASK)
        /* 未分配功能 */

    /* 清除GPIO2中断标志 */
    Xil_Out32(...);
}
```

**关键区别：**
- **BTNC**（bit0）：切换 `mode = SCROLL_LEFT`，T1每秒自动向左滚动
- **BTNL**（bit2）：按下即刻左移1位（在按键中断中直接操作segcode），不改变mode
- **BTNR**（bit3）：切换 `mode = SCROLL_RIGHT`，T1每秒自动向右滚动
- **BTNU/BTND**：预留，可后续扩展

## 自动滚动方向控制（T1中断驱动）

```c
void timer1_handle(void)
{
    if (mode == SCROLL_LEFT)      // 自动向左滚动
    {
        char temp = segcode[0];
        for (int i = 0; i < 7; i++)
            segcode[i] = segcode[i + 1];
        segcode[7] = temp;
    }
    else if (mode == SCROLL_RIGHT) // 自动向右滚动
    {
        char temp = segcode[7];
        for (int i = 0; i < 7; i++)
            segcode[7 - i] = segcode[6 - i];
        segcode[0] = temp;
    }
    /* 清除T1中断标志 */
}
```

### 滚动时序示例（向左）

```
t=0： [ff][ff][ff][ff][3][4][5][6]   ← 初始状态（静止）
按下 C 键后开始自动向左滚动：
t=1s：[ff][ff][ff][3][4][5][6][ff]   ← 向左移1位
t=2s：[ff][ff][3][4][5][6][ff][ff]
t=3s：[ff][3][4][5][6][ff][ff][ff]
...
```

### 滚动时序示例（向右）

```
按下 R 键切换为自动向右滚动：
t=0s： [ff][3][4][5][6][ff][ff][ff]
t=1s： [ff][ff][3][4][5][6][ff][ff]  ← 数字整体向右移
t=2s： [ff][ff][ff][3][4][5][6][ff]
...
```

> **右移注意事项**：必须从右向左遍历（`segcode[7]=segcode[6]` → `segcode[6]=segcode[5]` → ...），否则会因覆盖导致数据丢失。

## 初始化时序

1. 配置所有GPIO方向寄存器
2. 使能GPIO2按键中断（清除标志 → 使能通道 → 使能全局）
3. 初始化T0定时器（10μs中断 → 数码管动态扫描）
4. 初始化T1定时器（1s中断 → 滚动节奏控制）
5. 初始化中断控制器（清除标志 → 使能中断源 → 主使能+硬件使能）
6. 使能CPU全局中断（`microblaze_enable_interrupts()`）
7. 设置初始显示内容："3456"在最右边4位，其余全灭

## 中断响应流程

```
GPIO2按键按下           T0溢出(~10μs)          T1溢出(1s)
    ↓                       ↓                     ↓
INTC(IRQ=0x2)          INTC(IRQ=0x4) ←──── 共享中断线
    ↓                       ↓
My_ISR() ←─────────────────┴─────────────────────┘
    ├── status & GPIO_2_IRQ_MASK → button_handle()
    │       ├── 读取按键值 button
    │       ├── button==0？ → 直接返回（按键松开忽略）
    │       ├── BTNC(bit0) → mode=SCROLL_LEFT（自动左滚动）
    │       ├── BTNU(bit1) → 预留
    │       ├── BTNL(bit2) → segcode手动左移1位（immediate）
    │       ├── BTNR(bit3) → mode=SCROLL_RIGHT（自动右滚动）
    │       └── BTND(bit4) → 预留
    │
    └── status & TIMER_0_IRQ_MASK → timer_handle()
            ├── T0.TCSR有INT_OCCURED? → timer0_handle()
            │       ├── 消影（位选+段码全灭）
            │       ├── 输出 poscode[pos] + segcode[pos]
            │       ├── pos++（循环0~7）
            │       └── 清T0中断标志
            │
            └── T1.TCSR有INT_OCCURED? → timer1_handle()
                    ├── mode==SCROLL_LEFT? → 循环左移
                    └── mode==SCROLL_RIGHT? → 循环右移（反向遍历）
                    └── 清T1中断标志
```

## 关键寄存器配置

### T0 定时器（数码管扫描）
| 寄存器 | 初始值 | 说明 |
|--------|--------|------|
| TLR0 | 1000 - 2 | 约10μs（100MHz时钟） |
| TCSR0 | 使能中断 + 自动重载 + 减计数 + 使能 | 持续产生定时中断 |

### T1 定时器（滚动节奏）
| 寄存器 | 初始值 | 说明 |
|--------|--------|------|
| TLR1 | 100000000 - 2 | 约1s（100MHz时钟） |
| TCSR1 | 使能中断 + 自动重载 + 减计数 + 使能 | 每秒触发一次滚动 |

### 中断控制器
| 寄存器 | 写入值 | 说明 |
|--------|--------|------|
| IAR | 0x2 \| 0x4 = 0x6 | 清除按键和定时器中断标志 |
| IER | 0x2 \| 0x4 = 0x6 | 使能按键和定时器中断 |
| MER | Master Enable \| Hardware Enable | 中断控制器全局使能 |

### GPIO2 按键中断配置
| 寄存器 | 写入值 | 说明 |
|--------|--------|------|
| ISR | XGPIO_IR_CH1_MASK | 清除中断标志 |
| IER | XGPIO_IR_CH1_MASK | 使能CH1中断 |
| GIE | XGPIO_GIE_GINTR_ENABLE_MASK | 使能GPIO全局中断 |

## GPIO 方向配置

| GPIO | 通道 | 方向 | 数据宽度 | 功能 |
|------|------|------|---------|------|
| GPIO0 | CH1 | 输入 | 16位 | 独立开关（未使用） |
| GPIO0 | CH2 | 输出 | 16位 | LED灯（未使用） |
| GPIO1 | CH1 | 输出 | 8位 | 数码管位选 |
| GPIO1 | CH2 | 输出 | 8位 | 数码管段码 |
| GPIO2 | CH1 | 输入 | 5位 | 独立按键（C/U/L/R/D） |

## 与 test8 的差异

| 特性 | test8 | test12 |
|------|-------|--------|
| 滚动方式 | 固定向左自动滚动 | 按键控制方向（C/L/R） |
| 初始状态 | 自动开始滚动 | 静止，按C后才开始 |
| 滚动方向 | 仅左移 | 左移（C键/自动 或 L键/手动）或右移（R键/自动） |
| 按键中断 | 使能但未使用（空函数） | 完全实现（C/U/L/R/D控制） |
| `button_handle` | 仅清中断标志 | 读取按键值，切换mode或执行手动移位 |
| 按键位序 | 未按CULRD排列 | 严格C→U→L→R→D顺序判断 |

## 常见问题

**Q：按键按下后数码管滚动方向没变？**
A：检查（1）GPIO2的TRI方向寄存器是否配置为输入（写0x1f）；（2）按键掩码是否正确（C=0x01, U=0x02, L=0x04, R=0x08, D=0x10）；（3）`button_handle` 中是否忽略了 `button==0` 的情况（按键松开也会触发中断）。

**Q：按一次键滚动方向切换了多次？**
A：按键机械抖动可能导致多次触发中断。若抖动严重，可在 `button_handle()` 中添加软件延时去抖，或仅在按键值发生边沿变化（按下瞬间）时才切换方向。

**Q：数码管不显示任何内容？**
A：检查（1）GPIO1的TRI方向寄存器是否配置为输出（写0）；（2）segtable_hex[3~6] 是否对应正确的段码；（3）T0定时器是否工作（RESET_VALUE0=1000-2）。

**Q：向右滚动时数字显示异常（出现相同的重复数字）？**
A：循环右移时必须从右向左遍历，即先处理 `segcode[7]=segcode[6]`，再 `segcode[6]=segcode[5]`，以此类推。如果从左向右遍历会导致所有位被 `segcode[0]` 覆盖。