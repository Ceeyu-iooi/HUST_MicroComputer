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
#define RESET_VALUE0  1000 - 2   // T0定时器重装载值，对应约10μs（100MHz时钟），用于数码管动态扫描

/* 中断掩码（对应中断控制器的中断源编号） */
#define GPIO_0_IRQ_MASK  0x1// GPIO0（开关）中断掩码：开关值变化时触发，用于更新左边4位二进制显示
#define GPIO_2_IRQ_MASK  0x2// GPIO2（按键）中断掩码：BTNC/BTNL 按下时触发
#define TIMER_0_IRQ_MASK 0x4// Timer0（T0）中断掩码：定时溢出触发，用于数码管动态扫描

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

/* segcode[0~7]：数码管显示缓冲区，对应最左边~最右边8个数码管的段码。初始值0xff=全灭 */
char segcode[8]   = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
/* poscode[0~7]：数码管位选码，低电平选中对应位。poscode[0]=第1位(最左) ~ poscode[7]=第8位(最右) */
short poscode[8]  = {0x7F, 0xBF, 0xDF, 0xEF, 0xf7, 0xfb, 0xfd, 0xfe};

int pos = 0;    // 数码管动态扫描当前位置（0~7），定时器中断中更新
int count = 0;  // BTNL 按键状态计数器：0→显示取反值，1→显示原值，交替循环

/* ========== 函数声明 ========== */
void Initialization(void);
void My_ISR() __attribute__ ((interrupt_handler));
void button_handle(void);
void timer_handle(void);
void switch_handle(void);


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
    /* ---- GPIO 方向配置 ---- */
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_TRI_OFFSET,  0xffff);  // GPIO0_CH1: 独立开关(16位) → 输入
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_TRI2_OFFSET, 0x0);     // GPIO0_CH2: LED灯(16位) → 输出
    Xil_Out8( XPAR_AXI_GPIO_1_BASEADDR + XGPIO_TRI_OFFSET,  0x0);     // GPIO1_CH1: 数码管位选(8位) → 输出
    Xil_Out8( XPAR_AXI_GPIO_1_BASEADDR + XGPIO_TRI2_OFFSET, 0x0);     // GPIO1_CH2: 数码管段码(8位) → 输出
    Xil_Out8( XPAR_AXI_GPIO_2_BASEADDR + XGPIO_TRI_OFFSET,  0x1f);    // GPIO2_CH1: 独立按键(5位) → 输入

    /* ---- GPIO0（开关）中断使能：开关值变化时触发，更新左边4位二进制显示 ---- */
    Xil_Out32(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_ISR_OFFSET, XGPIO_IR_CH1_MASK);  // 清除中断标志
    Xil_Out32(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_IER_OFFSET, XGPIO_IR_CH1_MASK);  // 使能通道中断
    Xil_Out32(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_GIE_OFFSET, XGPIO_GIE_GINTR_ENABLE_MASK);  // 使能GPIO全局中断

    /* ---- GPIO2（按键）中断使能：BTNC/BTNL 按下时触发 ---- */
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET, XGPIO_IR_CH1_MASK);  // 清除中断标志
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_IER_OFFSET, XGPIO_IR_CH1_MASK);  // 使能通道中断
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_GIE_OFFSET, XGPIO_GIE_GINTR_ENABLE_MASK);  // 使能GPIO全局中断

    /* ---- T0 定时器初始化（数码管动态扫描） ---- */
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

    /* ---- 中断控制器初始化 ---- */
    // 清除 GPIO0、GPIO2、Timer0 的中断标志
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IAR_OFFSET,
              GPIO_2_IRQ_MASK | TIMER_0_IRQ_MASK|GPIO_0_IRQ_MASK);
    // 使能 GPIO0、GPIO2、Timer0 的中断
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IER_OFFSET,
              GPIO_2_IRQ_MASK | TIMER_0_IRQ_MASK|GPIO_0_IRQ_MASK);
    // 使能中断控制器（主使能 + 硬件使能）
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_MER_OFFSET,
              XIN_INT_MASTER_ENABLE_MASK | XIN_INT_HARDWARE_ENABLE_MASK);

    /* 使能 CPU 全局中断（MicroBlaze 的 MSR[IE] 位） */
    microblaze_enable_interrupts();
}

/* ========== 主中断服务函数 ========== */
void My_ISR()
{
    int status = Xil_In32(XPAR_AXI_INTC_0_BASEADDR + XIN_ISR_OFFSET);  // 读取中断状态寄存器

    /* 判断哪个中断源触发了中断，分别调用对应的处理函数 */
    if ((status & GPIO_2_IRQ_MASK) == GPIO_2_IRQ_MASK)
        button_handle();   // 按键中断 → 处理 BTNC/BTNL
    if ((status & TIMER_0_IRQ_MASK) == TIMER_0_IRQ_MASK)
        timer_handle();    // T0定时器中断 → 数码管动态扫描
    if ((status & GPIO_0_IRQ_MASK) == GPIO_0_IRQ_MASK)
        switch_handle();   // 开关中断 → 更新左边4位二进制显示

    /* 写回 IAR（中断应答寄存器），清除本次已处理的中断，允许下次中断继续触发 */
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IAR_OFFSET, status);
}


/* ========== GPIO0 开关中断处理函数 ========== */
/* 功能：读取最右边4位开关的二进制值，以"0"/"1"形式显示在最左边4个数码管
 * 触发条件：GPIO0 通道1 输入值发生变化时触发（边沿或电平变化）
 * 显示位置：segcode[0]=bit3(最高位) ~ segcode[3]=bit0(最低位) */
void switch_handle(void)
{
    char sw = Xil_In16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA_OFFSET);  // 读取16位独立开关当前值
    char low4 = sw & 0xf;                                                // 取最右边4位（bit3~bit0）

    for (int i = 0; i < 4; i++)  // i=0对应bit3(最高位), i=3对应bit0(最低位)
    {
        if (low4 & (0x8 >> i))   // 用掩码 (0x8>>i) 依次检查 bit3,bit2,bit1,bit0
            segcode[i] = segtable_hex[1];  // 该位为1 → 显示 "1"（段码 0xf9）
        else
            segcode[i] = segtable_hex[0];  // 该位为0 → 显示 "0"（段码 0xc0）
    }

    /* 清除 GPIO0 通道1 的中断标志，否则无法再次触发中断 */
    Xil_Out32(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_ISR_OFFSET,
              Xil_In32(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_ISR_OFFSET)); 
}

/* ========== GPIO2 按键中断处理函数 ========== */
/* 功能：检测 BTNC（bit0）和 BTNL（bit2）按键按下，执行对应显示操作
 * 触发条件：任意按键按下或松开时触发（电平变化）
 * 显示位置：segcode[6]=负号/熄灭, segcode[7]=数值（最右边2位数码管） */
void button_handle(void)
{
    char button = Xil_In8(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_DATA_OFFSET) & 0x1f;  // 读取5个按键电平状态，只取低5位
    if (button == 0)
    {
        /* 按键松开时也会触发中断（电平从低变高），此时 button=0，不做任何显示处理 */
        Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET,
                  Xil_In32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET));  // 仅清除中断标志
        return;
    }

    unsigned short sw = Xil_In16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA_OFFSET);  // 读取当前开关值
    xil_printf("Switch=0x%04X(%u), Button=0x%02X, count=%d\r\n", sw, sw, button, count);
    unsigned char low4 = sw & 0xF;  // 取最右边4位

    /* ========== BTNC（bit0）：显示有符号十进制值 ========== */
    /* 格式：最高位(bit3)为符号位，低3位(bit2~bit0)为数值
     * 负数（bit3=1）：在 segcode[6] 显示负号 "-"，segcode[7] 显示 数值(1~7)
     * 正数（bit3=0）：熄灭负号位 segcode[6]，segcode[7] 显示 数值(0~7) */
    if (button & 0x01)
    {
        char sign= low4 & 0x8;   // 提取符号位（bit3）
        char number = low4 & 0x7; // 提取数值部分（bit2~bit0），范围 0~7

        if(sign)  // 负数（bit3=1）
        {
            segcode[6] = 0xbf;               // 共阳极段码 0xbf：在 segcode[6] 显示负号 "-"（仅g段不亮）
            segcode[7] = segtable_hex[number];  // segcode[7] 显示数值（1~7，对应 -1~-7）
        }
        else      // 正数（bit3=0）
        {
            segcode[6] = 0xff;               // 熄灭 segcode[6]（全灭，不显示负号）
            segcode[7] = segtable_hex[number];  // segcode[7] 显示数值（0~7，对应 0~+7）
        }
    }

    /* ========== BTNL（bit2）：交替显示取反值 / 原值 ========== */
    /* 每按一次切换显示：
     *   count=0 → 显示取反后的十进制值（仅低3位取反，符号位不取反）
     *   count=1 → 恢复显示原值（同 BTNC 的格式）
     * 取反操作：number_inverted = (~low4) & 0x7，只对 bit2~bit0 取反，bit3(符号位)保持不变 */
    else if (button & 0x04)
    {
        char sign= low4 & 0x8;                 // 符号位（bit3）
        char number = low4 & 0x7;              // 原数值（bit2~bit0）
        char number_inverted = (~low4) & 0x7;  // 取反后的数值：低3位逐位取反，bit3符号位被 &0x7 屏蔽

        if(count==0)  // 第奇数次（1,3,5,...）：显示取反值
        {
            if(sign)  // 原数为负数，取反后符号位不变，仍为负数
            {
                segcode[6] = 0xbf;                     // 显示负号（符号位不变）
                segcode[7] = segtable_hex[number_inverted];  // 显示取反后的数值
            }
            else      // 原数为正数，取反后仍为正数
            {
                segcode[6] = 0xff;                     // 熄灭负号位
                segcode[7] = segtable_hex[number_inverted];  // 显示取反后的数值
            }
        count++;  // 切换到下一次状态
        }
        else if(count==1)  // 第偶数次（2,4,6,...）：恢复显示原值（同 BTNC）
        {
            if(sign)  // 原数为负数
            {
                segcode[6] = 0xbf;                     // 显示负号
                segcode[7] = segtable_hex[number];              // 显示原数值
            }
            else      // 原数为正数
            {
                segcode[6] = 0xff;                     // 熄灭负号位
                segcode[7] = segtable_hex[number];              // 显示原数值
            }
            count=0;  // 重置为 0，下次按 BTNL 再次显示取反值
        }


        
    }

    /* 清除 GPIO2 按键中断标志（必须操作，否则不再触发按键中断） */
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET,
              Xil_In32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET));
}

/* ========== T0 定时器中断处理函数（数码管动态扫描） ========== */
/* 功能：以约10μs的间隔依次扫描8个数码管，利用人眼视觉暂留实现稳定显示
 * 扫描顺序：pos=0(最左) → pos=1 → ... → pos=7(最右) → pos=0 循环 */
void timer_handle(void)
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