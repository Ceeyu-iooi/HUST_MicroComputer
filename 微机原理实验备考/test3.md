# 题目3：8位数字显示（普通中断方式）

## 题目要求
- 使用普通中断方式
- 8个七段数码管同时显示同一数字（0~F）
- 按键BTNC用于数字+1，BTNU用于数字-1
- 数字循环变化

## 硬件资源
| 外设 | GPIO | 方向 |
|------|------|------|
| 数码管段码(8位) | GPIO1_CH1 | 输出 |
| 数码管位选(8位) | GPIO1_CH2 | 输出 |
| 独立按键(5位) | GPIO2_CH1 | 输入 |

## 核心实现逻辑

### 1. 中断处理
```c
// 按键中断 → 修改显示数字
void button_handle(void) {
    char button = Xil_In8(GPIO2_BASE + XGPIO_DATA_OFFSET) & 0x1F;
    
    if (button & 0x01) {  // BTNC: 数字+1
        display_num = (display_num + 1) & 0x0F;
    }
    if (button & 0x02) {  // BTNU: 数字-1
        display_num = (display_num - 1) & 0x0F;
    }
    
    // 更新8个数码管段码
    for (int i = 0; i < 8; i++)
        segcode[i] = segtable_hex[display_num];
}

// 定时器中断 → 数码管动态扫描
void timer_handle(void) {
    // 关闭所有位选
    Xil_Out8(GPIO1_BASE + XGPIO_DATA_OFFSET, 0xFF);
    Xil_Out8(GPIO1_BASE + XGPIO_DATA2_OFFSET, 0xFF);
    // 输出当前位段码
    Xil_Out8(GPIO1_BASE + XGPIO_DATA_OFFSET, poscode[pos]);
    Xil_Out8(GPIO1_BASE + XGPIO_DATA2_OFFSET, segcode[pos]);
    pos = (pos + 1) & 0x07;
}
```

### 2. 段码表
```c
char segtable_hex[16] = {
    0xC0, 0xF9, 0xA4, 0xB0, 0x99, 0x92, 0x82, 0xF8,  // 0-7
    0x80, 0x90, 0x88, 0x83, 0xC6, 0xA1, 0x86, 0x8E   // 8-F
};
```

## 关键点
- 8个数码管显示**相同**数字
- 按键BTNC增加、BTNU减少
- 数码管动态扫描定时刷新
- 数字循环：0→1→...→F→0