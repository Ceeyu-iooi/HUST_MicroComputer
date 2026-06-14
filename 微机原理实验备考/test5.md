# 题目5：按键切换显示模式 + 定时器递增（双定时器中断）

## 题目要求
- 点击按键 BTNC（bit0）：数码管最右边2位以 **16进制** 显示最右边8位独立开关的值，高6位熄灭
- 点击按键 BTNL（bit2）：数据 `disp_value` 从0开始，**每1秒加1**，加至 **255（即FF）** 后回0，继续递增，数码管最右边2位实时显示当前值的16进制
- 使用 **两个定时器**：
  - **T0**：数码管动态扫描（约10μs）
  - **T1**：1秒定时递增（RESET_VALUE1 = 100,000,000 - 2 ≈ 1s）

## 硬件资源
| 外设 | GPIO | 方向 |
|------|------|------|
| 独立开关(16位) | GPIO0_CH1 | 输入 |
| LED灯(16位) | GPIO0_CH2 | 输出 |
| 数码管位选(8位) | GPIO1_CH1 | 输出 |
| 数码管段码(8位) | GPIO1_CH2 | 输出 |
| 独立按键(5位) | GPIO2_CH1 | 输入 |

## 中断架构
| 中断源 | 掩码 | 作用 |
|--------|------|------|
| GPIO2（按键） | 0x2 | 检测按键按下，切换显示模式 |
| TIMER0 T0 | 0x4（合并） | 数码管动态扫描 |
| TIMER0 T1 | （同一中断线） | 每秒递增计数 |

## 核心实现逻辑

### 1. 双定时器初始化（T0 数码管扫描 + T1 1秒递增）
```c
// T0：数码管扫描（~10μs）
Xil_Out32(TIMER_BASE + XTC_TCSR_OFFSET, ... | XTC_CSR_ENABLE_TMR_MASK);
Xil_Out32(TIMER_BASE + XTC_TLR_OFFSET, RESET_VALUE0);  // 1000-2

// T1：1秒定时递增
Xil_Out32(TIMER_BASE + XTC_TIMER_COUNTER_OFFSET + XTC_TCSR_OFFSET, ...);
Xil_Out32(TIMER_BASE + XTC_TIMER_COUNTER_OFFSET + XTC_TLR_OFFSET, RESET_VALUE1);
```

### 2. 按键中断处理（切换模式）
```c
void button_handle(void) {
    char button = Xil_In8(GPIO2_BASE + XGPIO_DATA_OFFSET) & 0x1f;
    unsigned short sw = Xil_In16(GPIO0_BASE + XGPIO_DATA_OFFSET);
    unsigned char low8 = sw & 0xFF;

    if (button & 0x01) {        // BTNC：显示开关值（16进制）
        mode = 0;
        // 熄灭高6位
        segcode[6] = segtable_hex[(low8 >> 4) & 0xF];  // 高4位
        segcode[7] = segtable_hex[low8 & 0xF];          // 低4位
    }
    else if (button & 0x04) {   // BTNL：启动递增模式
        mode = 1;
        disp_value = 0;         // 从0开始
        segcode[6] = segtable_hex[0];
        segcode[7] = segtable_hex[0];
    }
}
```

### 3. T1 定时器中断（每秒递增）
```c
void timer1_handle(void) {
    if (mode == 1) {
        disp_value++;
        if (disp_value > 255) {  // 加到FF回0
            disp_value = 0;
        }
        // 更新数码管显示（最右边2位16进制）
        segcode[6] = segtable_hex[(disp_value >> 4) & 0xF];
        segcode[7] = segtable_hex[disp_value & 0xF];
    }
    // 清除T1中断标志
}
```

### 4. T0 定时器中断（数码管扫描）
```c
void timer0_handle(void) {
    // 消影
    Xil_Out8(GPIO1_BASE + XGPIO_DATA_OFFSET,  0xff);
    Xil_Out8(GPIO1_BASE + XGPIO_DATA2_OFFSET, 0xff);
    
    // 输出当前位
    Xil_Out8(GPIO1_BASE + XGPIO_DATA_OFFSET,  poscode[pos]);
    Xil_Out8(GPIO1_BASE + XGPIO_DATA2_OFFSET, segcode[pos]);
    
    pos++;
    if (pos == 8) pos = 0;
}
```

### 5. 主中断分发
```c
void My_ISR() {
    int status = Xil_In32(INTC_BASE + XIN_ISR_OFFSET);
    
    if ((status & GPIO_2_IRQ_MASK) == GPIO_2_IRQ_MASK)
        button_handle();
    if ((status & TIMER_0_IRQ_MASK) == TIMER_0_IRQ_MASK)
        timer_handle();  // 内部判断T0/T1

    Xil_Out32(INTC_BASE + XIN_IAR_OFFSET, status);
}
```

## 定时器时间计算
| 定时器 | 时钟频率 | 计数值 | 中断周期 |
|--------|----------|--------|----------|
| T0 | 100MHz（10ns） | (1000-2) × 10ns ≈ 10μs | 数码管扫描 |
| T1 | 100MHz（10ns） | (100,000,000-2) × 10ns = 1s | 递增计数 |

## 关键点
- **双定时器共用一个 TIMER IP**：T0 使用主计数器，T1 使用级联计数器（`XTC_TIMER_COUNTER_OFFSET`）
- **定时器中断共用一条中断线**（掩码 0x4），在 `timer_handle()` 内通过读取各自的 `TCSR` 寄存器区分 T0/T1
- **mode 变量**控制显示模式：`0` = 显示开关值，`1` = 递增模式
- 两种模式都使用 `segcode[6]` 和 `segcode[7]`（最右边2位）显示16进制值，高6位熄灭
- **递增到255（0xFF）回0**：`disp_value` 类型为 `unsigned char`，超过255自动回绕，加显式判断确保正确性
- 按键切换模式后立即更新数码管显示，T1递增时动态更新 `segcode`
- 数码管段码表 `segtable_hex[16]` 包含0~9和A~F共16个16进制段码