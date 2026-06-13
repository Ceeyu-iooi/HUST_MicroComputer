# Test11 — 开关高8位LED显示 + R右移

## 题目描述
LED显示开关的高8位（开关低8位LED不亮），数码管前4位以16进制显示开关值。按R键数码管显示值自动左移（实际上是右移），LED也跟随移位。

## 硬件平台
CPU_INT_TIMER

## 核心代码逻辑

### 1. 高8位LED显示
```c
unsigned short hi = display_val & 0xFF00;
Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET, hi);
```

### 2. R键自动右移
```c
if (btn & 0x08) {
    mode = 1;
    shift_val = display_val;
    shift_slow = 0;
}

// timer_handle 中
if (mode == 1) {
    if (shift_slow++ >= 1000) {
        shift_slow = 0;
        shift_val >>= 1;
    }
    Xil_Out16(..., shift_val & 0xFF00);  // LED同步高8位
}
```

### 3. 注意
- 默认模式下LED只亮高8位（0xFF00 掩码）
- 移位后LED也同步显示高8位
- 数码管始终显示完整的16进制值