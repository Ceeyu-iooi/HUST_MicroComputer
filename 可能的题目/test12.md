# Test12 — 滚动显示3456 + C开始/L左移/R右移/U停止

## 题目描述
开机后数码管固定显示"3456"。按C键开始滚动，按L键设为左移方向，按R键设为右移方向，按U键停止滚动。滚动时3456四个数字在4个数码管上循环滚动。

## 硬件平台
CPU_INT_TIMER

## 中断设备
- GPIO_0 通道2（16位输出，LED）：未使用但初始化
- GPIO_1（8位输出，位码 + 段码）：数码管动态扫描
- GPIO_2 通道1（5位输入，按键）：按键中断
- Timer_0 T0：数码管动态扫描定时

## 核心代码逻辑

### 1. 3456段码数组
```c
char seg_3456[4] = {0xB0, 0x99, 0x92, 0x82};  // 3,4,5,6
```

### 2. 滚动控制变量
```c
int running = 0;        // 0=停止，1=滚动
int scroll_offset = 0;  // 滚动起始偏移 0~3
int direction = 0;      // 0=左移(L)，1=右移(R)
int scroll_slow = 0;    // 慢速计数器
```

### 3. 滚动显示逻辑
```c
// 更新segcode[0..3]
for (int i = 0; i < 4; i++) {
    int si = (scroll_offset + i) % 4;
    segcode[i] = seg_3456[si];
}

// 每完成一轮完整扫描（8位数码管都扫完）更新偏移
if (running) {
    scroll_slow++;
    if (scroll_slow >= 500) {
        scroll_slow = 0;
        if (direction == 0)       // 左移
            scroll_offset = (scroll_offset + 1) % 4;
        else                       // 右移
            scroll_offset = (scroll_offset + 3) % 4;  // +3 ≡ -1
    }
}
```

### 4. 按键处理
- C键（bit0，0x01）：`running = 1`
- L键（bit2，0x04）：`direction = 0`
- R键（bit3，0x08）：`direction = 1`
- U键（bit1，0x02）：`running = 0`

### 5. 备注
本题不需要开关中断（未使用开关输入），也不需要LED显示，只需数码管滚动显示功能。