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

#define XPAR_AXI_GPIO_0_IP2INTC_IRPT_MASK 0x1
#define XPAR_AXI_TIMER_0_INTERRUPT_MASK   0x4

void Initialization(void);
void My_ISR() __attribute__ ((interrupt_handler));
void timer_handle();

int pos = 0;
int scroll_pos = 0;  // 滚动位置 0~7

// 3,4,5,6的段码
char seg_3 = 0xB0;
char seg_4 = 0x99;
char seg_5 = 0x92;
char seg_6 = 0x82;
#define SEG_OFF 0xFF

// 不带滚动的显示缓冲区
char segcode[8];
// 滚动缓冲区（8+4=12个位置，循环显示3456）
char scroll_buf[8] = {SEG_OFF, SEG_OFF, SEG_OFF, SEG_OFF, SEG_OFF, SEG_OFF, SEG_OFF, SEG_OFF};
// 要显示的字符串"3456"被映射到滚动缓冲区（在定时中断中动态更新）

short poscode[8] = {0x7F, 0xBF, 0xDF, 0xEF, 0xf7, 0xfb, 0xfd, 0xfe};

int main()
{
    xil_printf("\r\nTest8: 数码管滚动显示3456\r\n");
    Initialization();
    while(1);
}

void Initialization(void)
{
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_TRI_OFFSET, 0xffff);
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_TRI2_OFFSET, 0x0);
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_TRI_OFFSET, 0x0);
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_TRI2_OFFSET, 0x0);

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
              XPAR_AXI_TIMER_0_INTERRUPT_MASK);
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IER_OFFSET,
              XPAR_AXI_TIMER_0_INTERRUPT_MASK);
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_MER_OFFSET,
              XIN_INT_MASTER_ENABLE_MASK | XIN_INT_HARDWARE_ENABLE_MASK);

    // 初始化：将3456放入滚动缓冲区的后4位（从右边进入）
    scroll_buf[4] = seg_3;
    scroll_buf[5] = seg_4;
    scroll_buf[6] = seg_5;
    scroll_buf[7] = seg_6;

    microblaze_enable_interrupts();
}

void My_ISR()
{
    int status;
    status = Xil_In32(XPAR_AXI_INTC_0_BASEADDR + XIN_ISR_OFFSET);

    if ((status & XPAR_AXI_TIMER_0_INTERRUPT_MASK) == XPAR_AXI_TIMER_0_INTERRUPT_MASK)
    {
        timer_handle();
    }

    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IAR_OFFSET, status);
}

// 滚动计数器，每N次T0中断才滚动一位（控制滚动速度）
int scroll_divider = 0;
#define SCROLL_SPEED 500  // 调整此值控制滚动速度

void timer_handle()
{
    // 消隐
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA_OFFSET, 0xff);
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA2_OFFSET, 0xff);

    // 输出当前位码和段码
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA_OFFSET, poscode[pos]);
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA2_OFFSET, scroll_buf[pos]);

    pos++;

    // 一帧扫描完毕（8位数码管都扫了一遍），更新滚动位置
    if (pos == 8)
    {
        pos = 0;
        scroll_divider++;
        if (scroll_divider >= SCROLL_SPEED)
        {
            scroll_divider = 0;
            // 滚动：左移一位
            char temp = scroll_buf[0];
            for (int i = 0; i < 7; i++)
            {
                scroll_buf[i] = scroll_buf[i + 1];
            }
            scroll_buf[7] = temp;
        }
    }

    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET) | XTC_CSR_INT_OCCURED_MASK);
}