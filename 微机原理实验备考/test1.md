# 题目1：按键编码字符显示（全部显示）

## 题目要求
- LED实时显示独立开关对应位状态
- 8个七段数码管实时**全部**显示最近按下的独立按键位置编码字符（C, U, L, D, R）

## 硬件资源
| 外设 | GPIO | 方向 |
|------|------|------|
| 独立开关(16位) | GPIO0_CH1 | 输入 |
| LED灯(16位) | GPIO0_CH2 | 输出 |
| 数码管段码(8位) | GPIO1_CH1 | 输出 |
| 数码管位选(8位) | GPIO1_CH2 | 输出 |
| 独立按键(5位) | GPIO2_CH1 | 输入 |

## 核心实现逻辑

### 1. 按键编码表
```c
// BTNC=bit0 → 'C', BTNU=bit1 → 'U', BTNL=bit2 → 'L', BTND=bit3 → 'D', BTNR=bit4 → 'R'
char segtable_char[5] = {
    segtable_hex[12],  // C → 0xC
    segtable_hex[10],  // U → ...(自定义段码)
    segtable_hex[11],  // L → 0x8C
    segtable_hex[13],  // D → ...(自定义段码)
    segtable_hex[14]   // R → ...(自定义段码)
};
```

### 2. 字符段码定义
```c
#define SEG_C 0xC6  // C
#define SEG_U 0xC1  // U
#define SEG_L 0xC7  // L
#define SEG_D 0x86  // d
#define SEG_R 0x86  // r
```

### 3. 中断处理
- 按键中断触发 → 读取按键值 → 确定按键 → 将所有8个segcode设为对应字符
- 定时器中断触发 → 数码管动态扫描（逐个点亮位选，输出段码）

## 按键映射
| 按键 | 位 | 字符 | 段码 |
|------|----|------|------|
| BTNC | 0x01 | C | 0xC6 |
| BTNU | 0x02 | U | 0xC1 |
| BTNL | 0x04 | L | 0xC7 |
| BTND | 0x08 | D | 0x86 |
| BTNR | 0x10 | R | 0x86 |

## 关键函数
- `button_handle()`: 读取按键，更新segcode数组全部8位
- `timer_handle()`: 数码管动态扫描刷新