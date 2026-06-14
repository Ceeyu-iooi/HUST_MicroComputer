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
#define RESET_VALUE0  1000 - 2   // T0 数码管扫描定时器初值(~10μs，100MHz时钟)

/* 中断掩码 */
#define GPIO_0_IRQ_MASK  0x1     // GPIO0（开关）中断掩码（未使用）
#define GPIO_2_IRQ_MASK  0x2     // GPIO2（按键）中断掩码
#define TIMER_0_IRQ_MASK 0x4     // Timer0（T0/T1共用）中断掩码

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

/* 数码管显示缓冲区，segcode[0]~segcode[7] 对应最左边~最右边，初始全灭(0xff) */
char segcode[8]   = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
/* 数码管扫描位码，低电平选中，poscode[0]~poscode[7] 对应第1~8个数码管 */
short poscode[8]  = {0x7F, 0xBF, 0xDF, 0xEF, 0xf7, 0xfb, 0xfd, 0xfe};

volatile int pos = 0;            // 当前扫描到的数码管位置（0~7），在中断中修改，需 volatile

/* ========== 函数声明 ========== */
void Initialization(void);
void My_ISR() __attribute__ ((interrupt_handler));
void button_handle(void);
void timer_handle(void);
void timer0_handle(void);
void timer1_handle(void);

/* ========== main ========== */
int main()
{
    xil_printf("\r\nTest6: 按键左/右移显示4位开关16进制值\r\n");
    Initialization();
    while(1);    // 等待中断
    return 0;
}

/* ========== 初始化 ========== */
void Initialization(void)
{
    /* --- GPIO 方向配置 --- */
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_TRI_OFFSET,  0xffff);  // GPIO0_CH1: 独立开关(16位) → 输入
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_TRI2_OFFSET, 0x0);     // GPIO0_CH2: LED灯(16位) → 输出
    Xil_Out8( XPAR_AXI_GPIO_1_BASEADDR + XGPIO_TRI_OFFSET,  0x0);     // GPIO1_CH1: 数码管位选(8位) → 输出
    Xil_Out8( XPAR_AXI_GPIO_1_BASEADDR + XGPIO_TRI2_OFFSET, 0x0);     // GPIO1_CH2: 数码管段码(8位) → 输出
    Xil_Out8( XPAR_AXI_GPIO_2_BASEADDR + XGPIO_TRI_OFFSET,  0x1f);    // GPIO2_CH1: 独立按键(5位) → 输入

    /* --- 按键中断使能 --- */
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET, XGPIO_IR_CH1_MASK);  // 清除按键中断标志
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_IER_OFFSET, XGPIO_IR_CH1_MASK);  // 使能按键中断
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_GIE_OFFSET, XGPIO_GIE_GINTR_ENABLE_MASK);  // 使能GPIO2全局中断

    /* --- 初始化 T0（数码管动态扫描定时器）--- */
    // 1) 关闭定时器
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET) & ~XTC_CSR_ENABLE_TMR_MASK);
    // 2) 设置初值
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TLR_OFFSET, RESET_VALUE0);
    // 3) 加载初值
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET) | XTC_CSR_LOAD_MASK);
    // 4) 配置并启动（自动重载、减计数、中断使能）
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
              (Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET) & ~XTC_CSR_LOAD_MASK) |
              XTC_CSR_ENABLE_INT_MASK | XTC_CSR_AUTO_RELOAD_MASK |
              XTC_CSR_DOWN_COUNT_MASK | XTC_CSR_INT_OCCURED_MASK |
              XTC_CSR_ENABLE_TMR_MASK);

    /* --- 中断控制器初始化 --- */
    // 清空中断标志
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IAR_OFFSET,
              GPIO_2_IRQ_MASK | TIMER_0_IRQ_MASK);
    // 使能 GPIO2 和 Timer0 中断
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IER_OFFSET,
              GPIO_2_IRQ_MASK | TIMER_0_IRQ_MASK);
    // 使能中断控制器（主使能 + 硬件使能）
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_MER_OFFSET,
              XIN_INT_MASTER_ENABLE_MASK | XIN_INT_HARDWARE_ENABLE_MASK);

    /* 使能 CPU 全局中断 */
    microblaze_enable_interrupts();
}

/* ========== 主中断分发函数 ========== */
void My_ISR()
{
    int status = Xil_In32(XPAR_AXI_INTC_0_BASEADDR + XIN_ISR_OFFSET);  // 读取中断状态

    /* 判断哪个中断源触发了中断 */
    if ((status & GPIO_2_IRQ_MASK) == GPIO_2_IRQ_MASK)
        button_handle();   // 按键中断处理
    if ((status & TIMER_0_IRQ_MASK) == TIMER_0_IRQ_MASK)
        timer_handle();    // 定时器中断处理（数码管扫描）

    /* 写回 IAR 清除已处理的中断 */
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IAR_OFFSET, status);
}

/* ========== 按键中断处理 ========== */
void button_handle(void)
{
    /* 读取按键值，取低5位（对应5个独立按键） */
    char button = Xil_In8(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_DATA_OFFSET) & 0x1f;

    /* 按键松开时也会触发中断（电平变化），此时 button==0，不做处理直接返回 */
    if (button == 0)
    {
        Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET,
                  Xil_In32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET));  // 清中断标志
        return;
    }

    /* ========== BTNC（bit0）：数码管最低位显示最右边4位开关的16进制值 ========== */
    if (button & 0x01)
    {
        unsigned short sw = Xil_In16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA_OFFSET);  // 读取当前开关值
        unsigned char low4 = sw & 0xF;  // 取最右边4位
        xil_printf("BTNC: Switch low4=0x%X\r\n", low4);

        /* 熄灭前7位数码管（segcode[0]~segcode[6]） */
        for (int i = 0; i < 7; i++)
        {
            segcode[i] = 0xff;
        }
        /* 最低位（segcode[7]）显示开关低4位的16进制值 */
        segcode[7] = segtable_hex[low4];
    }

    /* ========== BTNL（bit2）：循环左移一位 ========== */
    else if (button & 0x04)
    {
        xil_printf("BTNL: Left Shift\r\n");

        /* 循环左移：segcode[0]←segcode[1]←...←segcode[6]←segcode[7]←segcode[0]
         * 先保存最左边的值 segcode[0]，再逐位左移，最后将保存的值放到最右边 segcode[7]
         * 视觉上：最左边的数字消失，整体向左移一位，最左边的数字循环到最右边 */
        char temp = segcode[0];              // 保存最左端(第1位)的值
        for (int i = 0; i < 7; i++)
        {
            segcode[i] = segcode[i + 1];     // 左移：后一位覆盖前一位
        }
        segcode[7] = temp;                   // 原最左端值移到最后端(第8位)
    }

    /* ========== BTNR（bit3）：循环右移一位 ========== */
    else if (button & 0x08)
    {
        xil_printf("BTNR: Right Shift\r\n");

        /* 循环右移：segcode[7]→segcode[6]→...→segcode[1]→segcode[0]→segcode[7]
         * 先保存最右边的值 segcode[7]，再逐位右移，最后将保存的值放到最左边 segcode[0]
         * 视觉上：最右边的数字消失，整体向右移一位，最右边的数字循环到最左边 */
        char temp = segcode[7];              // 保存最右端(第8位)的值
        for (int i = 0; i < 7; i++)
        {
            segcode[7 - i] = segcode[6 - i]; // 右移：前一位覆盖后一位（从右向左遍历避免覆盖）
        }
        segcode[0] = temp;                   // 原最右端值移到最前端(第1位)
    }

    /* 清除按键中断标志（必须操作，否则不再触发中断） */
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET,
              Xil_In32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET));
}

/* ========== 定时器中断（数码管动态扫描） ========== */
void timer_handle(void)
{
    /* 消影：先关闭所有位选和段码输出，防止切换瞬间出现残影 */
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA_OFFSET,  0xff);   // 位选全灭（高电平=不选中）
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA2_OFFSET, 0xff);   // 段码全灭

    /* 输出当前位的位码和段码 */
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA_OFFSET,  poscode[pos]);    // 位码：选中当前要显示的数码管
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA2_OFFSET, segcode[pos]);    // 段码：显示对应字符

    /* 切换到下一位，循环0→1→2→...→7→0 */
    pos++;
    if (pos == 8) pos = 0;

    /* 清除 T0 中断标志 */
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET) | XTC_CSR_INT_OCCURED_MASK);
}