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
#define RESET_VALUE0  1000 - 2           // T0: 重装载值=1000，100MHz时钟→100kHz→10μs，驱动数码管动态扫描
#define T1_BASE_TICK  100000000 - 2       // T1: 重装载值=100M，100MHz时钟→1s（基础周期=1秒）
#define T1_TICK_halfS    50000000-2       // T1: 半秒重装载值=50M，100MHz时钟→0.5s
#define T1_TICK_quarterS 25000000-2       // T1: 四分之一秒重装载值=25M，100MHz时钟→0.25s

int count1=0;
int count2=0;
/* 中断掩码（对应中断控制器的中断源编号） */
#define GPIO_2_IRQ_MASK  0x2             // GPIO2（按键）中断掩码
#define TIMER_0_IRQ_MASK 0x4             // Timer0（T0+T1共用同一个IP核的同一根中断线）中断掩码

/* 按键位掩码（GPIO2_CH1 低5位对应5个独立按键） */
#define BTNC_MASK  0x01                  // 中间按键 C（bit0）
#define BTNR_MASK  0x08                  // 右键 R（bit3）
#define BTNL_MASK  0x04                  // 左键 L（bit2）
#define BTND_MASK  0x10                  // 下键 D（bit4）
#define BTNU_MASK  0x02                  // 上键 U（bit1）

/* 显示模式 */
#define MODE_IDLE  0                     // 初始空闲模式
#define MODE_C     1                     // 按键C：数码管熄灭+流水灯（速度1s/0.5s/0.25s循环）
#define MODE_U     2                     // 按键U：右2位显示原码十进制+T1驱动左移滚动（1s/0.5s循环）
#define MODE_L1     3                     // 按键L：LED显示16位开关值，再按循环左移16次后复位
#define MODE_L2     4                    
#define MODE_L3    5                   

/* ========== 段码表（0~9, A~F） ========== */
/* 共阳极数码管段码：dp g f e d c b a，0=亮、1=灭 */
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

/* 负号段码（"-"）：
 * 共阳极：要使g段（中间横线）亮起，需要g=0
 * dp g f e d c b a → 1 0 1 1 1 1 1 1 = 0xBF */
#define SEG_MINUS  0xBF                  // 负号 "-"

/* segcode[0~7]：数码管显示缓冲区，对应最左边~最右边8个数码管的段码。0xff=全灭 */
char segcode[8]   = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
/* poscode[0~7]：数码管位选码，低电平选中对应位。poscode[0]=第1位(最左) ~ poscode[7]=第8位(最右) */
short poscode[8]  = {0x7F, 0xBF, 0xDF, 0xEF, 0xf7, 0xfb, 0xfd, 0xfe};

int count=0;
int mode = MODE_IDLE;
int pos = 0;                             // 数码管动态扫描当前位置（0~7），T0定时器中断中更新
char ledbits = 0x0000;                         // LED状态（0~15，低4位）
/* ========== 全局状态变量 ========== */
int display_mode = MODE_IDLE;            // 当前显示模式            
char sw_current = 0;                    // 当前开关值（0~15，低4位）
/* ========== 函数声明 ========== */
void Initialization(void);
void My_ISR() __attribute__ ((interrupt_handler));
void timer_handle(void);
void timer0_handle(void);
void timer1_handle(void);
void button_handle(void);


/* ========== main ========== */
int main()
{
    xil_printf("\r\nTest9: 5按键多功能系统(C流水灯/U十进制/L左移/R反码/D二进制)\r\n");
    Initialization();
    while(1);
    return 0;
}

/* ========== 初始化 ========== */
void Initialization(void)
{
    /* ---- GPIO 方向配置 ---- */
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_TRI_OFFSET,  0xffff);  // GPIO0_CH1: 独立开关(16位) → 输入
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_TRI2_OFFSET, 0x0);     // GPIO0_CH2: LED灯(16位) → 输出
    Xil_Out8( XPAR_AXI_GPIO_1_BASEADDR + XGPIO_TRI_OFFSET,  0x0);     // GPIO1_CH1: 数码管位选(8位) → 输出
    Xil_Out8( XPAR_AXI_GPIO_1_BASEADDR + XGPIO_TRI2_OFFSET, 0x0);     // GPIO1_CH2: 数码管段码(8位) → 输出
    Xil_Out8( XPAR_AXI_GPIO_2_BASEADDR + XGPIO_TRI_OFFSET,  0x1f);    // GPIO2_CH1: 独立按键(5位) → 输入

    /* ---- GPIO2（按键）中断使能 ---- */
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET, XGPIO_IR_CH1_MASK);  // 清除中断标志
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_IER_OFFSET, XGPIO_IR_CH1_MASK);  // 使能通道中断
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_GIE_OFFSET, XGPIO_GIE_GINTR_ENABLE_MASK);  // 使能GPIO全局中断

    /* ---- T0 定时器初始化（数码管动态扫描，约10μs中断一次） ---- */
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

    /* ---- T1 定时器初始化（1s基础周期，软件分频实现不同速度） ---- */
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET + XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET + XTC_TCSR_OFFSET) & ~XTC_CSR_ENABLE_TMR_MASK);
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET + XTC_TLR_OFFSET, T1_BASE_TICK);
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET + XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET + XTC_TCSR_OFFSET) | XTC_CSR_LOAD_MASK);
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET + XTC_TCSR_OFFSET,
              (Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET + XTC_TCSR_OFFSET) & ~XTC_CSR_LOAD_MASK) |
              XTC_CSR_ENABLE_INT_MASK | XTC_CSR_AUTO_RELOAD_MASK |
              XTC_CSR_DOWN_COUNT_MASK | XTC_CSR_INT_OCCURED_MASK |
              XTC_CSR_ENABLE_TMR_MASK);

    /* ---- 中断控制器初始化 ---- */
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IAR_OFFSET,
              GPIO_2_IRQ_MASK | TIMER_0_IRQ_MASK);
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IER_OFFSET,
              GPIO_2_IRQ_MASK | TIMER_0_IRQ_MASK);
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_MER_OFFSET,
              XIN_INT_MASTER_ENABLE_MASK | XIN_INT_HARDWARE_ENABLE_MASK);

    microblaze_enable_interrupts();

    /* ---- 初始状态：熄灭所有数码管和LED ---- */
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET, 0x0000);  // LED全灭
    /* segcode默认全0xff（全灭），不需额外设置 */

    xil_printf("Ready. Press C/U/L/R/D to switch mode.\r\n");
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

/* ========== 定时器中断分发函数 ========== */
void timer_handle(void)
{
    int timer_status;

    timer_status = Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET);
    if ((timer_status & XTC_CSR_INT_OCCURED_MASK) == XTC_CSR_INT_OCCURED_MASK)
        timer0_handle();

    timer_status = Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET + XTC_TCSR_OFFSET);
    if ((timer_status & XTC_CSR_INT_OCCURED_MASK) == XTC_CSR_INT_OCCURED_MASK)
        timer1_handle();
}

/* ========== T0 定时器中断处理函数（数码管动态扫描，约10μs/位） ========== */
void timer0_handle(void)
{
    /* 消影 */
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA_OFFSET,  0xff);
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA2_OFFSET, 0xff);

    /* 输出当前位的位码和段码 */
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA_OFFSET,  poscode[pos]);
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA2_OFFSET, segcode[pos]);

    pos++;
    if (pos == 8) pos = 0;

    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET) | XTC_CSR_INT_OCCURED_MASK);
}

/* ========== T1 定时器中断处理函数（基础周期初始为1s，按键可切换速度） ========== */
/* 功能：根据当前模式执行周期性操作
 * MODE_C（BTNC）：流水灯按速度步进（初始=0.5s，再按=0.25s，循环）
 * MODE_U（BTNU）：十进制滚动左移（初始=1s，再按=0.5s，循环）
 * 其他模式：不做周期性操作 */
void timer1_handle(void)
{
    if(mode==MODE_C)
    {  
        
    
    }
    else if(mode==MODE_L1)
    {
            sw_current=((sw_current<<1)&0x00ff)|((sw_current>>7)&0x0001);
            Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET,sw_current&0x00FF);

            count1++;

    }
    else if(mode==MODE_L2)
    {
            sw_current=((sw_current<<7)&0x0080)|(((sw_current&0x00ff)>>1)&0x00ff);
            Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET,sw_current&0x00FF);
            
            count2++;
    }
    else if(mode==MODE_L3)
    {

    }
    else ;
    
  
    /* 清除 T1 定时器中断标志 */
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET + XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET + XTC_TCSR_OFFSET) | XTC_CSR_INT_OCCURED_MASK);
}

/* ========== segcode循环左移 ========== */
/* 功能：将segcode[0~7]整体向左循环移位一位
 * 最左端（segcode[0]）移出后进入最右端（segcode[7]） */


/* ========== GPIO2 按键处理函数 ========== */
void button_handle(void)
{
    char button = Xil_In8(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_DATA_OFFSET) & 0x1f;

    /* 按键松开时直接清除中断标志后返回 */
    if (button == 0)
    {
        Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET,
                  Xil_In32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET));
        return;
    }

    /* ========== BTNC（bit0）：模式A - 流水灯 ========== */
    if (button & BTNC_MASK)
    {
        mode=MODE_C;
        for (int i = 0; i < 8; i++)
            segcode[i] = 0xff;  // 数码管全灭
        Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET, 0x00F0);
        
    }

    /* ========== BTNU（bit1）：模式E - 滚动十进制 ========== */
    else if (button & BTNU_MASK)
    {
         mode=MODE_IDLE;
    }

    /* ========== BTNL（bit2）：模式B - LED=开关+左移 ========== */
    else if (button & BTNL_MASK)
    {
        unsigned short sw = Xil_In16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA_OFFSET);
        if(count==0)
        {
            sw_current=0x00F0;
        }
        if(count%3==0)
        {
            mode=MODE_L1;
            count++;
        }
        else if(count%3==1)
        {
            mode=MODE_L2;
            count++;
        }
        else if(count%3==2)
        {
            mode=MODE_L3;
            Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET, sw_current&0x00ff);
            count++;
        }

            
    }

    /* ========== BTNR（bit3）：模式D - 反码+偶数LED ========== */
    else if (button & BTNR_MASK)
    {
       mode=MODE_IDLE;
    }
    
    /* ========== BTND（bit4）：模式C - 二进制+奇数LED ========== */
    else if (button & BTND_MASK)
    {
         mode=MODE_IDLE;
    }

    /* 清除 GPIO2 中断标志 */
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET,
              Xil_In32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET));
}


