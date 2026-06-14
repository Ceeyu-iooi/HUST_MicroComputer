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

/* 滚动模式 */
#define SCROLL_LEFT  0                   // 向左滚动
#define SCROLL_RIGHT 1                   // 向右滚动

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

int pos = 0;         // 数码管动态扫描当前位置（0~7），T0定时器中断中更新
int mode = SCROLL_LEFT;  // 滚动方向：0=向左滚动，1=向右滚动。初始为向左

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
    xil_printf("\r\nTest12: 8位7段数码管显示数字序列3456，按键C开始滚动，L向左移，R向右移\r\n");
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
    /* 按键C(bit0)、L(bit2)、R(bit1)用于控制滚动方向和启停 */
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET, XGPIO_IR_CH1_MASK);  // 清除中断标志
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_IER_OFFSET, XGPIO_IR_CH1_MASK);  // 使能通道中断
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_GIE_OFFSET, XGPIO_GIE_GINTR_ENABLE_MASK);  // 使能GPIO全局中断

    /* ---- T0 定时器初始化（数码管动态扫描，约10μs中断一次） ---- */
    // 1) 关闭定时器
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET) & ~XTC_CSR_ENABLE_TMR_MASK);
    // 2) 设置重装载值
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TLR_OFFSET, RESET_VALUE0);
    // 3) 加载初值到计数器
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET) | XTC_CSR_LOAD_MASK);
    // 4) 配置控制寄存器并启动：使能中断 | 自动重载 | 减计数 | 清除中断标志 | 使能定时器
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
              (Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET) & ~XTC_CSR_LOAD_MASK) |
              XTC_CSR_ENABLE_INT_MASK | XTC_CSR_AUTO_RELOAD_MASK |
              XTC_CSR_DOWN_COUNT_MASK | XTC_CSR_INT_OCCURED_MASK |
              XTC_CSR_ENABLE_TMR_MASK);

    /* ---- T1 定时器初始化（1秒中断一次，控制滚动节奏） ---- */
    /* T1 寄存器地址 = T0基址 + XTC_TIMER_COUNTER_OFFSET（值为0x10）
     * AXI Timer IP核包含2个独立的定时器通道：通道0(T0)从基址+0x00开始，通道1(T1)从基址+0x10开始 */
    // 1) 关闭定时器
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET + XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET + XTC_TCSR_OFFSET) & ~XTC_CSR_ENABLE_TMR_MASK);
    // 2) 设置重装载值（1秒）
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET + XTC_TLR_OFFSET, RESET_VALUE1);
    // 3) 加载初值到计数器
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET + XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET + XTC_TCSR_OFFSET) | XTC_CSR_LOAD_MASK);
    // 4) 配置控制寄存器并启动：使能中断 | 自动重载 | 减计数 | 清除中断标志 | 使能定时器
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET + XTC_TCSR_OFFSET,
              (Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET + XTC_TCSR_OFFSET) & ~XTC_CSR_LOAD_MASK) |
              XTC_CSR_ENABLE_INT_MASK | XTC_CSR_AUTO_RELOAD_MASK |
              XTC_CSR_DOWN_COUNT_MASK | XTC_CSR_INT_OCCURED_MASK |
              XTC_CSR_ENABLE_TMR_MASK);

    /* ---- 中断控制器初始化 ---- */
    // 清除 GPIO2、Timer0 的中断标志（写中断应答寄存器 IAR）
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IAR_OFFSET,
              GPIO_2_IRQ_MASK | TIMER_0_IRQ_MASK);
    // 使能 GPIO2、Timer0 的中断
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IER_OFFSET,
              GPIO_2_IRQ_MASK | TIMER_0_IRQ_MASK);
    // 使能中断控制器（主使能 + 硬件使能）
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_MER_OFFSET,
              XIN_INT_MASTER_ENABLE_MASK | XIN_INT_HARDWARE_ENABLE_MASK);

    /* 使能 CPU 全局中断（MicroBlaze 的 MSR[IE] 位） */
    microblaze_enable_interrupts();

    /* ---- 设置初始显示内容 ---- */
    /* 8个数码管从最左到最右编号 0~7
     * 初始状态：最左边4位(0~3)全灭，最右边4位(4~7)显示"3456"
     * 按键C按下 → 开始自动向左滚动（每秒1位）
     * 按键L按下 → 向左滚动
     * 按键R按下 → 向右滚动 */
    segcode[0] = 0xff;                     // 第1位（最左）：全灭
    segcode[1] = 0xff;                     // 第2位：全灭
    segcode[2] = 0xff;                     // 第3位：全灭
    segcode[3] = 0xff;                     // 第4位：全灭
    segcode[4] = segtable_hex[3];          // 第5位：显示"3"
    segcode[5] = segtable_hex[4];          // 第6位：显示"4"
    segcode[6] = segtable_hex[5];          // 第7位：显示"5"
    segcode[7] = segtable_hex[6];          // 第8位（最右）：显示"6"

    mode = SCROLL_LEFT;                    // 初始滚动方向：向左
}

/* ========== 主中断服务函数 ========== */
/* 功能：读取中断控制器状态寄存器，判断中断源，分别调用对应的处理函数
 * 中断源：GPIO2按键中断（0x2），Timer0的T0/T1定时器中断（0x4） */
void My_ISR()
{
    int status = Xil_In32(XPAR_AXI_INTC_0_BASEADDR + XIN_ISR_OFFSET);  // 读取中断状态寄存器

    /* 判断按键中断（GPIO2_CH1，对应BTNC/BTNL/BTNR） */
    if ((status & GPIO_2_IRQ_MASK) == GPIO_2_IRQ_MASK)
        button_handle();    // 按键处理函数

    /* 判断定时器中断（T0/T1共用TIMER_0_IRQ_MASK） */
    if ((status & TIMER_0_IRQ_MASK) == TIMER_0_IRQ_MASK)
        timer_handle();     // T0/T1定时器中断 → 进入分发函数

    /* 写回 IAR（中断应答寄存器），清除本次已处理的中断 */
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IAR_OFFSET, status);
}

/* ========== 定时器中断分发函数 ========== */
/* 功能：T0和T1共用同一根中断线进入 My_ISR，此处分别读取各自的 TCSR 寄存器
 * 判断哪个定时器的 INT_OCCURED 位被置位，再分别调用对应的处理函数 */
void timer_handle(void)
{
    int timer_status;

    /* 判断 T0 是否触发中断：读取 T0 的 TCSR 寄存器，检查 INT_OCCURED 位 */
    timer_status = Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET);
    if ((timer_status & XTC_CSR_INT_OCCURED_MASK) == XTC_CSR_INT_OCCURED_MASK)
    {
        timer0_handle();  // T0中断 → 数码管动态扫描
    }

    /* 判断 T1 是否触发中断：读取 T1 的 TCSR 寄存器（地址偏移 +0x10），检查 INT_OCCURED 位 */
    timer_status = Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET + XTC_TCSR_OFFSET);
    if ((timer_status & XTC_CSR_INT_OCCURED_MASK) == XTC_CSR_INT_OCCURED_MASK)
    {
        timer1_handle();  // T1中断 → 每1秒滚动一次显示内容
    }
}

/* ========== T0 定时器中断处理函数（数码管动态扫描，约10μs/位） ========== */
/* 功能：以约10μs的间隔依次扫描8个数码管，利用人眼视觉暂留实现稳定显示
 * 扫描顺序：pos=0(最左) → pos=1 → ... → pos=7(最右) → pos=0 循环 */
void timer0_handle(void)
{
    /* 消影：先关闭所有位选和段码输出，防止切换瞬间显示上一位的残影 */
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA_OFFSET,  0xff);   // 位选端口输出全1（高电平=全部不选中）
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA2_OFFSET, 0xff);   // 段码端口输出全1（全灭）

    /* 输出当前位的位码和段码 */
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA_OFFSET,  poscode[pos]);   // 位码：选中第 pos 个数码管（低电平选中）
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA2_OFFSET, segcode[pos]);   // 段码：显示对应的字符

    /* 切换到下一位（循环扫描 0→1→2→...→7→0→...） */
    pos++;
    if (pos == 8) pos = 0;

    /* 清除 T0 定时器中断标志，允许下一次定时中断触发 */
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET) | XTC_CSR_INT_OCCURED_MASK);
}

/* ========== T1 定时器中断处理函数（滚动显示，每1秒触发一次） ========== */
/* 功能：每隔1秒，根据 mode 方向将 segcode[0~7] 整体向左或向右循环移位一位
 *
 * mode=SCROLL_LEFT(0)：向左循环移位
 *   数字从左端移出后进入右端，视觉上整体向左移
 *   char temp = segcode[0];            // 保存最左端
 *   for (i=0; i<7; i++)
 *       segcode[i] = segcode[i+1];     // 后→前（左移）
 *   segcode[7] = temp;                 // 原最左端到最右
 *
 * mode=SCROLL_RIGHT(1)：向右循环移位
 *   数字从右端移出后进入左端，视觉上整体向右移
 *   char temp = segcode[7];            // 保存最右端
 *   for (i=0; i<7; i++)
 *       segcode[7-i] = segcode[6-i];   // 前→后（右移）
 *   segcode[0] = temp;                 // 原最右端到最左 */
void timer1_handle(void)
{
    if (mode == SCROLL_LEFT)
    {
        /* 循环左移 */
        char temp = segcode[0];              // 保存最左端(第1位)的值
        for (int i = 0; i < 7; i++)
        {
            segcode[i] = segcode[i + 1];     // 左移：后一位覆盖前一位
        }
        segcode[7] = temp;                   // 原最左端值移到最后端(第8位)
    }
    else  // mode == SCROLL_RIGHT
    {
        /* 循环右移 */
        char temp = segcode[7];              // 保存最右端(第8位)的值
        for (int i = 0; i < 7; i++)
        {
            segcode[7 - i] = segcode[6 - i]; // 右移：前一位覆盖后一位（从右向左遍历以避免覆盖）
        }
        segcode[0] = temp;                   // 原最右端值移到最前端(第1位)
    }

    /* 清除 T1 定时器中断标志 */
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET + XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET + XTC_TCSR_OFFSET) | XTC_CSR_INT_OCCURED_MASK);
}

/* ========== GPIO2 按键处理函数 ========== */
/* 功能：读取GPIO2按键值，按C→U→L→R→D顺序判断按键，执行对应操作
 * C(bit0)=开始自动滚动, L(bit2)=向左滚动, R(bit3)=向右滚动 */
void button_handle(void)
{
    char button = Xil_In8(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_DATA_OFFSET) & 0x1f;

    if ((button & 0x1f) == 0)
    {
        /* 按键松开：仅清除中断标志后返回 */
        Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET,
                  Xil_In32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET));
        return;
    }

    /* ========== BTNC（bit0）：开始自动向左滚动（T1驱动，每秒1位） ========== */
    if (button & BTNC_MASK)
    {
        mode = SCROLL_LEFT;      // 模式0：自动向左滚动（T1中断驱动）
    }

    /* ========== BTNU（bit1）：预留（当前无功能） ========== */
    else if (button & BTNU_MASK)
    {
        /* 未分配功能：可在此扩展 */
    }

    /* ========== BTNL（bit2）：立即循环左移一位 ========== */
    else if (button & BTNL_MASK)
    {
        /* 手动左移一次：segcode整体左移，最左端移到最右端 */
        mode = SCROLL_LEFT;
    }

    /* ========== BTNR（bit3）：向右滚动 ========== */
    else if (button & BTNR_MASK)
    {
        mode = SCROLL_RIGHT;     // 模式1：T1驱动自动向右滚动
    }

    /* ========== BTND（bit4）：预留（当前无功能） ========== */
    else if (button & BTND_MASK)
    {
        /* 未分配功能：可在此扩展 */
    }

    /* 清除 GPIO2 中断标志 */
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET,
              Xil_In32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET));
}
