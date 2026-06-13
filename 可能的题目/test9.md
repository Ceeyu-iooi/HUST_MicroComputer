# Test9 — 开关16进制显示 + C左移/R右移/L低8位/U停止

## 题目描述
LED实时显示开关状态，数码管前4位以16进制显示开关值。按C键每按一次左移一位，按R键每按一次右移一位，按L键显示开关低8位，按U键停止（恢复原始显示）。

## 硬件平台
CPU_INT_TIMER

## 核心代码逻辑

### 1. 模式管理
```c
int mode = 0;  // 0=停止，1=低8位显示，2=移位状态
unsigned short shift_val;

// C键
if (btn & 0x01) {
    mode = 2;
    shift_val = (shift_val << 1) & 0xFFFF;
}
// R键
if (btn & 0x08) {
    mode = 2;
    shift_val >>= 1;
}
// L键
if (btn & 0x04) {
    mode = 1;
    shift_val = display_val;  // 低8位
}
// U键
if (btn & 0x02) {
    mode = 0;
    shift_val = display_val;
}
```

### 2. 显示切换（timer_handle）
```c
if (mode == 0) val = display_val;
else if (mode == 1) val = (display_val & 0x00FF);
else if (mode == 2) val = shift_val;
// 再做4位拆分显示
```

### 3. LED同步
LED始终跟随当前显示值更新。