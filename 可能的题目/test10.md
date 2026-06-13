# Test10 — 开关16进制显示 + C最低位 / R自动左右移

## 题目描述
LED实时显示开关状态，数码管前4位以16进制显示开关值。按C键所有数码管显示十六进制值的最低位（如0x1A3B显示"BBBB"），按R键自动左右移循环：先右移移到0，再左移回到原值，反复循环。

## 硬件平台
CPU_INT_TIMER

## 核心代码逻辑

### 1. C键最低位显示
```c
unsigned char lo = display_val & 0xF;
for (int i = 0; i < 4; i++)
    segcode[i] = segtable[lo];
```

### 2. R键自动左右移循环
```c
int auto_dir = 0;  // 0=右移，1=左移

if (auto_dir == 0) {
    shift_val >>= 1;
    if (shift_val == 0) auto_dir = 1;  // 切换为左移
} else {
    shift_val = (shift_val << 1) & 0xFFFF;
    if (shift_val >= display_val) {
        shift_val = display_val;
        auto_dir = 0;  // 切换为右移
    }
}
```

### 3. 速度控制
使用 `shift_slow` 计数器，每累积1000次定时器中断（约1秒）移动一位。