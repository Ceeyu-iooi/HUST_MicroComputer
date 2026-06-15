/**
 * ============================================================================
 * 2026.6.15 期末考试题目 — 按键 BTNC/BTNL LED 显示与循环移位
 * ============================================================================
 *
 * 【题目】
 * 嵌入式计算机系统将独立按键以及独立开关作为输入设备，LED 灯[7:0]作为输出设备。
 *
 * a. 点按键 BTNC：低八位 LED 灯[7:0] 显示 1111 0000（按下后才显示），其余LED灯熄灭；
 *
 * b. 点按键 BTNL，LED灯整体以1hz自动左移一位，到最左边的时候回到最低位；
 *    （如1111 0000变为1110 0001）
 *    再按一次BTNL，LED灯整体以1hz自动右移一位，到最右边的时候回到最左边；
 *    （如1111 0000变为0110 1000）
 *    再按一次BTNL，LED灯停止，并显示当前LED值。
 *
 * 【要求】
 * a. 普通中断方式实现。向左循环时，到最左边的时候回到最低位；
 *    向右循环时，到最右边的时候回到最高位。
 * b. 实验报告画出硬件原理框图，软件算法、模块组成以及模块之间的接口、软件流程图。
 *
 * ============================================================================
 * 代码实现
 * ============================================================================
 * 平台：CPU_INT_TIMER（AXI GPIO×3 + AXI Timer×1 + INTC×1）
 * 驱动方式：普通中断（Normal Interrupt）
 * 功能概要：
 *   - BTNC 按下 → LED 低8位显示 0xF0（1111 0000），高8位熄灭
 *   - BTNL 三态循环（count % 3 区分）：
 *       0 → 左移模式：T1 定时器 1Hz 循环左移
 *       1 → 右移模式：T1 定时器 1Hz 循环右移
 *       2 → 停止模式：LED 保持当前值
 * ============================================================================
 */

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

/* 定时器重装载值 */
#define RESET_VALUE0  1000 - 2           /* T0: 重装载值=1000，100MHz→100kHz→10μs，驱动数码管动态扫描 */
#define T1_BASE_TICK  100000000 - 2      /* T1: 重装载值=100M，100MHz→1s（1Hz 基准周期） */

/* 中断掩码（对应中断控制器的中断源编号） */
#define GPIO_2_IRQ_MASK  0x2             /* GPIO2（按键）中断掩码 */
#define TIMER_0_IRQ_MASK 0x4             /* Timer0（T0+T1共用同一根中断线）中断掩码 */

/* 按键位掩码（GPIO2_CH1 低5位对应5个独立按键） */
#define BTNC_MASK  0x01                  /* 中间按键 C（bit0） */
#define BTNU_MASK  0x02                  /* 上键 U（bit1） */
#define BTNL_MASK  0x04                  /* 左键 L（bit2） */
#define BTNR_MASK  0x08                  /* 右键 R（bit3） */
#define BTND_MASK  0x10                  /* 下键 D（bit4） */

/* 显示模式 */
#define MODE_IDLE  0                     /* 初始空闲模式 */
#define MODE_C     1                     /* BTNC 模式：LED 显示 0xF0 */
#define MODE_L1    3                     /* BTNL 状态0：LED 自动左移（1Hz） */
#define MODE_L2    4                     /* BTNL 状态1：LED 自动右移（1Hz） */
#define MODE_L3    5                     /* BTNL 状态2：LED 停止，保持当前值 */

/* ========== 段码表（0~9, A~F） ========== */
/* 共阳极数码管段码：dp g f e d c b a，0=亮、1=灭 */
char segtable_hex[16] = {
    0xc0, /* 0 */
    0xf9, /* 1 */
    0xa4, /* 2 */
    0xb0, /* 3 */
    0x99, /* 4 */
    0x92, /* 5 */
    0x82, /* 6 */
    0xf8, /* 7 */
    0x80, /* 8 */
    0x90, /* 9 */
    0x88, /* A */
    0x83, /* B */
    0xc6, /* C */
    0xa1, /* D */
    0x86, /* E */
    0x8e  /* F */
};

/* segcode[0~7]：数码管显示缓冲区，对应最左边~最右边8个数码管的段码。0xff=全灭 */
char segcode[8]   = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
/* poscode[0~7]：数码管位选码，低电平选中对应位。poscode[0]=第1位(最左) ~ poscode[7]=第8位(最右) */
short poscode[8]  = {0x7F, 0xBF, 0xDF, 0xEF, 0xf7, 0xfb, 0xfd, 0xfe};

/* ========== 全局状态变量 ========== */
int count   = 0;                         /* BTNL 按下次数计数器（用于三态循环） */
int mode    = MODE_IDLE;                 /* 当前显示模式 */
int pos     = 0;                         /* 数码管动态扫描当前位置（0~7），T0定时器中断中更新 */
char Led_current = 0;                    /* LED 当前值（低8位），左移/右移时更新 */

/* ========== 函数声明 ========== */
void Initialization(void);
void My_ISR() __attribute__ ((interrupt_handler));
void timer_handle(void);
void timer0_handle(void);
void timer1_handle(void);
void button_handle(void);

/* ========== main 主函数 ========== */
int main()
{
    xil_printf("\r\n2026.6.15 期末考试：BTNC显示0xF0 / BTNL三态循环(左移/右移/停止)\r\n");
    Initialization();
    while(1);  /* 所有功能由中断驱动 */
    return 0;
}

/* ========== 系统初始化 ========== */
void Initialization(void)
{
    /* ---- GPIO 方向配置 ---- */
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_TRI_OFFSET,  0xffff);  /* GPIO0_CH1: 独立开关(16位) → 输入 */
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_TRI2_OFFSET, 0x0);     /* GPIO0_CH2: LED灯(16位) → 输出 */
    Xil_Out8( XPAR_AXI_GPIO_1_BASEADDR + XGPIO_TRI_OFFSET,  0x0);     /* GPIO1_CH1: 数码管位选(8位) → 输出 */
    Xil_Out8( XPAR_AXI_GPIO_1_BASEADDR + XGPIO_TRI2_OFFSET, 0x0);     /* GPIO1_CH2: 数码管段码(8位) → 输出 */
    Xil_Out8( XPAR_AXI_GPIO_2_BASEADDR + XGPIO_TRI_OFFSET,  0x1f);    /* GPIO2_CH1: 独立按键(5位) → 输入 */

    /* ---- GPIO2（按键）中断使能 ---- */
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET, XGPIO_IR_CH1_MASK);  /* 清除中断标志 */
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_IER_OFFSET, XGPIO_IR_CH1_MASK);  /* 使能通道中断 */
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_GIE_OFFSET, XGPIO_GIE_GINTR_ENABLE_MASK);  /* 使能GPIO全局中断 */

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

    /* ---- T1 定时器初始化（1s 基准周期，用于 1Hz LED 移位节奏） ---- */
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
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET, 0x0000);  /* LED 全灭 */

    xil_printf("Ready. Press BTNC or BTNL to start.\r\n");
}

/* ========== 主中断服务函数（普通中断模式统一入口） ========== */
void My_ISR()
{
    int status = Xil_In32(XPAR_AXI_INTC_0_BASEADDR + XIN_ISR_OFFSET);

    /* 判断是否为 GPIO2 按键中断 */
    if ((status & GPIO_2_IRQ_MASK) == GPIO_2_IRQ_MASK)
        button_handle();

    /* 判断是否为 Timer0 定时器中断（T0/T1 共用中断线） */
    if ((status & TIMER_0_IRQ_MASK) == TIMER_0_IRQ_MASK)
        timer_handle();

    /* 写 IAR 寄存器清除已处理的中断 */
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IAR_OFFSET, status);
}

/* ========== 定时器中断分发函数 ========== */
/* 由于 T0 和 T1 是同一个 AXI Timer IP 核的两个独立通道，
 * 共用同一根中断线（TIMER_0_IRQ_MASK=0x4），
 * 需要在软件中分别读取两个通道的 TCSR 寄存器来区分。 */
void timer_handle(void)
{
    int timer_status;

    /* 检查 T0 通道是否有中断发生 */
    timer_status = Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET);
    if ((timer_status & XTC_CSR_INT_OCCURED_MASK) == XTC_CSR_INT_OCCURED_MASK)
        timer0_handle();

    /* 检查 T1 通道是否有中断发生 */
    timer_status = Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET + XTC_TCSR_OFFSET);
    if ((timer_status & XTC_CSR_INT_OCCURED_MASK) == XTC_CSR_INT_OCCURED_MASK)
        timer1_handle();
}

/* ========== T0 定时器中断处理函数（数码管动态扫描，约10μs/位） ========== */
/* 功能：每10μs切换一个数码管位，以约1.25kHz频率刷新8位数码管 */
void timer0_handle(void)
{
    /* 消影：先关闭所有位选和段码，防止鬼影 */
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA_OFFSET,  0xff);
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA2_OFFSET, 0xff);

    /* 输出当前位的位码和段码 */
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA_OFFSET,  poscode[pos]);
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA2_OFFSET, segcode[pos]);

    /* 移动到下一位（循环0→1→...→7→0） */
    pos++;
    if (pos == 8) pos = 0;

    /* 清除 T0 定时器中断标志 */
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET) | XTC_CSR_INT_OCCURED_MASK);
}

/* ========== T1 定时器中断处理函数（1Hz LED 移位节奏控制） ========== */
/* 功能：根据当前模式（mode），每1秒执行一次 LED 移位操作
 *  MODE_L1：LED 整体循环左移一位
 *  MODE_L2：LED 整体循环右移一位
 *  MODE_L3：LED 停止，保持当前值（不做任何操作） */
void timer1_handle(void)
{
    if (mode == MODE_L1)
    {
        /* 循环左移：最高位移出，补到最低位
         * Led_current = ((Led_current<<1) & 0xFF) | ((Led_current>>7) & 0x01)
         * 例：0xF0(1111 0000) → 0xE1(1110 0001) → 0xC3(1100 0011) → ... */
        Led_current = ((Led_current << 1) & 0x00FF) | ((Led_current >> 7) & 0x0001);
        Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET, Led_current & 0x00FF);
    }
    else if (mode == MODE_L2)
    {
        /* 循环右移：最低位移出，补到最高位
         * Led_current = ((Led_current<<7) & 0x80) | ((Led_current>>1) & 0xFF)
         * 例：0xF0(1111 0000) → 0x78(0111 1000) → 0x3C(0011 1100) → ... */
        Led_current = ((Led_current << 7) & 0x0080) | ((Led_current & 0x00FF) >> 1);
        Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET, Led_current & 0x00FF);
    }
    else if (mode == MODE_L3)
    {
        /* 停止模式：不做任何操作，LED 保持当前值 */
        /* 不需要额外操作，led_current 已经在 BTNL 按下时输出到 LED */
    }
    else
    {
        /* 其他模式（MODE_IDLE 等）：无操作 */
    }

    /* 清除 T1 定时器中断标志 */
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET + XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TIMER_COUNTER_OFFSET + XTC_TCSR_OFFSET) | XTC_CSR_INT_OCCURED_MASK);
}

/* ========== GPIO2 按键中断处理函数 ========== */
/* 功能：读取按键值，根据按下的按键切换模式
 *  BTNC → 立即显示 0xF0
 *  BTNL → 三态循环切换（左移/右移/停止） */
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

    /* ========== BTNC（bit0）：LED 低8位显示 0xF0 ========== */
    if (button & BTNC_MASK)
    {
        mode = MODE_C;
        /* 数码管全灭 */
        for (int i = 0; i < 8; i++)
            segcode[i] = 0xff;
        /* LED 低8位显示 1111 0000 (0xF0)，高8位熄灭 */
        Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET, 0x00F0);
        /* 同步更新 Led_current 以备后续 BTNL 移位使用 */
        Led_current = 0xF0;
    }

    /* ========== BTNL（bit2）：三态循环（左移/右移/停止） ========== */
    else if (button & BTNL_MASK)
    {
        /* 首次按下（count=0）时，初始化 Led_current 为 0xF0 */
        if (count == 0)
        {
            Led_current = 0xF0;
        }

        /* count % 3 == 0 → 左移模式 */
        if (count % 3 == 0)
        {
            mode = MODE_L1;
            count++;
        }
        /* count % 3 == 1 → 右移模式 */
        else if (count % 3 == 1)
        {
            mode = MODE_L2;
            count++;
        }
        /* count % 3 == 2 → 停止模式 */
        else if (count % 3 == 2)
        {
            mode = MODE_L3;
            /* 停止时显示当前 LED 值 */
            Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET, Led_current & 0x00FF);
            count++;
        }
    }

    /* 清除 GPIO2 中断标志 */
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET,
              Xil_In32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET));
}