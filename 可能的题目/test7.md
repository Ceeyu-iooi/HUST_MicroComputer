# Test7 — 开关16进制显示 + C高8位/R低8位

## 题目描述
LED实时显示开关状态，数码管前4位以16进制显示开关值。按C键切换为显示开关高8位（低8位为0的16进制），按R键切换为显示开关低8位（高8位为0的16进制）。

## 硬件平台
CPU_INT_TIMER

## 核心代码逻辑

### 1. 模式切换
```c
int mode = 0;  // 0=完整16位，1=C键高8位，2=R键低8位

// 按键处理
if (btn & 0x01) {      // C键
    mode = 1;
    display_val = sw;  // 高8位
}
else if (btn & 0x08) { // R键
    mode = 2;
    display_val = sw;  // 低8位
}
```

### 2. 显示切换（timer_handle）
```c
if (mode == 1) {
    // 显示高8位，低8位设为0
    val = (display_val & 0xFF00);
} else if (mode == 2) {
    // 显示低8位，高8位设为0
    val = (display_val & 0x00FF);
}
// 然后再做4位十六进制拆分
```

### 3. 注意
LED始终保持完整16位开关状态显示。