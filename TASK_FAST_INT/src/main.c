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

#define RESET_VALUE0  1000 - 2// T0 初值0.000001s扫描数码管
#define RESET_VALUE1  100000 - 2
#define STEP_PACE 10000000

//宏定义中断掩码
#define XPAR_AXI_GPIO_0_IP2INTC_IRPT_MASK 0x1 
#define XPAR_AXI_GPIO_2_IP2INTC_IRPT_MASK 0x2
#define XPAR_AXI_TIMER_0_INTERRUPT_MASK 0x4

// 快速中断模式下每个外设的中断编号（对应 IP2INTC 掩码的 bit 位）
#define GPIO_0_INTC_ID   0  // 0x1 → bit 0
#define GPIO_2_INTC_ID   1  // 0x2 → bit 1
#define TIMER_0_INTC_ID  2  // 0x4 → bit 2

//函数声明——每个中断服务程序直接声明为快速中断
void Initialization(void);
void switch_handle(void) __attribute__((fast_interrupt));
void button_handle(void) __attribute__((fast_interrupt));
void timer_handle(void) __attribute__((fast_interrupt));

//全局变量
int mask;
int pos = 0;
char segtable[5]={0xc6,0xc1,0xc7,0x88,0xa1};//段码表CULRd
char segcode[8]={0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0};//显示缓冲区
short poscode[8] = {0x7F, 0xBF, 0xDF, 0xEF, 0xf7, 0xfb, 0xfd, 0xfe}; // 8位数码管位码表，低电平选中，从左到右对应第1~8个

int main()
{
    xil_printf("\r\nRunning GPIO Test(Fast Interrupt)\r\n");
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
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET) 
              & ~XTC_CSR_ENABLE_TMR_MASK);// 关闭 T0
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TLR_OFFSET, RESET_VALUE0);// 设置初值
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET)
              | XTC_CSR_LOAD_MASK);// 载入初值
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
              (Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET) &
              ~XTC_CSR_LOAD_MASK) | // 清除加载位
              XTC_CSR_ENABLE_INT_MASK |// 使能 T0 中断
              XTC_CSR_AUTO_RELOAD_MASK|// 使能 T0 自动重载
              XTC_CSR_DOWN_COUNT_MASK | // 设置为减计数模式
              XTC_CSR_INT_OCCURED_MASK|//清除中断标志位
            XTC_CSR_ENABLE_TMR_MASK);// 使能 T0定时器
    //(原状态 & ~屏蔽位) | 开启位1 | 开启位2 | 开启位3 | 开启位4 


    // ============ 快速中断配置 ============

    // 步骤1: 配置 IMR（Interrupt Mode Register）将 GPIO_0、GPIO_2、TIMER_0 设为快速中断模式
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IMR_OFFSET,
              XPAR_AXI_GPIO_0_IP2INTC_IRPT_MASK |
              XPAR_AXI_GPIO_2_IP2INTC_IRPT_MASK |
              XPAR_AXI_TIMER_0_INTERRUPT_MASK);

    // 步骤2: 配置 IVAR（Interrupt Vector Address Register）为每个中断设置独立的向量地址
    // 每个中断对应的 ISR 函数地址写入 IVAR，中断发生时硬件直接跳转
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IVAR_OFFSET + (GPIO_0_INTC_ID * 4),
              (u32)switch_handle);
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IVAR_OFFSET + (GPIO_2_INTC_ID * 4),
              (u32)button_handle);
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IVAR_OFFSET + (TIMER_0_INTC_ID * 4),
              (u32)timer_handle);

    // 步骤3: 初始化 INTC IER/IAR
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IAR_OFFSET,
            XPAR_AXI_GPIO_0_IP2INTC_IRPT_MASK |
              XPAR_AXI_GPIO_2_IP2INTC_IRPT_MASK |
              XPAR_AXI_TIMER_0_INTERRUPT_MASK);// 清除中断标志位

    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IER_OFFSET,
              XPAR_AXI_GPIO_0_IP2INTC_IRPT_MASK |
              XPAR_AXI_GPIO_2_IP2INTC_IRPT_MASK |
              XPAR_AXI_TIMER_0_INTERRUPT_MASK);// 使能中断

    // 步骤4: 使能 INTC（Master Enable + Hardware Enable）
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_MER_OFFSET,
              XIN_INT_MASTER_ENABLE_MASK | XIN_INT_HARDWARE_ENABLE_MASK);

    //CPU 中断使能
    microblaze_enable_interrupts();
}

// ============ 快速中断服务程序（各自独立，无需主分发函数）============

// 开关快速中断服务程序
void switch_handle(void)
{
    short int sw;
    sw = Xil_In16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA_OFFSET); // 读取开关状态
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET,sw); // 点亮对应的 Leds
    if(sw)//如果开关被按下,因为开关关闭时也会触发中断,所以这里要判断是否是开关按下
    {
        xil_printf("Switch = 0x%X\r\n",sw); // 通过 Uart 打印开关状态
    }
    // 清除状态标志
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_ISR_OFFSET,
        Xil_In16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_ISR_OFFSET)); 
}

// 按键快速中断服务程序
void button_handle(void)
{
    char button;
    button = Xil_In8(XPAR_AXI_GPIO_2_BASEADDR+XGPIO_DATA_OFFSET)&0x1f;
        if(button==0)//如果是按键松开引起的中断,则不处理
    {
        Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET,
            Xil_In32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET));//清除状态标志
        return;
    }
    //while((Xil_In8(XPAR_AXI_GPIO_2_BASEADDR+XGPIO_DATA_OFFSET)&0x1f)!=0);//等待按键松开
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
    //更新显示缓冲区,将新的段码放到最右边，其他位向左移动一位
    for(int digit_index=0;digit_index<7;digit_index++)
    {
        segcode[digit_index]=segcode[digit_index+1];  
    }
    segcode[7]=segtable[mask];

    // 清除状态标志
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET,
            Xil_In32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET));


}

// 定时器快速中断服务程序: 只做一件事扫描数码管（0.000001s）（段码和位码已经处理好）
void timer_handle(void)
{
    //xil_printf("TIMER");
    //消影显示
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA_OFFSET,0xff); // 输出位码到数码管位选引脚
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA2_OFFSET,0xff); // 输出段码到数码管段码引脚

    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA_OFFSET,poscode[pos]); // 输出位码到数码管位选引脚
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA2_OFFSET,segcode[pos]);//输出段码到数码管段码引脚
    
    pos++;
    if(pos == 8)
    {
        pos = 0;
    }

    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET) | XTC_CSR_INT_OCCURED_MASK);//清除中断标志位
}