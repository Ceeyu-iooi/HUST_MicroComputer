# Test2 — LED实时 + 数码管依次显示按键字符

## 题目描述
拨动16位开关，LED灯实时显示开关状态。每按一次C/U/L/R/D键，数码管从左到右依次显示该字符。

## 硬件平台
CPU_INT_TIMER

## 中断设备
同test1

## 核心代码逻辑

### 1. 依次显示逻辑
在 button_handle 中检测按键，将当前字符（段码）存入一个显示队列。使用 `display_pos` 变量记录当前要显示到的数码管位置（0~7）。每按一次键，将对应segment存入 `segcode[display_pos]`，然后 `display_pos++`。

### 2. 按键处理代码示例
```c
if (btn & 0x01) {  // C键
    segcode[display_pos] = seg_char('C');  // 0xC6
    display_pos++;
    if (display_pos >= 8) display_pos = 0;
}
```

### 3. 注意
LED实时刷新在 switch_handle 中，数码管动态扫描在 timer_handle 中，这些与其他题目一致。