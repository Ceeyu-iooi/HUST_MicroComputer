# Test10：数码管显示开关值 + 按键R切换左/右滚动

## 功能概述

- **按键C**（bit0）：读取最右边4位独立开关的值，以**16进制**显示到**最右端数码管**（segcode[7]），其余7位熄灭
- **按键R**（bit3）：切换滚动方向
  - 第1次按R：进入**自动左移**模式（MODE_LEFT），每秒循环左移1位，到最左后回到最右
  - 第2次按R：切换为**自动右移**模式（MODE_RIGHT），每秒循环右移1位，到最右后回到最左
  - 第3次按R：切回左移，依此类推
- 按键U/L/D：预留（当前无功能）

## 操作流程示例

```
初始状态：全部熄灭（MODE_IDLE）

Step1: 拨动开关→低4位为0101（5）

Step2: 按C
       segcode: [ff][ff][ff][ff][ff][ff][ff][5]   ← 最右显示"5"
       mode=MODE_SHOW

Step3: 按R（第1次）
       mode=MODE_LEFT，T1驱动，每秒左移1位：
       t=1s: [ff][ff][ff][ff][ff][ff][5][ff]
       t=2s: [ff][ff][ff][ff][ff][5][ff][ff]
       ...
       t=7s: [5][ff][ff][ff][ff][ff][ff][ff]       ← "5"到最左
       t=8s: [ff][ff][ff][ff][ff][ff][ff][5]       ← 回到最右（循环）

Step4: 按R（第2次）
       mode=MODE_RIGHT，每秒右移1位：
       t=1s: [ff][ff][ff][ff][ff][ff][5][ff]
       t=2s: [ff][ff][ff][ff][ff][5][ff][ff]
       ... （向右滚动，最右→最左）

Step5: 再按C：重新读取开关值显示到最右，清除之前内容
```

## 模式定义

| 模式 | 值 | 触发按键 | 说明 |
|------|----|---------|------|
| MODE_IDLE | 0 | 初始 | 数码管全灭 |
| MODE_SHOW | 1 | C(bit0) | 开关低4位16进制显示到segcode[7] |
| MODE_LEFT | 2 | R(bit3)第1次 | T1驱动，每秒左移1位（循环） |
| MODE_RIGHT | 3 | R(bit3)第2次 | T1驱动，每秒右移1位（循环） |

## 硬件平台

| 外设 | 说明 |
|------|------|
| AXI GPIO 0 | CH1=16位独立开关（输入），CH2=16位LED灯（输出，未使用） |
| AXI GPIO 1 | CH1=8位数码管位选（输出），CH2=8位数码管段码（输出） |
| AXI GPIO 2 | CH1=5位独立按键（输入），支持中断 |
| AXI Timer 0 | T0通道=数码管动态扫描（约10μs），T1通道=滚动节奏（1s） |
| 中断控制器 | GPIO2按键中断（0x2）+ Timer0中断（0x4） |

## 中断响应流程

```
GPIO2按键按下           T0溢出(~10μs)          T1溢出(1s)
    ↓                       ↓                     ↓
My_ISR() ←─────────────────┴─────────────────────┘
    ├── button_handle()
    │       ├── BTNC(bit0) → mode=MODE_SHOW, 读取开关值到segcode[7]
    │       ├── BTNU(bit1) → 预留
    │       ├── BTNL(bit2) → 预留
    │       ├── BTNR(bit3) → mode切换：MODE_LEFT↔MODE_RIGHT↔MODE_LEFT...
    │       └── BTND(bit4) → 预留
    │
    └── timer_handle()
            ├── T0 → timer0_handle()：动态扫描数码管
            └── T1 → timer1_handle()
                    ├── MODE_LEFT → 循环左移
                    ├── MODE_RIGHT → 循环右移（反向遍历）
                    └── MODE_IDLE/MODE_SHOW → 不做操作
```

## 关键代码

### 按键C：读取开关值并显示

```c
if (button & BTNC_MASK)
{
    mode = MODE_SHOW;
    sw_val = Xil_In8(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA_OFFSET) & 0x0f;
    for (int i = 0; i < 7; i++)
        segcode[i] = 0xff;             // 左7位熄灭
    segcode[7] = segtable_hex[sw_val]; // 最右位显示16进制
}
```

### 按键R：切换左移/右移

```c
else if (button & BTNR_MASK)
{
    if (mode == MODE_LEFT)      // 当前左移 → 切右移
        mode = MODE_RIGHT;
    else if (mode == MODE_RIGHT) // 当前右移 → 切左移
        mode = MODE_LEFT;
    else                         // 非滚动模式 → 进入左移
        mode = MODE_LEFT;
}
```

### T1中断：每秒滚动

```c
void timer1_handle(void)
{
    if (mode == MODE_LEFT)
    {
        char temp = segcode[0];
        for (int i = 0; i < 7; i++)
            segcode[i] = segcode[i + 1];
        segcode[7] = temp;
    }
    else if (mode == MODE_RIGHT)
    {
        char temp = segcode[7];
        for (int i = 0; i < 7; i++)
            segcode[7 - i] = segcode[6 - i];
        segcode[0] = temp;
    }
    /* MODE_IDLE/MODE_SHOW：不操作 */
    /* 清除T1中断标志 */
}
```

## 与test12的差异对比

| 特性 | test12 | test10 |
|------|--------|--------|
| 初始内容 | 固定"3456" | 全灭 |
| 按键C功能 | 开始自动左移 | 读取开关值显示到最右 |
| 按键L功能 | 手动左移1位 | 预留 |
| 按键R功能 | 开始自动右移 | 切换左移/右移模式 |
| 滚动方向切换 | 按C左/按R右（固定） | 按R交替切换（左→右→左→右...） |
| U/D按键 | 预留 | 预留 |

## 常见问题

**Q：按C后显示的是A~F而不是数字？**
A：正确。开关低4位的值范围是0~15，当值为10~15时分别显示A~F（16进制）。

**Q：按R没有滚动？**
A：检查是否在非MODE_IDLE/MODE_SHOW模式下操作。如果当前是MODE_IDLE，按R会进入MODE_LEFT，但此时segcode只有最右位有值（其他都是0xff），滚动效果可能不明显。建议先按C读取开关值后再按R。

**Q：滚动时"5"消失在其他位中？**
A：因为segcode中只有segcode[7]有值（显示开关值），其他位都是0xff（全灭）。循环左移/右移时，熄灭的位也在移动，所以"5"在移动过程中会和其他熄灭位交替出现。如果要在滚动时看到完整的数字序列，需要先在多个位显示不同的数字。

**Q：为什么滚动方向切换是left→right→left→...而不是stop→left→right→stop？**
A：这是需求中"再按一次r"的切换逻辑。用户按C显示开关值后，按R开始左移，再按R切换为右移，再按R又切回左移。如果想停止滚动，可以再次按C重新读取开关值（会将mode重置为MODE_SHOW）。