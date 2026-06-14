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

/* --- 定时器重装载值 --- */
#define RESET_VALUE0       1000 - 2           // T0：数码管动态扫描（~10μs/位）
#define RESET_VALUE1       100000000 - 2      // T1：基础周期1s
#define T1_TICK_halfS      50000000 - 2       // T1：0.5s
#define T1_TICK_quarterS   25000000 - 2       // T1：0.25s

/* --- 中断掩码（对应中断控制器的中断源编号） --- */
#define GPIO_2_IRQ_MASK    0x2                // GPIO2（按键）中断掩码
#define TIMER_0_IRQ_MASK   0x4                // Timer0（T0+T1共用同一根中断线）中断掩码

/* --- 按键位掩码（GPIO2_CH1 低5位对应5个独立按键） --- */
#define BTNC_MASK  0x01                       // 中间按键 C（bit0）
#define BTNU_MASK  0x02                       // 上键   U（bit1）
#define BTNL_MASK  0x04                       // 左键   L（bit2）
#define BTNR_MASK  0x08                       // 右键   R（bit3）
#define BTND_MASK  0x10                       // 下键   D（bit4）

/* ========== 数码管表 ========== */

/* 共阳极数码管段码表（0~9, A~F）：dp g f e d c b a，0=亮、1=灭 */
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

/* 负号段码：dp g f e d c b a → 1 0 1 1 1 1 1 1 = 0xBF */
#define SEG_MINUS  0xBF

/* 按键字符段码 */
#define SEG_C   0xC6   // 大写C
#define SEG_U   0xC1   // 大写U
#define SEG_L   0xC7   // 大写L
#define SEG_D   0x86   // 小写d
#define SEG_R   0x86   // 小写r

/* 段码显示缓冲区：segcode[0]（最左）~ segcode[7]（最右），0xff=全灭 */
char segcode[8]   = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

/* 位选码：低电平选中对应位，poscode[0]（最左）~ poscode[7]（最右） */
short poscode[8]  = {0x7F, 0xBF, 0xDF, 0xEF, 0xf7, 0xfb, 0xfd, 0xfe};

/* ========== 全局变量 ========== */
int mode = 0;            // 当前模式
int pos  = 0;            // 数码管动态扫描当前位置（0~7）
char sw_val = 0;         // 开关值
unsigned short led_val = 0x0000;  // LED灯状态

/* ========== 函数声明 ========== */
void Initialization(void);
void My_ISR() __attribute__ ((interrupt_handler));
void switch_handle(void);
void button_handle(void);
void timer_handle(void);
void timer0_handle(void);
void timer1_handle(void);

/* ========== main ========== */
int main()
{
    xil_printf("\r\nTemplate: 微机原理实验模板\r\n");
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

    /* ---- T1 定时器初始化（基础周期1s，软件切换实现变速） ---- */
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET + XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET + XTC_TCSR_OFFSET) & ~XTC_CSR_ENABLE_TMR_MASK);
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET + XTC_TLR_OFFSET, RESET_VALUE1);
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

    /* ---- 初始状态：全部熄灭 ---- */
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET, 0x0000);
    /* segcode默认全0xff（全灭），不需额外设置 */

    xil_printf("Initialization Complete.\r\n");
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

/* ========== 开关处理函数 ========== */
void switch_handle(void)
{
    /* 读取当前独立开关值 */
    sw_val = Xil_In8(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA_OFFSET);

    /* 根据需求处理开关值（待实现具体功能） */
    // 例如：LED实时显示开关状态
    // Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET, sw_val);
}

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

    /* BTNC（bit0）：功能待实现 */
    if (button & BTNC_MASK)
    {
        // 待实现
    }

    /* BTNU（bit1）：功能待实现 */
    else if (button & BTNU_MASK)
    {
        // 待实现
    }

    /* BTNL（bit2）：功能待实现 */
    else if (button & BTNL_MASK)
    {
        // 待实现
    }

    /* BTNR（bit3）：功能待实现 */
    else if (button & BTNR_MASK)
    {
        // 待实现
    }

    /* BTND（bit4）：功能待实现 */
    else if (button & BTND_MASK)
    {
        // 待实现
    }

    /* 清除 GPIO2 中断标志 */
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET,
              Xil_In32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET));
}

/* ========== 定时器中断分发函数 ========== */
void timer_handle(void)
{
    int timer_status;

    /* 检查 T0 是否触发中断 */
    timer_status = Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET);
    if ((timer_status & XTC_CSR_INT_OCCURED_MASK) == XTC_CSR_INT_OCCURED_MASK)
        timer0_handle();

    /* 检查 T1 是否触发中断（用if，不用else if，因为可能同时触发） */
    timer_status = Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET + XTC_TCSR_OFFSET);
    if ((timer_status & XTC_CSR_INT_OCCURED_MASK) == XTC_CSR_INT_OCCURED_MASK)
        timer1_handle();
}

/* ========== T0 定时器中断处理函数（数码管动态扫描） ========== */
void timer0_handle(void)
{
    /* 消影 */
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA_OFFSET,  0xff);
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA2_OFFSET, 0xff);

    /* 输出当前位的位码和段码 */
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA_OFFSET,  poscode[pos]);
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA2_OFFSET, segcode[pos]);

    /* 切换到下一位 */
    pos++;
    if (pos == 8) pos = 0;

    /* 清除 T0 中断标志 */
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET) | XTC_CSR_INT_OCCURED_MASK);
}

/* ========== T1 定时器中断处理函数（周期控制） ========== */
void timer1_handle(void)
{
    /* 根据当前模式执行周期性操作 */
    // if (mode == MODE_X) { ... }

    /* 清除 T1 中断标志 */
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET + XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET + XTC_TCSR_OFFSET) | XTC_CSR_INT_OCCURED_MASK);
}