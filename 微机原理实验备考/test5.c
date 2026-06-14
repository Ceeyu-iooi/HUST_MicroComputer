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

/* ========== 宏定义 ========== */
#define RESET_VALUE0  1000 - 2   // 数码管扫描定时器初值(~1us)
#define RESET_VALUE1  100000000 - 2   // 定时器初值(~1s)

/* 中断掩码 */
#define GPIO_0_IRQ_MASK  0x1
#define GPIO_2_IRQ_MASK  0x2
#define TIMER_0_IRQ_MASK 0x4



/* ========== 段码表（0~9, A~F） ========== */
/* 共阳极数码管段码：dp g f e d c b a */
char segtable_hex[16] = {
    0xc0, // 0
    0xf9, // 1
    0xa4, // 2
    0xb0, // 3
    0x99, // 4
    0x92, // 5
    0x82, // 6
    0xf8, // 7
    0x80, // 8
    0x90, // 9
    0x88, // A
    0x83, // B
    0xc6, // C
    0xa1, // D
    0x86, // E
    0x8e  // F
};

char segcode[8]   = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};// 数码管显示缓冲区，初始全灭(0xff)
short poscode[8]  = {0x7F, 0xBF, 0xDF, 0xEF, 0xf7, 0xfb, 0xfd, 0xfe};// 数码管扫描位置码，初始全灭(0xff)

int pos = 0;
volatile unsigned char disp_value = 0;  // 递增显示值(0~255)
volatile int mode = 0;                  // 0=显示开关, 1=递增模式

/* ========== 函数声明 ========== */
void Initialization(void);
void My_ISR() __attribute__ ((interrupt_handler));
void button_handle(void);
void timer_handle(void);

/* ========== main ========== */
int main()
{
    xil_printf("\r\nTest3: 进制转换——二进制/十六进制/十进制显示\r\n");
    Initialization();
    while(1);
    return 0;
}

/* ========== 初始化 ========== */
void Initialization(void)
{
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_TRI_OFFSET,  0xffff);
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_TRI2_OFFSET, 0x0);
    Xil_Out8( XPAR_AXI_GPIO_1_BASEADDR + XGPIO_TRI_OFFSET,  0x0);
    Xil_Out8( XPAR_AXI_GPIO_1_BASEADDR + XGPIO_TRI2_OFFSET, 0x0);
    Xil_Out8( XPAR_AXI_GPIO_2_BASEADDR + XGPIO_TRI_OFFSET,  0x1f);

    /* 按键中断使能（题目3使用按键切模式，不用开关中断） */
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET, XGPIO_IR_CH1_MASK);
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_IER_OFFSET, XGPIO_IR_CH1_MASK);
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_GIE_OFFSET, XGPIO_GIE_GINTR_ENABLE_MASK);
    // 初始化 T0 的计数器部分，作为1us定时器   
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET) & ~XTC_CSR_ENABLE_TMR_MASK);
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TLR_OFFSET, RESET_VALUE0);
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET) | XTC_CSR_LOAD_MASK);
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
              (Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET) & ~XTC_CSR_LOAD_MASK) |
              XTC_CSR_ENABLE_INT_MASK | XTC_CSR_AUTO_RELOAD_MASK |
              XTC_CSR_DOWN_COUNT_MASK | XTC_CSR_INT_OCCURED_MASK |
              XTC_CSR_ENABLE_TMR_MASK);
    // 初始化 T1 的计数器部分，作为1s定时器   
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET+XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR +XTC_TIMER_COUNTER_OFFSET + XTC_TCSR_OFFSET) & ~XTC_CSR_ENABLE_TMR_MASK);
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET+XTC_TLR_OFFSET, RESET_VALUE1);
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET+XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET + XTC_TCSR_OFFSET) | XTC_CSR_LOAD_MASK);
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET+XTC_TCSR_OFFSET,
              (Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET + XTC_TCSR_OFFSET) & ~XTC_CSR_LOAD_MASK) |
              XTC_CSR_ENABLE_INT_MASK | XTC_CSR_AUTO_RELOAD_MASK |
              XTC_CSR_DOWN_COUNT_MASK | XTC_CSR_INT_OCCURED_MASK |
              XTC_CSR_ENABLE_TMR_MASK);

    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IAR_OFFSET,
              GPIO_2_IRQ_MASK | TIMER_0_IRQ_MASK);
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IER_OFFSET,
              GPIO_2_IRQ_MASK | TIMER_0_IRQ_MASK);
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_MER_OFFSET,
              XIN_INT_MASTER_ENABLE_MASK | XIN_INT_HARDWARE_ENABLE_MASK);

    microblaze_enable_interrupts();
}

/* ========== 主中断服务函数 ========== */
void My_ISR()
{
    int status = Xil_In32(XPAR_AXI_INTC_0_BASEADDR + XIN_ISR_OFFSET);

    if ((status & GPIO_2_IRQ_MASK) == GPIO_2_IRQ_MASK)
        button_handle();
    if ((status & TIMER_0_IRQ_MASK) == TIMER_0_IRQ_MASK)
        timer_handle();

    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IAR_OFFSET, status);
}

/* ========== 按键中断处理 ========== */
void button_handle(void)
{
    char button = Xil_In8(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_DATA_OFFSET) & 0x1f;
    if (button == 0)
    {
        Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET,
                  Xil_In32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET));
        return;
    }

    unsigned short sw = Xil_In16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA_OFFSET);// 读取开关状态
    xil_printf("Switch=0x%04X(%u), Button=0x%02X\r\n", sw, sw, button);
    unsigned char low8 = sw & 0xFF;// 取低8位
    /* BTNC(bit0)：显示最右边8位开关的16进制值 */
    if (button & 0x01)
    {
        mode = 0;  // 切换为显示开关模式
        for (int i = 0; i < 6; i++)
        {
            segcode[i] = 0xff;  // 熄灭高6位
        }
        segcode[6] = segtable_hex[(low8 >> 4) & 0xF];
        segcode[7] = segtable_hex[(low8) & 0xF];
    }
    /* BTNL(bit2)：启动递增模式 */
    else if (button & 0x04)
    {
        mode = 1;               // 切换为递增模式
        disp_value = 0;         // 从0开始递增
        
        for (int i = 0; i < 6; i++)
        {
            segcode[i] = 0xff;  // 熄灭高6位
        }
        segcode[6] = segtable_hex[(disp_value >> 4) & 0xF];
        segcode[7] = segtable_hex[disp_value & 0xF];
    }

    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET,
              Xil_In32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET));
}

void timer_handle(void)// 判断定时器中断来源
{
    int timer_status;
    timer_status = Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET);
    // 判断是否是 T0 中断
    if ((timer_status & XTC_CSR_INT_OCCURED_MASK)==XTC_CSR_INT_OCCURED_MASK)
    {
        timer0_handle();
    }
    // 判断是否是 T1 中断
    timer_status = Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET+XTC_TCSR_OFFSET);
    if ((timer_status & XTC_CSR_INT_OCCURED_MASK)==XTC_CSR_INT_OCCURED_MASK)
    {
        timer1_handle();
    }
}

/* ========== 定时器中断（数码管扫描） ========== */
void timer0_handle(void)
{
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA_OFFSET,  0xff);
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA2_OFFSET, 0xff);

    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA_OFFSET,  poscode[pos]);
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA2_OFFSET, segcode[pos]);

    pos++;
    if (pos == 8) pos = 0;

    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET) | XTC_CSR_INT_OCCURED_MASK);
}


void timer1_handle(void)
{
    /* 递增模式：每秒加1，加到255回0 */
    if (mode == 1)
    {
        disp_value++;
        if (disp_value > 255)
        {
            disp_value = 0;
        }
        
        /* 更新数码管显示(最右边2位16进制) */
        segcode[6] = segtable_hex[(disp_value >> 4) & 0xF];
        segcode[7] = segtable_hex[disp_value & 0xF];
    }
    
    /* 清除T1中断标志 */
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET+XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET+XTC_TCSR_OFFSET) | XTC_CSR_INT_OCCURED_MASK);
}
