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
#define RESET_VALUE0  1000 - 2           // T0定时器重装载值（100MHz时钟约10μs），用于数码管动态扫描
#define RESET_VALUE1  100000000 - 2      // T1定时器重装载值（100MHz时钟约1s），用于滚动节奏控制

/* 中断掩码（对应中断控制器的中断源编号） */
#define GPIO_2_IRQ_MASK  0x2             // GPIO2（按键）中断掩码
#define TIMER_0_IRQ_MASK 0x4             // Timer0（T0+T1共用同一个IP核的同一根中断线）中断掩码

/* 按键位掩码（GPIO2_CH1 低5位对应5个独立按键） */
#define BTNC_MASK  0x01                  // 中间按键 C（bit0）
#define BTNU_MASK  0x02                  // 上键 U（bit1）
#define BTNL_MASK  0x04                  // 左键 L（bit2）
#define BTNR_MASK  0x08                  // 右键 R（bit3）
#define BTND_MASK  0x10                  // 下键 D（bit4）

/* 显示模式 */
#define MODE_IDLE   0                    // 初始空闲模式：数码管全灭
#define MODE_SHOW   1                    // 按键C：低4位开关值以16进制显示到segcode[7]（最右）
#define MODE_LEFT   2                    // 按键R（第1次）：自动左移，每秒1位，到最左后回到最右循环
#define MODE_RIGHT  3                    // 按键R（第2次）：自动右移，每秒1位，到最右后回到最左循环

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

/* segcode[0~7]：数码管显示缓冲区，对应最左边~最右边8个数码管的段码。0xff=全灭 */
char segcode[8]   = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
/* poscode[0~7]：数码管位选码，低电平选中对应位。poscode[0]=第1位(最左) ~ poscode[7]=第8位(最右) */
short poscode[8]  = {0x7F, 0xBF, 0xDF, 0xEF, 0xf7, 0xfb, 0xfd, 0xfe};

int pos = 0;                             // 数码管动态扫描当前位置（0~7），T0定时器中断中更新
int mode = MODE_IDLE;                    // 当前显示模式
char sw_val = 0;                         // 保存最新读取的开关值（低4位）

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
    xil_printf("\r\nTest10: 按键C显示开关值到数码管, 按键R控制左右滚动\r\n");
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

    /* ---- T1 定时器初始化（1秒中断一次，控制滚动节奏） ---- */
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
    for(int i = 0; i < 8; i++)
        segcode[i] = 0xff;
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET, 0x0000);

    xil_printf("Ready. Press C to show switch value, R to toggle scroll direction.\r\n");
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

/* ========== T1 定时器中断处理函数（滚动控制，每秒触发一次） ========== */
/* 功能：根据当前模式执行滚动操作
 * MODE_LEFT  → segcode[0~7]整体循环左移1位（最左→最右）
 * MODE_RIGHT → segcode[0~7]整体循环右移1位（最右→最左）
 * 其他模式 → 不做操作 */
void timer1_handle(void)
{
    if (mode == MODE_LEFT)
    {
        /* 循环左移：segcode[0]→segcode[7]→segcode[6]→...→segcode[1]→segcode[0] */
        char temp = segcode[0];              // 保存最左端
        for (int i = 0; i < 7; i++)
        {
            segcode[i] = segcode[i + 1];     // 后一位覆盖前一位
        }
        segcode[7] = temp;                   // 原最左端到最右端
    }
    else if (mode == MODE_RIGHT)
    {
        /* 循环右移：segcode[7]→segcode[0]→segcode[1]→...→segcode[6]→segcode[7] */
        char temp = segcode[7];              // 保存最右端
        for (int i = 0; i < 7; i++)
        {
            segcode[7 - i] = segcode[6 - i]; // 前一位覆盖后一位（从右向左遍历）
        }
        segcode[0] = temp;                   // 原最右端到最左端
    }
    /* MODE_IDLE/MODE_SHOW：不执行滚动 */

    /* 清除 T1 定时器中断标志 */
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET + XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET + XTC_TCSR_OFFSET) | XTC_CSR_INT_OCCURED_MASK);
}

/* ========== GPIO2 按键处理函数 ========== */
/* 功能：按C→U→L→R→D顺序判断按键，执行对应操作
 * C(bit0)：读取开关低4位，以16进制显示到最右数码管，其他位熄灭
 * R(bit3)：第1次→左移模式，第2次→右移模式，第3次→左移模式，依此类推
 * U/L/D：预留，不做操作 */
void button_handle(void)
{
    char button = Xil_In8(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_DATA_OFFSET) & 0x1f;

    /* 按键松开：仅清除中断标志后返回 */
    if ((button & 0x1f) == 0)
    {
        Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET,
                  Xil_In32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET));
        return;
    }

    /* ========== BTNC（bit0）：读取开关低4位，16进制显示到最右数码管 ========== */
    if (button & BTNC_MASK)
    {
        mode = MODE_SHOW;                              // 切换为显示模式
        sw_val = Xil_In8(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA_OFFSET) & 0x0f;  // 读取低4位
        for (int i = 0; i < 7; i++)
            segcode[i] = 0xff;                         // 左7位熄灭
        segcode[7] = segtable_hex[sw_val];             // 最右位显示开关值的16进制
    }

    /* ========== BTNU（bit1）：预留 ========== */
    else if (button & BTNU_MASK)
    {
        /* 未分配功能 */
    }

    /* ========== BTNL（bit2）：预留 ========== */
    else if (button & BTNL_MASK)
    {
        /* 未分配功能 */
    }

    /* ========== BTNR（bit3）：切换左移/右移模式 ========== */
    else if (button & BTNR_MASK)
    {
        if (mode == MODE_LEFT)
            mode = MODE_RIGHT;                         // 当前左移 → 切换为右移
        else if (mode == MODE_RIGHT)
            mode = MODE_LEFT;                          // 当前右移 → 切换为左移
        else
            mode = MODE_LEFT;                          // 非滚动模式 → 进入左移
    }

    /* ========== BTND（bit4）：预留 ========== */
    else if (button & BTND_MASK)
    {
        /* 未分配功能 */
    }

    /* 清除 GPIO2 中断标志 */
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET,
              Xil_In32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET));
}