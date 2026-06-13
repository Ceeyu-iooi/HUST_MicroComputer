# Test8 — 开关16进制显示 + C自左移/R自右移

## 题目描述
LED实时显示开关状态，数码管前4位以16进制显示开关值。按C键显示值自动左移从原值移出变为0，按R键显示值自动右移从原值移出变为0。

## 硬件平台
CPU_INT_TIMER

## 核心代码逻辑

### 1. 自动移位
```c
int mode = 0;              // 0=停止，1=C自动左移，2=R自动右移
unsigned short shift_val;
int shift_slow = 0;        // 慢速计数器

// timer_handle 中
if (mode == 1) {
    if (shift_slow++ >= 1000) {  // 每秒移1位
        shift_slow = 0;
        shift_val <<= 1;         // 左移
    }
} else if (mode == 2) {
    if (shift_slow++ >= 1000) {
        shift_slow = 0;
        shift_val >>= 1;         // 右移
    }
}
```

### 2. 按键切换
- C键：`mode = 1`，`shift_val = display_val`
- R键：`mode = 2`，`shift_val = display_val`

### 3. 注意
移位后LED也同步更新，移出后变为0。