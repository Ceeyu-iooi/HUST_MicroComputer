# Test3 — 定时器1秒 + 数码管1位循环0~F

## 题目描述
LED灯实时显示开关状态。不按任何键时，数码管第1位自动循环显示0~F（每秒变化）。按C键时数码管第2位开始循环，按L键恢复第1位循环，按R键第3位循环。

## 硬件平台
CPU_INT_TIMER

## 中断设备
同test1

## 核心代码逻辑

### 1. 慢速计数器
```c
int slow_count = 0;
int digit_val = 0;       // 当前显示值 0~F
int active_digit = 0;    // 当前活动位 0~7
```

### 2. 计时逻辑（timer_handle 每秒更新）
```c
slow_count++;
if (slow_count >= 1000) {  // 1000次1ms中断 ≈ 1s
    slow_count = 0;
    digit_val = (digit_val + 1) % 16;
}
// 始终在 active_digit 位置显示 digit_val 的段码
segcode[active_digit] = segtable[digit_val];
```

### 3. 按键切换活动位
- C键：`active_digit = 1`
- L键：`active_digit = 0`
- R键：`active_digit = 2`