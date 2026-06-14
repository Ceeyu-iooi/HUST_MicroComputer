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
#define GPIO_2_IRQ_MASK  0x2             // GPIO2（按键）中断掩码：使能了但本测试未使用
#define TIMER_0_IRQ_MASK 0x4             // Timer0（T0+T1共用同一个IP核的同一根中断线）中断掩码

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

int pos = 0;    // 数码管动态扫描当前位置（0~7），T0定时器中断中更新

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
    xil_printf("\r\nTest8: 8位7段数码管每秒滚动显示数字序列3456\r\n");
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

    /* ---- GPIO2（按键）中断使能（本测试未使用按键中断，但保留使能以保持硬件配置一致性） ---- */
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
     * 功能：8位LED每秒滚动显示数字序列"3456"
     * 初始状态：最左边4位(0~3)全灭，最右边4位(4~7)显示"3456"
     * 视觉上表现为：3456 从最右端开始，每秒向左滚动一位 */
    segcode[0] = 0xff;                     // 第1位（最左）：全灭
    segcode[1] = 0xff;                     // 第2位：全灭
    segcode[2] = 0xff;                     // 第3位：全灭
    segcode[3] = 0xff;                     // 第4位：全灭
    segcode[4] = segtable_hex[3];          // 第5位：显示"3"
    segcode[5] = segtable_hex[4];          // 第6位：显示"4"
    segcode[6] = segtable_hex[5];          // 第7位：显示"5"
    segcode[7] = segtable_hex[6];          // 第8位（最右）：显示"6"
}

/* ========== 主中断服务函数 ========== */
/* 功能：读取中断控制器状态寄存器，判断中断源，分别调用对应的处理函数
 * 中断源：T0/T1定时器中断（共享同一根中断线，通过 TIMER_0_IRQ_MASK=0x4 标记） */
void My_ISR()
{
    int status = Xil_In32(XPAR_AXI_INTC_0_BASEADDR + XIN_ISR_OFFSET);  // 读取中断状态寄存器

    /* 判断定时器中断（本测试仅使用定时器中断，按键中断使能了但未使用） */
    if ((status & TIMER_0_IRQ_MASK) == TIMER_0_IRQ_MASK)
        timer_handle();    // T0/T1定时器中断 → 进入分发函数，进一步判断是T0还是T1

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
 * 扫描顺序：pos=0(最左) → pos=1 → ... → pos=7(最右) → pos=0 循环
 * 人眼视觉暂留原理：每位数码管点亮约10μs，每秒刷新约12500次（8位×10μs×12500≈1s），
 * 人眼看不出闪烁，感觉8个数码管同时稳定显示 */
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
/* 功能：每秒将 segcode[0~7] 整体向左循环移位一位
 * 
 * 循环左移逻辑：
 *   char temp = segcode[0];        // 先保存最左端(第1位)的值
 *   for (i=0; i<7; i++)
 *       segcode[i] = segcode[i+1]; // 后一位覆盖前一位（向左移）
 *   segcode[7] = temp;             // 原最左端值循环到最右端(第8位)
 * 
 * 滚动效果时序示例（初始 "3456" 显示在最右边4位）：
 *   t=0： [ff][ff][ff][ff][3][4][5][6]   ← 初始状态
 *   t=1： [ff][ff][ff][3][4][5][6][ff]   ← 1秒后，向左移1位
 *   t=2： [ff][ff][3][4][5][6][ff][ff]
 *   t=3： [ff][3][4][5][6][ff][ff][ff]
 *   t=4： [3][4][5][6][ff][ff][ff][ff]   ← 4秒后，3456移到最左边
 *   t=5： [4][5][6][ff][ff][ff][ff][3]   ← 5秒后，3循环到最右边
 *   ...  循环滚动...
 */
void timer1_handle(void)
{
    char temp = segcode[0];              // 保存最左端(第1位)的值
    for (int i = 0; i < 7; i++)
    {
        segcode[i] = segcode[i + 1];     // 左移：后一位覆盖前一位
    }
    segcode[7] = temp;                   // 原最左端值移到最后端(第8位)

    /* 清除 T1 定时器中断标志 */
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET + XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET + XTC_TCSR_OFFSET) | XTC_CSR_INT_OCCURED_MASK);
}

/* ========== GPIO2 按键空处理函数 ========== */
/* 功能：本测试未使用按键中断。但初始化使能了GPIO2中断，当中断控制器的状态寄存器中
 * GPIO_2_IRQ_MASK 位被置位时（物理上未连接按键，但静电/噪声可能误触发），
 * 如果 My_ISR 中没有清除该标志，会导致重复进入中断。
 * 与 test5~test7 的兼容需要，保留此函数避免编译警告。 */
void button_handle(void)
{
    /* 仅清除 GPIO2 中断标志，不做任何显示操作 */
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET,
              Xil_In32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET));
}