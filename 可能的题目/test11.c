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

#define RESET_VALUE0  1000 - 2  // T0初值

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

// 段码表
char segtable[16] = {
    0xC0, 0xF9, 0xA4, 0xB0, 0x99, 0x92, 0x82, 0xF8,
    0x80, 0x90, 0x88, 0x83, 0xC6, 0xA1, 0x86, 0x8E
};
#define SEG_OFF 0xFF

char segcode[8] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
short poscode[8] = {0x7F,0xBF,0xDF,0xEF,0xf7,0xfb,0xfd,0xfe};

// 模式：0=高8位LED显示，1=R右移
int mode = 0;
unsigned short shift_val = 0;
int shift_slow = 0;

int main()
{
    xil_printf("\r\nTest11: 高8位LED显示，R右移\r\n");
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
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET) & ~XTC_CSR_ENABLE_TMR_MASK);
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TLR_OFFSET, RESET_VALUE0);
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET) | XTC_CSR_LOAD_MASK);
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
              (Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET) & ~XTC_CSR_LOAD_MASK) |
              XTC_CSR_ENABLE_INT_MASK | XTC_CSR_AUTO_RELOAD_MASK |
              XTC_CSR_DOWN_COUNT_MASK | XTC_CSR_INT_OCCURED_MASK | XTC_CSR_ENABLE_TMR_MASK);

    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IAR_OFFSET,
              XPAR_AXI_GPIO_0_IP2INTC_IRPT_MASK | XPAR_AXI_GPIO_2_IP2INTC_IRPT_MASK |
              XPAR_AXI_TIMER_0_INTERRUPT_MASK);
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IER_OFFSET,
              XPAR_AXI_GPIO_0_IP2INTC_IRPT_MASK | XPAR_AXI_GPIO_2_IP2INTC_IRPT_MASK |
              XPAR_AXI_TIMER_0_INTERRUPT_MASK);
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_MER_OFFSET,
              XIN_INT_MASTER_ENABLE_MASK | XIN_INT_HARDWARE_ENABLE_MASK);

    microblaze_enable_interrupts();
}

void My_ISR()
{
    int status = Xil_In32(XPAR_AXI_INTC_0_BASEADDR + XIN_ISR_OFFSET);
    if ((status & XPAR_AXI_GPIO_0_IP2INTC_IRPT_MASK) == XPAR_AXI_GPIO_0_IP2INTC_IRPT_MASK) switch_handle();
    if ((status & XPAR_AXI_GPIO_2_IP2INTC_IRPT_MASK) == XPAR_AXI_GPIO_2_IP2INTC_IRPT_MASK) button_handle();
    if ((status & XPAR_AXI_TIMER_0_INTERRUPT_MASK) == XPAR_AXI_TIMER_0_INTERRUPT_MASK) timer_handle();
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IAR_OFFSET, status);
}

void switch_handle()
{
    short sw = Xil_In16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA_OFFSET);
    display_val = sw;
    shift_val = sw;

    // 默认模式：高8位LED显示（通过GPIO_DATA2输出高8位屏蔽低8位）
    // 实际上LED显示高8位，数码管显示4位16进制
    unsigned short hi = display_val & 0xFF00;
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET, hi);
    xil_printf("Switch = 0x%04X, High 8 LED = 0x%02X\r\n", sw, hi >> 8);

    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_ISR_OFFSET,
              Xil_In16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_ISR_OFFSET));
}

void button_handle()
{
    char btn = Xil_In8(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_DATA_OFFSET) & 0x1f;
    if (btn == 0) {
        Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET,
                  Xil_In32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET));
        return;
    }

    // R键：右移模式
    if (btn & 0x08) {
        mode = 1;
        shift_val = display_val;
        shift_slow = 0;
        xil_printf("Mode R: right shift starting 0x%04X\r\n", shift_val);
    }

    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET,
              Xil_In32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET));
}

void timer_handle()
{
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA_OFFSET, 0xff);
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA2_OFFSET, 0xff);

    if (mode == 0) {
        // 显示当前值（高8位+低8位分别显示）
        unsigned short v = display_val;
        for (int i = 0; i < 4; i++)
            segcode[i] = segtable[(v >> (12 - 4*i)) & 0xF];
        for (int i = 4; i < 8; i++)
            segcode[i] = SEG_OFF;
        // LED显示高8位
        Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET, display_val & 0xFF00);
    }
    else if (mode == 1) {
        // 右移模式
        if (shift_slow++ >= 1000) {  // 约1秒移1位
            shift_slow = 0;
            shift_val >>= 1;
        }
        unsigned short v = shift_val;
        for (int i = 0; i < 4; i++)
            segcode[i] = segtable[(v >> (12 - 4*i)) & 0xF];
        for (int i = 4; i < 8; i++)
            segcode[i] = SEG_OFF;
        Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET, shift_val & 0xFF00);
    }

    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA_OFFSET, poscode[pos]);
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA2_OFFSET, segcode[pos]);
    pos++;
    if (pos == 8) pos = 0;

    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET) | XTC_CSR_INT_OCCURED_MASK);
}