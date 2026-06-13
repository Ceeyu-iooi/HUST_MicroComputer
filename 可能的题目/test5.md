# Test5 — 开关低4位十六进制显示（共4位）

## 题目描述
拨动16位开关，数码管前4位以16进制显示开关的当前值（后4位全灭），LED实时显示开关状态。

## 硬件平台
CPU_INT_TIMER

## 中断设备
同test1

## 核心代码逻辑

### 1. 4位十六进制拆分
```c
unsigned short sw = Xil_In16(...);
for (int i = 0; i < 4; i++)
    segcode[i] = segtable[(sw >> (12 - 4*i)) & 0xF];
for (int i = 4; i < 8; i++)
    segcode[i] = SEG_OFF;
```

### 2. 取位公式
- 第0位数码管显示最高4位：`(sw >> 12) & 0xF`
- 第1位数码管显示次高4位：`(sw >> 8) & 0xF`
- 第2位数码管显示次低4位：`(sw >> 4) & 0xF`
- 第3位数码管显示最低4位：`sw & 0xF`

### 3. 通用循环
`segcode[i] = segtable[(sw >> (12 - 4*i)) & 0xF]` 其中 i=0,1,2,3