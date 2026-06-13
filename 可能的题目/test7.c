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

#define RESET_VALUE0  1000 - 2  // T0初值，用于数码管动态扫描

// 中断掩码
#define XPAR_AXI_GPIO_0_IP2INTC_IRPT_MASK 0x1
#define XPAR_AXI_GPIO_2_IP2INTC_IRPT_MASK 0x2
#define XPAR_AXI_TIMER_0_INTERRUPT_MASK   0x4

void Initialization(void);
void My_ISR() __attribute__ ((interrupt_handler));
void switch_handle();
void button_handle();
void timer_handle();

int pos = 0;
unsigned short display_val = 0;

// 数码管共阳极段码表 0~F
char segtable[16] = {
    0xC0, 0xF9, 0xA4, 0xB0, 0x99, 0x92, 0x82, 0xF8,
    0x80, 0x90, 0x88, 0x83, 0xC6, 0xA1, 0x86, 0x8E
};
// 负号段码：只点亮g段(dp-g-f-e-d-c-b-a, g=0亮) = 0xBF
#define SEG_NEG  0xBF
#define SEG_OFF  0xFF

char segcode[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
short poscode[8] = {0x7F, 0xBF, 0xDF, 0xEF, 0xf7, 0xfb, 0xfd, 0xfe};

int main()
{
    xil_printf("\r\nTest7: 开关低8位开关输入/补码/取反显示\r\n");
    Initialization();
    while(1);
}

void Initialization(void)
{
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_TRI_OFFSET, 0xffff);
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_TRI2_OFFSET, 0x0);
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_TRI_OFFSET, 0x0);
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_TRI2_OFFSET, 0x0);
    Xil_Out8(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_TRI_OFFSET, 0x1f);

    Xil_Out32(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_ISR_OFFSET, XGPIO_IR_CH1_MASK);
    Xil_Out32(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_IER_OFFSET, XGPIO_IR_CH1_MASK);
    Xil_Out32(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_GIE_OFFSET,
              XGPIO_GIE_GINTR_ENABLE_MASK);

    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET, XGPIO_IR_CH1_MASK);
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_IER_OFFSET, XGPIO_IR_CH1_MASK);
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_GIE_OFFSET,
              XGPIO_GIE_GINTR_ENABLE_MASK);

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

    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IAR_OFFSET,
              XPAR_AXI_GPIO_0_IP2INTC_IRPT_MASK |
              XPAR_AXI_GPIO_2_IP2INTC_IRPT_MASK |
              XPAR_AXI_TIMER_0_INTERRUPT_MASK);
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IER_OFFSET,
              XPAR_AXI_GPIO_0_IP2INTC_IRPT_MASK |
              XPAR_AXI_GPIO_2_IP2INTC_IRPT_MASK |
              XPAR_AXI_TIMER_0_INTERRUPT_MASK);
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_MER_OFFSET,
              XIN_INT_MASTER_ENABLE_MASK | XIN_INT_HARDWARE_ENABLE_MASK);

    microblaze_enable_interrupts();
}

void My_ISR()
{
    int status;
    status = Xil_In32(XPAR_AXI_INTC_0_BASEADDR + XIN_ISR_OFFSET);

    if ((status & XPAR_AXI_GPIO_0_IP2INTC_IRPT_MASK) == XPAR_AXI_GPIO_0_IP2INTC_IRPT_MASK)
        switch_handle();
    if ((status & XPAR_AXI_GPIO_2_IP2INTC_IRPT_MASK) == XPAR_AXI_GPIO_2_IP2INTC_IRPT_MASK)
        button_handle();
    if ((status & XPAR_AXI_TIMER_0_INTERRUPT_MASK) == XPAR_AXI_TIMER_0_INTERRUPT_MASK)
        timer_handle();

    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IAR_OFFSET, status);
}

void switch_handle()
{
    short int sw;
    sw = Xil_In16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA_OFFSET);
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET, sw);
    if (sw)
        xil_printf("Switch = 0x%04X\r\n", sw);
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_ISR_OFFSET,
              Xil_In16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_ISR_OFFSET));
}

void button_handle()
{
    char button;
    button = Xil_In8(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_DATA_OFFSET) & 0x1f;
    if (button == 0)
    {
        Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET,
                  Xil_In32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET));
        return;
    }

    // 读取16位开关，取低8位
    unsigned char in_val = Xil_In16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA_OFFSET) & 0xFF;

    // C键(bit0)：低8位开关状态直接输入
    if (button & 0x01)
    {
        xil_printf("Mode C: Original value = 0x%02X (%d)\r\n", in_val, (signed char)in_val);
        display_val = in_val;

        // 判断符号位(bit7)，决定是否显示负号
        if (in_val & 0x80)
        {
            // 负数：显示"-"后跟原码的绝对值
            unsigned char abs_val = (~in_val + 1) & 0x7F;  // 补码转原码绝对值
            xil_printf("  Two's complement, absolute = %u\r\n", abs_val);
            int dc = 0;
            char digits[3] = {0,0,0};
            if (abs_val == 0) { digits[0] = 0; dc = 1; }
            else {
                unsigned char t = abs_val;
                while (t > 0) { digits[dc++] = t % 10; t /= 10; }
            }
            // 符号显示在第2位
            for (int i = 0; i < 8; i++) segcode[i] = SEG_OFF;
            segcode[1] = SEG_NEG;  // 负号
            for (int i = 0; i < dc; i++)
                segcode[3 - dc + i] = segtable[digits[dc - 1 - i]];
        }
        else
        {
            // 正数：十进制显示
            unsigned char val = in_val;
            int dc = 0;
            char digits[3] = {0,0,0};
            if (val == 0) { digits[0] = 0; dc = 1; }
            else {
                unsigned char t = val;
                while (t > 0) { digits[dc++] = t % 10; t /= 10; }
            }
            for (int i = 0; i < 8; i++) segcode[i] = SEG_OFF;
            for (int i = 0; i < dc; i++)
                segcode[4 - dc + i] = segtable[digits[dc - 1 - i]];
        }
    }
    // U键(bit1)：显示低8位补码
    else if (button & 0x02)
    {
        signed char sval = (signed char)in_val;
        xil_printf("Mode U: Two's complement value = %d\r\n", sval);
        display_val = (unsigned short)(unsigned char)sval;

        int negative = 0;
        unsigned char abs_val;
        if (sval < 0)
        {
            negative = 1;
            abs_val = (unsigned char)(-sval);
        }
        else
        {
            abs_val = (unsigned char)sval;
        }

        int dc = 0;
        char digits[3] = {0,0,0};
        if (abs_val == 0) { digits[0] = 0; dc = 1; }
        else {
            unsigned char t = abs_val;
            while (t > 0) { digits[dc++] = t % 10; t /= 10; }
        }

        for (int i = 0; i < 8; i++) segcode[i] = SEG_OFF;
        if (negative)
        {
            segcode[1] = SEG_NEG;
            for (int i = 0; i < dc; i++)
                segcode[3 - dc + i] = segtable[digits[dc - 1 - i]];
        }
        else
        {
            for (int i = 0; i < dc; i++)
                segcode[4 - dc + i] = segtable[digits[dc - 1 - i]];
        }
    }
    // L键(bit2)：取反结果
    else if (button & 0x04)
    {
        unsigned char inv = ~in_val;
        xil_printf("Mode L: Bitwise NOT of 0x%02X = 0x%02X (%d)\r\n", in_val, inv, (signed char)inv);
        display_val = inv;

        // 判断符号位
        int negative = 0;
        unsigned char abs_val;
        if (inv & 0x80)
        {
            negative = 1;
            abs_val = (~inv + 1) & 0x7F;
        }
        else
        {
            abs_val = inv;
        }

        int dc = 0;
        char digits[3] = {0,0,0};
        if (abs_val == 0) { digits[0] = 0; dc = 1; }
        else {
            unsigned char t = abs_val;
            while (t > 0) { digits[dc++] = t % 10; t /= 10; }
        }

        for (int i = 0; i < 8; i++) segcode[i] = SEG_OFF;
        if (negative)
        {
            segcode[1] = SEG_NEG;
            for (int i = 0; i < dc; i++)
                segcode[3 - dc + i] = segtable[digits[dc - 1 - i]];
        }
        else
        {
            for (int i = 0; i < dc; i++)
                segcode[4 - dc + i] = segtable[digits[dc - 1 - i]];
        }
    }

    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET,
              Xil_In32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET));
}

void timer_handle()
{
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA_OFFSET, 0xff);
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA2_OFFSET, 0xff);
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA_OFFSET, poscode[pos]);
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA2_OFFSET, segcode[pos]);
    pos++;
    if (pos == 8) pos = 0;
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET) | XTC_CSR_INT_OCCURED_MASK);
}