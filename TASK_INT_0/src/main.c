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

#define RESET_VALUE0  100000 - 2// T0 初值 0.001s扫描数码管
#define RESET_VALUE1  100000 - 2
#define STEP_PACE 10000000

//宏定义中断掩码
#define XPAR_AXI_GPIO_0_IP2INTC_IRPT_MASK 0x1 
#define XPAR_AXI_GPIO_2_IP2INTC_IRPT_MASK 0x2
#define XPAR_AXI_TIMER_0_INTERRUPT_MASK 0x4
//函数声明
void Initialization(void);
void My_ISR() __attribute__ ((interrupt_handler));
void switch_handle();
void button_handle();
void timer_handle();

//全局变量
int mask;
int pos = 0;
char segtable[5]={0xc6,0xc1,0xc7,0x88,0xa1};//段码表CULRd
char segcode[8]={0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0};//显示缓冲区
short poscode[8] = {0x7F, 0xBF, 0xDF, 0xEF, 0xf7, 0xfb, 0xfd, 0xfe}; // 8位数码管位码表，低电平选中，从左到右对应第1~8个

int main()
{
    xil_printf("\r\nRunning GPIO Test(Interrupt)\r\n");
    Initialization(); 
    while(1);
}


void Initialization(void)
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
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
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

// 注册主中断服务程序
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
    short int sw;
    sw = Xil_In16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA_OFFSET); // 读取开关状态
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET,sw); // 点亮对应的 Leds
    xil_printf("Switch = 0x%X\r\n",sw); // 通过 Uart 打印开关状态
    
    // 清除状态标志
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_ISR_OFFSET,
        Xil_In16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_ISR_OFFSET)); 
}

// 按键中断服务程序
void button_handle()
{
    char button;
    button = Xil_In8(XPAR_AXI_GPIO_2_BASEADDR+XGPIO_DATA_OFFSET)&0x1f;
    while((Xil_In8(XPAR_AXI_GPIO_2_BASEADDR+XGPIO_DATA_OFFSET)&0x1f)!=0);//等待按键松开
    xil_printf("The pushed button's code is 0x%x\n",button);

    //判断按下的按键，并将对应的段码索引保存在mask中
    for(int j=0;j<5;j++)
    {
        if(button&(0x01<<j))
        {
            mask = j;
            break;
        }
    }

    //根据按键选择段码并刷新显示缓冲区
    for(int digit_index=0;digit_index<8;digit_index++)
    segcode[7-digit_index]=segtable[mask];  

    // 清除状态标志
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET,
            Xil_In32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET));


}

// 定时器中断服务程序: 只做一件事扫描数码管（0.001s）（段码和位码已经处理好）
void timer_handle()
{
    xil_printf("TIMER");
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA2_OFFSET,segcode[mask]);//输出段码到数码管段码引脚
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA_OFFSET,poscode[pos]); // 输出位码到数码管位选引脚
    pos++;
    if(pos == 8)
    {
        pos = 0;
    }
}

