# Test6 — 开关16进制显示 + L左移/R右移

## 题目描述
LED实时显示开关状态，数码管前4位以16进制显示开关值。按L键数码管显示值左移一位，按R键数码管显示值右移一位。

## 硬件平台
CPU_INT_TIMER

## 核心代码逻辑

### 1. 移位逻辑
```c
unsigned short shift_val;

// L键：左移
shift_val = (display_val << 1) & 0xFFFF;

// R键：右移
shift_val = display_val >> 1;
```

### 2. 显示更新
```c
for (int i = 0; i < 4; i++)
    segcode[i] = segtable[(shift_val >> (12 - 4*i)) & 0xF];
```

### 3. LED同步
```c
Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET, shift_val);
```

### 4. 按键处理
- L键：`shift_val` 左移，更新数码管和LED
- R键：`shift_val` 右移，更新数码管和LED
- 开关拨动时 `shift_val` 重置为当前开关值