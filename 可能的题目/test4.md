# Test4 — 定时器1秒 + 单数码管递增显示

## 题目描述
数码管第1位自动循环显示0~F（每秒变化）。不需按键和开关。

## 硬件平台
CPU_INT_TIMER

## 中断设备
- GPIO_0 通道2（16位输出，LED）：未使用但初始化
- GPIO_1（8位输出，位码 + 段码）：数码管动态扫描
- Timer_0 T0：数码管动态扫描定时

## 核心代码逻辑

### 1. 1秒计数
```c
int slow_count = 0;
int digit_val = 0;
```

### 2. timer_handle 中每秒更新
```c
slow_count++;
if (slow_count >= 1000) {
    slow_count = 0;
    digit_val = (digit_val + 1) % 16;
}
segcode[0] = segtable[digit_val];
// 其余位全灭
for (int i = 1; i < 8; i++) segcode[i] = SEG_OFF;
```

### 3. 简化版
此题目不需要开关中断和按键中断，只需定时器中断驱动数码管动态扫描。