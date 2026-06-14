# 题目6：按键左/右移显示开关4位16进制值（中断方式 + 数码管扫描）

## 题目要求
- 点击按键 **BTNC**（bit0）：数码管**最低位（最右边）**以 **16进制** 显示**最右边4位**独立开关的值，其余7位熄灭
- 每点击按键 **BTNL**（bit2）：显示的16进制数字**循环左移一位**（数字到达最左端后，下次左移移动到最低位）
- 每点击按键 **BTNR**（bit3）：显示的16进制数字**循环右移一位**
- 使用中断方式 + T0 定时器数码管动态扫描

## 硬件资源
| 外设 | GPIO | 方向 |
|------|------|------|
| 独立开关(16位) | GPIO0_CH1 | 输入 |
| LED灯(16位) | GPIO0_CH2 | 输出 |
| 数码管位选(8位) | GPIO1_CH1 | 输出 |
| 数码管段码(8位) | GPIO1_CH2 | 输出 |
| 独立按键(5位) | GPIO2_CH1 | 输入 |

## 中断架构
| 中断源 | 掩码 | 作用 |
|--------|------|------|
| GPIO2（按键） | 0x2 | 检测 BTNC/BTNL/BTNR 按键按下，执行对应操作 |
| TIMER0 T0 | 0x4 | 数码管动态扫描（约10μs） |

## 核心实现逻辑

### 1. BTNC：数码管最低位显示开关值
```c
if (button & 0x01) {  // BTNC
    unsigned short sw = Xil_In16(GPIO0_BASE + XGPIO_DATA_OFFSET);
    unsigned char low4 = sw & 0xF;  // 取最右边4位

    for (int i = 0; i < 7; i++)
        segcode[i] = 0xff;  // 熄灭高7位
    segcode[7] = segtable_hex[low4];  // 最低位显示16进制值
}
```

### 2. BTNL：循环左移一位
```c
else if (button & 0x04) {  // BTNL
    char temp = segcode[0];            // 保存最左端值
    for (int i = 0; i < 7; i++)
        segcode[i] = segcode[i + 1];   // 后一位覆盖前一位（左移）
    segcode[7] = temp;                 // 原最左端循环到最右端
}
```

### 3. BTNR：循环右移一位
```c
else if (button & 0x08) {  // BTNR
    char temp = segcode[7];            // 保存最右端值
    for (int i = 0; i < 7; i++)
        segcode[7 - i] = segcode[6 - i]; // 前一位覆盖后一位（右移，从右向左遍历避免覆盖）
    segcode[0] = temp;                 // 原最右端循环到最左端
}
```

### 4. T0 定时器：数码管动态扫描
```c
void timer_handle(void) {
    // 消影
    Xil_Out8(GPIO1_BASE + XGPIO_DATA_OFFSET,  0xff);
    Xil_Out8(GPIO1_BASE + XGPIO_DATA2_OFFSET, 0xff);

    // 输出当前位
    Xil_Out8(GPIO1_BASE + XGPIO_DATA_OFFSET,  poscode[pos]);
    Xil_Out8(GPIO1_BASE + XGPIO_DATA2_OFFSET, segcode[pos]);

    pos++;
    if (pos == 8) pos = 0;
}
```

## 数据流向示意
```
                    BTNC按下
独立开关[3:0] ──→ low4(0~F) ──→ segtable_hex ──→ segcode[7] ──→ 最低位显示
（例如开关=1010₂=10=A）                                   ↓
                                                    BTNL/BTNR 循环移位
                                               segcode[0] ↔ ... ↔ segcode[7]
```

## 循环移位示例（初始显示 A 在最右端）
```
初始:  [X] [X] [X] [X] [X] [X] [X] [A]    (X=熄灭, A=0xA的段码)
左移:  [X] [X] [X] [X] [X] [X] [A] [X]
再左移: [X] [X] [X] [X] [X] [A] [X] [X]
...（持续左移，A 移动到最左端再回到最右端）
7次左移: [A] [X] [X] [X] [X] [X] [X] [X]
8次左移: [X] [X] [X] [X] [X] [X] [X] [A]  （回到原位）
```

## 关键点
- **BTNC 只影响 segcode[7]**（最低位），其余位熄灭为 0xff
- **BTNL/BTNR 是对 8 位 segcode 整体做循环移位**，不关心当前实际显示了几位数字
- 循环左移/右移的实现注意**遍历方向**：
  - 左移：`i` 从 `0→6`，`segcode[i]=segcode[i+1]`（从前往后，用后一位的值覆盖前一位）
  - 右移：`i` 从 `0→6`，`segcode[7-i]=segcode[6-i]`（从后往前，用前一位的值覆盖后一位）
  - 右移若用 `segcode[i]=segcode[i-1]`（`i` 从 1→7）也可以，但注意 `i` 从 1 开始而非 0
- **先保存被移出的值**到临时变量，最后放到对面端，实现循环
- 本实验只用 T0（数码管扫描），**不需要 T1**，因此初始化中去掉了 T1 相关代码
- VSCode 报"未定义标识符"是因 IntelliSense 未配置 BSP 路径，Vitis 编译无问题