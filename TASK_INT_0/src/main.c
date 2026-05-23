#include "xil_io.h"
#include "stdio.h"
#include "xintc_l.h"
#include "xtmrctr_l.h"
#include "xgpio_l.h"
#include "xil_printf.h"
#include "xparameters.h"

#define RESET_VALUE0  100000 - 2
#define RESET_VALUE1  100000 - 2
#define STEP_PACE 10000000

//中断掩码
#define XPAR_AXI_GPIO_0_IP2INTC_IRPT_MASK 0x1 
#define XPAR_AXI_GPIO_2_IP2INTC_IRPT_MASK 0x2
#define XPAR_AXI_TIMER_0_INTERRUPT_MASK 0x4

void My_ISR() __attribute__ ((interrupt_handler));
void switch_handle();
void button_handle();
void timer_handle();
void timer0_handle();
void timer1_handle();


char segtable[5]={0xc6,0xc1,0xc7,0x88,0xa1};//段码表CULRd
char segcode[8]={0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0};//显示缓冲区
short poscode[8] = {0x7F, 0xBF, 0xDF, 0xEF, 0xf7, 0xfb, 0xfd, 0xfe}; // 8位数码管位码表，低电平选中，从左到右对应第1~8个
int mask;

int ledbits = 0;
int pos = 0;

int main()
{
    Initialization();
    return 0;
}


void Initialization()
{
        // GPIO 输入/输出配置
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_TRI_OFFSET,0xffff); // 设置开关为输入
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_TRI2_OFFSET,0x0); // 设置LED为输出
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_TRI_OFFSET,0x0); // 设置数码管位选为输出
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_TRI2_OFFSET,0x0); // 设置数码管段码为输出
    Xil_Out8(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_TRI_OFFSET,0x1f); // 设置按键为输入


    // GPIO 中断使能
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET,XGPIO_IR_CH1_MASK);// 清除中断标志位
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_IER_OFFSET,XGPIO_IR_CH1_MASK);// 使能开关中断
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_GIE_OFFSET,
              XGPIO_GIE_GINTR_ENABLE_MASK);// 使能开关中断

    Xil_Out32(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_ISR_OFFSET,XGPIO_IR_CH1_MASK);// 清除中断标志位
    Xil_Out32(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_IER_OFFSET,XGPIO_IR_CH1_MASK);// 使能开关中断
    Xil_Out32(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_GIE_OFFSET,
              XGPIO_GIE_GINTR_ENABLE_MASK);// 使能开关中断

    // 初始化 T0
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
              ~XTC_CSR_ENABLE_TMR_MASK);// 关闭 T0
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TLR_OFFSET, RESET_VALUE0);// 设置初值
    Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
             XTC_CSR_LOAD_MASK);// 载入初值
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
              (Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET) &
              ~XTC_CSR_AUTO_RELOAD_MASK) | XTC_CSR_ENABLE_INT_MASK |
              XTC_CSR_DOWN_COUNT_MASK | XTC_CSR_INT_OCCURED_MASK);// 启动 T0
    //(原状态 & ~屏蔽位) | 开启位1 ENT| 开启位2 减计数| 开启位3  TINT 请中断状态


    // 初始化 INTC、开中断
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IAR_OFFSET,
            XPAR_AXI_GPIO_0_IP2INTC_IRPT_MASK |
              XPAR_AXI_GPIO_2_IP2INTC_IRPT_MASK |
              XPAR_AXI_TIMER_0_INTERRUPT_MASK);// 清除中断标志位
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IER_OFFSET,
              XPAR_AXI_GPIO_0_IP2INTC_IRPT_MASK |
              XPAR_AXI_GPIO_2_IP2INTC_IRPT_MASK |
              XPAR_AXI_TIMER_0_INTERRUPT_MASK);// 使能中断
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_MER_OFFSET,
              XIN_INT_MASTER_ENABLE_MASK | XIN_INT_HARDWARE_ENABLE_MASK);// 使能 INTC 中断
    
    //CPU 中断使能
    microblaze_enable_interrupts();
}

void My_ISR()
{
    int status;
    status = Xil_In32(XPAR_AXI_INTC_0_BASEADDR + XIN_ISR_OFFSET);


    //用三个if判断是否是哪个中断触发的，注意这里不能用else if，因为可能同时满足多个中断条件
    if((status & XPAR_AXI_GPIO_0_IP2INTC_IRPT_MASK) == XPAR_AXI_GPIO_0_IP2INTC_IRPT_MASK)
    {
        switch_handle();
    }
    if((status & XPAR_AXI_GPIO_2_IP2INTC_IRPT_MASK) == XPAR_AXI_GPIO_2_IP2INTC_IRPT_MASK)
    {
        button_handle();
    }
    if((status & XPAR_AXI_TIMER_0_INTERRUPT_MASK) == XPAR_AXI_TIMER_0_INTERRUPT_MASK)
    {
        timer_handle();
    }

    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IAR_OFFSET, status);// 清除中断标志位
}

// 开关中断服务程序
void switch_handle()
{
    short hex = Xil_In16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA_OFFSET);
    int segcode_index = 3;
    for(int digit_index = 0; digit_index < 4; digit_index++)
    {
        segcode[segcode_index] = segtable[(hex >> (4 * digit_index)) & 0xf];
        segcode_index --;
    }
    Xil_Out32(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_ISR_OFFSET,
              Xil_In32(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_ISR_OFFSET));
}

// 按键中断服务程序
void button_handle()
{
    char button;
    button = Xil_In8(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_DATA_OFFSET) & 0x1f;
    xil_printf("button %x\n", button);
    if(button == 0x2)  // BTNU 键
    {
        Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TLR_OFFSET,
                  Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TLR_OFFSET) - STEP_PACE);
    }
    if(button == 0x10) // BTND 键
    {
        Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TLR_OFFSET,
                  Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TLR_OFFSET) + STEP_PACE);
    }
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET,
              Xil_In32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET));
}

void timer_handle()
{
    int status;
    // 判断是否 T0 中断
    status = Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET);
    if((status & XTC_CSR_INT_OCCURED_MASK) == XTC_CSR_INT_OCCURED_MASK)
    {
        timer0_handle();
        Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET, status);
    }
    // 判断是否 T1 中断
    status = Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET + XTC_TCSR_OFFSET);
    if((status & XTC_CSR_INT_OCCURED_MASK) == XTC_CSR_INT_OCCURED_MASK)
    {
        timer1_handle();
        Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET + XTC_TCSR_OFFSET, status);
    }
}

// T0 中断事务处理
void timer0_handle()
{
    ledbits++;
    if(ledbits == 16)
    {
        ledbits = 0;
    }
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET, 1 << ledbits);
}

// T1 中断事务处理
void timer1_handle()
{
    Xil_Out16(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA_OFFSET, segcode[pos]);
    Xil_Out16(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA2_OFFSET, poscode[pos]);
    pos++;
    if(pos == 4)
    {
        pos = 0;
    }
}