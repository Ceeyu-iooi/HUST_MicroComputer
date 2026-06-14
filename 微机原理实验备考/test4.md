# 题目4：加法与乘法运算（程序控制方式）

## 题目要求
- 独立开关输入两个操作数
- 按键BTNC：将SW[7:0]（data1）和SW[15:8]（data2）相加，结果送LED
- 按键BTNU：将data1和data2相乘，结果送LED
- 纯程序控制（无中断）

## 硬件资源
| 外设 | GPIO | 方向 |
|------|------|------|
| 独立开关(16位) | GPIO0_CH1 | 输入 |
| LED灯(16位) | GPIO0_CH2 | 输出 |
| 独立按键(5位) | GPIO2_CH1 | 输入 |

## 核心实现逻辑

### 1. 轮询检测按键
```c
while(1) {
    // 读取按键值
    char button = Xil_In8(GPIO2_BASE + XGPIO_DATA_OFFSET) & 0x1F;
    
    if (button) {
        // 读取两个操作数
        unsigned short data1 = Xil_In16(GPIO0_BASE + XGPIO_DATA_OFFSET) & 0xFF;
        unsigned short data2 = (Xil_In16(GPIO0_BASE + XGPIO_DATA_OFFSET) >> 8) & 0xFF;
        
        if (button & 0x01) {  // BTNC: 加法
            result = data1 + data2;
        }
        if (button & 0x02) {  // BTNU: 乘法
            result = data1 * data2;
        }
        
        Xil_Out16(GPIO0_BASE + XGPIO_DATA2_OFFSET, result & 0xFFFF);
    }
}
```

### 2. 数据映射
```
SW[7:0]  = data1 (低8位)
SW[15:8] = data2 (高8位)
LED[15:0] = 运算结果
```

## 关键点
- 无中断，纯程序控制轮询
- 加法：data1 + data2（可能溢出16位，取低16位）
- 乘法：data1 × data2（结果可能超16位，取低16位）
- 两个按键触发两种不同运算