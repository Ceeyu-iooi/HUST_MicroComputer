#include "xil_io.h"
#include "stdio.h"
#include "xintc_l.h"
#include "xtmrctr_l.h"
#include "xtmrctr.h"
#include "xgpio_l.h"
#include "xgpio.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "mb_interface.h"

#define RESET_VALUE0  1000 - 2  // T0 初值，约0.000001s，用于数码管动态扫描

// 宏定义中断掩码
#define XPAR_AXI_GPIO_2_IP2INTC_IRPT_MASK 0x2
#define XPAR_AXI_TIMER_0_INTERRUPT_MASK   0x4

// 函数声明
void Initialization(void);
void My_ISR() __attribute__ ((interrupt_handler));
void button_handle();
void timer_handle();
void update_segcode_C();
void update_segcode_D();
void update_segcode_R();
void update_segcode_U_slow();

// 全局变量
int pos = 0;  // 数码管扫描位置 (0~7)

// 共阳极七段数码管段码表（dp-g-f-e-d-c-b-a，0=亮，1=灭）
char hex_segcode_table[16] = {
    0xC0, // 0
    0xF9, // 1
    0xA4, // 2
    0xB0, // 3
    0x99, // 4
    0x92, // 5
    0x82, // 6
    0xF8, // 7
    0x80, // 8
    0x90, // 9
    0x88, // A
    0x83, // B
    0xC6, // C
    0xA1, // D
    0x86, // E
    0x8E  // F
};

#define SEG_OFF    0xFF   // 熄灭
#define SEG_MINUS  0xBF   // 负号 '-'
#define SEG_CHAR_0 0xC0   // '0'
#define SEG_CHAR_1 0xF9   // '1'

// 显示缓冲区（8个数码管的段码）
char segcode[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// 位码表：低电平选中，从左到右对应第1~8个数码管
short poscode[8] = {0x7F, 0xBF, 0xDF, 0xEF, 0xF7, 0xFB, 0xFD, 0xFE};

// ========================= 模式与状态 =========================
// 当前模式：0=无模式, 'C','L','D','R','U'
char current_mode = 0;

// LED 状态（16位）
unsigned short led_state = 0;

// ---- 按键C：流水灯 ----
int flow_pos = 0;        // 当前亮的LED位置 (0~15)
int flow_speed = 0;      // 速度档位 0=1s, 1=0.5s, 2=0.25s
int flow_cnt = 0;        // 流水灯独立计数器
// Timer：100MHz÷998≈100kHz，即 100000 次/秒
int flow_thresholds[3] = {100000, 50000, 25000};

// ---- 按键U：滚动显示 ----
int scroll_offset = 0;   // 滚动偏移（字符串起始在segcode中的位置）
char scroll_seg[4];      // 滚动显示字符的段码（最多4字符：负号+2位数字+空）
int scroll_len = 0;      // 有效字符数
int u_speed = 0;         // U模式速度档位 0=1s, 1=0.5s
int scroll_cnt = 0;      // 滚动独立计数器
int u_thresholds[2] = {100000, 50000};

// 当前开关值（按键按下时锁存）
unsigned short current_sw = 0;

int main()
{
    xil_printf("\r\nRunning Test - Multi-function Display\r\n");
    Initialization();
    while(1);
}

void Initialization(void)
{
    // GPIO 输入/输出方向配置
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_TRI_OFFSET, 0xffff);   // 开关：16位输入
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_TRI2_OFFSET, 0x0);     // LED：16位输出
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_TRI_OFFSET, 0x0);       // 数码管位选：输出
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_TRI2_OFFSET, 0x0);      // 数码管段码：输出
    Xil_Out8(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_TRI_OFFSET, 0x1f);      // 按键：5位输入

    // GPIO_2 按键中断使能
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET, XGPIO_IR_CH1_MASK);
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_IER_OFFSET, XGPIO_IR_CH1_MASK);
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_GIE_OFFSET,
              XGPIO_GIE_GINTR_ENABLE_MASK);

    // 初始化 T0 定时器（用于数码管动态扫描 + 慢速定时）
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET)
              & ~XTC_CSR_ENABLE_TMR_MASK);
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TLR_OFFSET, RESET_VALUE0);
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET)
              | XTC_CSR_LOAD_MASK);
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
              (Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET) &
              ~XTC_CSR_LOAD_MASK) |
              XTC_CSR_ENABLE_INT_MASK |
              XTC_CSR_AUTO_RELOAD_MASK |
              XTC_CSR_DOWN_COUNT_MASK |
              XTC_CSR_INT_OCCURED_MASK |
              XTC_CSR_ENABLE_TMR_MASK);

    // 初始化 INTC（中断控制器）
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IAR_OFFSET,
              XPAR_AXI_GPIO_2_IP2INTC_IRPT_MASK |
              XPAR_AXI_TIMER_0_INTERRUPT_MASK);
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IER_OFFSET,
              XPAR_AXI_GPIO_2_IP2INTC_IRPT_MASK |
              XPAR_AXI_TIMER_0_INTERRUPT_MASK);
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_MER_OFFSET,
              XIN_INT_MASTER_ENABLE_MASK | XIN_INT_HARDWARE_ENABLE_MASK);

    // CPU 中断使能
    microblaze_enable_interrupts();
}

// ========================= 主中断服务程序 =========================
void My_ISR()
{
    int status;
    status = Xil_In32(XPAR_AXI_INTC_0_BASEADDR + XIN_ISR_OFFSET);

    if ((status & XPAR_AXI_GPIO_2_IP2INTC_IRPT_MASK) == XPAR_AXI_GPIO_2_IP2INTC_IRPT_MASK)
    {
        button_handle();
    }
    if ((status & XPAR_AXI_TIMER_0_INTERRUPT_MASK) == XPAR_AXI_TIMER_0_INTERRUPT_MASK)
    {
        timer_handle();
    }

    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IAR_OFFSET, status);
}

// ========================= 按键中断服务程序 =========================
// 按键映射：BTNC=bit0(C), BTNU=bit1(U), BTNL=bit2(L), BTNR=bit3(R), BTND=bit4(D)
// =================================================================
void button_handle()
{
    char button;
    button = Xil_In8(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_DATA_OFFSET) & 0x1f;
    if (button == 0)  // 按键松开引起的中断，不处理
    {
        Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET,
            Xil_In32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET));
        return;
    }

    // 读取16位独立开关状态
    current_sw = Xil_In16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA_OFFSET);

    // -------------------------------------------------------
    // A. 按键C (BTNC, bit0)：流水灯模式
    // -------------------------------------------------------
    if (button & 0x01)
    {
        if (current_mode != 'C')
        {
            // 首次进入C模式
            current_mode = 'C';
            flow_speed = 0;
            flow_pos = 0;
            flow_cnt = 0;
            xil_printf("Mode C: flow LED, speed=1s\r\n");
        }
        else
        {
            // 切换速度：0→1→2→0
            flow_speed = (flow_speed + 1) % 3;
            flow_cnt = 0;
            if (flow_speed == 0)      xil_printf("Mode C: speed=1s\r\n");
            else if (flow_speed == 1) xil_printf("Mode C: speed=0.5s\r\n");
            else                      xil_printf("Mode C: speed=0.25s\r\n");
        }
        // 数码管全灭
        update_segcode_C();
        // 更新LED
        led_state = (unsigned short)(1 << flow_pos);
        Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET, led_state);
    }
    // -------------------------------------------------------
    // B. 按键L (BTNL, bit2)：LED反映开关/左移
    // -------------------------------------------------------
    else if (button & 0x04)
    {
        if (current_mode != 'L')
        {
            // 首次进入L模式：LED反映开关状态
            current_mode = 'L';
            led_state = current_sw;
            xil_printf("Mode L: LED=0x%04X (switch state)\r\n", led_state);
        }
        else
        {
            // 循环左移一位
            led_state = (led_state << 1) | ((led_state >> 15) & 0x0001);
            xil_printf("Mode L: LED shifted left -> 0x%04X\r\n", led_state);
        }
        Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET, led_state);
    }
    // -------------------------------------------------------
    // C. 按键D (BTND, bit4)：左边4位数码管显示低4位二进制，LED奇数位亮
    // -------------------------------------------------------
    else if (button & 0x10)
    {
        current_mode = 'D';
        xil_printf("Mode D: left 4 seg = low4 binary, LED odd-on\r\n");
        update_segcode_D();
        led_state = 0x5555;  // 奇数位亮
        Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET, led_state);
    }
    // -------------------------------------------------------
    // D. 按键R (BTNR, bit3)：右边4位数码管显示低4位反码，LED偶数位亮
    // -------------------------------------------------------
    else if (button & 0x08)
    {
        current_mode = 'R';
        xil_printf("Mode R: right 4 seg = inverted low4 binary, LED even-on\r\n");
        update_segcode_R();
        led_state = 0xAAAA;  // 偶数位亮
        Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET, led_state);
    }
    // -------------------------------------------------------
    // E. 按键U (BTNU, bit1)：滚动显示低4位带符号十进制
    // -------------------------------------------------------
    else if (button & 0x02)
    {
        if (current_mode != 'U')
        {
            current_mode = 'U';
            u_speed = 0;
            scroll_cnt = 0;
            scroll_offset = 8;  // 从右侧开始进入
            xil_printf("Mode U: scroll decimal, speed=1s\r\n");
        }
        else
        {
            // 切换速度 0=1s ↔ 1=0.5s
            u_speed = (u_speed + 1) % 2;
            scroll_cnt = 0;
            if (u_speed == 0) xil_printf("Mode U: speed=1s\r\n");
            else              xil_printf("Mode U: speed=0.5s\r\n");
        }
        // LED反映开关值
        led_state = current_sw;
        Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET, led_state);

        // 计算低4位带符号十进制并准备滚动段码
        {
            char low4 = current_sw & 0x0F;
            char sign = (low4 >> 3) & 0x01;   // bit3 = 符号位
            char abs_val = low4 & 0x07;        // 低3位 = 绝对值

            // 构建段码字符串
            int idx = 0;
            if (sign == 1 && abs_val != 0)
            {
                scroll_seg[idx++] = SEG_MINUS;  // 负号
            }
            if (abs_val == 0)
            {
                scroll_seg[idx++] = hex_segcode_table[0];  // "0"
            }
            else
            {
                // 提取十进制数字（个位→高位）
                char digits[2];
                int dc = 0;
                char t = abs_val;
                while (t > 0)
                {
                    digits[dc++] = t % 10;
                    t /= 10;
                }
                // digits[0]=个位, digits[dc-1]=最高位 → 反向放入scroll_seg
                for (int i = dc - 1; i >= 0; i--)
                {
                    scroll_seg[idx++] = hex_segcode_table[digits[i]];
                }
            }
            scroll_len = idx;
        }
        // 初始化滚动显示
        update_segcode_U_slow();
    }

    // 清除按键中断标志位
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET,
              Xil_In32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET));
}

// ========================= 定时器中断服务程序 =========================
// 两件事：①数码管动态扫描（高速）；②慢速任务（通过软件计数实现秒级定时）
// ====================================================================
void timer_handle()
{
    // ---- 数码管动态扫描（始终保持） ----
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA_OFFSET, 0xFF);   // 消隐
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA2_OFFSET, 0xFF);

    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA_OFFSET, poscode[pos]);
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA2_OFFSET, segcode[pos]);

    pos++;
    if (pos == 8)
        pos = 0;

    // ---- 慢速任务（C模式流水灯） ----
    if (current_mode == 'C')
    {
        flow_cnt++;
        if (flow_cnt >= flow_thresholds[flow_speed])
        {
            flow_cnt = 0;
            flow_pos = (flow_pos + 1) & 0x0F;
            led_state = (unsigned short)(1 << flow_pos);
            Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET, led_state);
        }
    }
    // ---- 慢速任务（U模式滚动） ----
    else if (current_mode == 'U')
    {
        scroll_cnt++;
        if (scroll_cnt >= u_thresholds[u_speed])
        {
            scroll_cnt = 0;
            scroll_offset++;
            if (scroll_offset > 7)
                scroll_offset = -(scroll_len - 1);
            update_segcode_U_slow();
        }
    }

    // 清除定时器中断标志位
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET) | XTC_CSR_INT_OCCURED_MASK);
}

// ========================= 显示更新函数 =========================

// 按键C：数码管全灭
void update_segcode_C()
{
    for (int i = 0; i < 8; i++)
        segcode[i] = SEG_OFF;
}

// 按键D：左边4位 = 低4位二进制，右边4位灭
void update_segcode_D()
{
    char low4 = current_sw & 0x0F;
    // 左边4位：segcode[0]=bit3, segcode[1]=bit2, segcode[2]=bit1, segcode[3]=bit0
    segcode[0] = (low4 & 0x08) ? SEG_CHAR_1 : SEG_CHAR_0;
    segcode[1] = (low4 & 0x04) ? SEG_CHAR_1 : SEG_CHAR_0;
    segcode[2] = (low4 & 0x02) ? SEG_CHAR_1 : SEG_CHAR_0;
    segcode[3] = (low4 & 0x01) ? SEG_CHAR_1 : SEG_CHAR_0;
    // 右边4位灭
    for (int i = 4; i < 8; i++)
        segcode[i] = SEG_OFF;
}

// 按键R：右边4位 = 低4位反码二进制，左边4位灭
void update_segcode_R()
{
    char low4 = current_sw & 0x0F;
    // 右边4位：segcode[4]=~bit3, segcode[5]=~bit2, segcode[6]=~bit1, segcode[7]=~bit0
    segcode[4] = (low4 & 0x08) ? SEG_CHAR_0 : SEG_CHAR_1;
    segcode[5] = (low4 & 0x04) ? SEG_CHAR_0 : SEG_CHAR_1;
    segcode[6] = (low4 & 0x02) ? SEG_CHAR_0 : SEG_CHAR_1;
    segcode[7] = (low4 & 0x01) ? SEG_CHAR_0 : SEG_CHAR_1;
    // 左边4位灭
    for (int i = 0; i < 4; i++)
        segcode[i] = SEG_OFF;
}

// 按键U：根据 scroll_offset 将 scroll_seg 填入 segcode
void update_segcode_U_slow()
{
    // 先全灭
    for (int i = 0; i < 8; i++)
        segcode[i] = SEG_OFF;

    // 将字符串放入指定偏移位置
    for (int i = 0; i < scroll_len; i++)
    {
        int pos_dest = scroll_offset + i;
        if (pos_dest >= 0 && pos_dest < 8)
        {
            segcode[pos_dest] = scroll_seg[i];
        }
    }
}