#include "xil_io.h"
#include "xintc_l.h"
#include "xtmrctr_l.h"
#include "xgpio_l.h"
#include "xuartlite_l.h"
#include "xparameters.h"
#include "mb_interface.h"
#include "xil_printf.h"

// 定时器重载值，约10μs用于数码管动态扫描（100MHz时钟）
#define RESET_VALUE0  1000 - 2

// 中断掩码定义（基于硬件中断ID）
// GPIO_0（开关）:  中断号 0 -> bit 0
// GPIO_2（按键）:  中断号 1 -> bit 1
// Timer（定时器）: 中断号 2 -> bit 2
// UART1:           中断号 3 -> bit 3
// UART2:           中断号 4 -> bit 4
#define XPAR_AXI_GPIO_0_IP2INTC_IRPT_MASK    0x1
#define XPAR_AXI_GPIO_2_IP2INTC_IRPT_MASK    0x2
#define XPAR_AXI_TIMER_0_INTERRUPT_MASK      0x4
#define XPAR_AXI_UARTLITE_1_INTERRUPT_MASK   0x8
#define XPAR_AXI_UARTLITE_2_INTERRUPT_MASK   0x10

// 函数声明
void Initialization(void);
void My_ISR() __attribute__((interrupt_handler));
void switch_handle(void);
void button_handle(void);
void uart1_handle(void);
void uart2_handle(void);
void timer_handle(void);

// 全局变量
int pos = 0;
// 段码表：C, U, L, R, d（对应Nexys4 DDR按键位序）
char segtable[5] = {0xc6, 0xc1, 0xc7, 0x88, 0xa1};
// 数码管显示缓冲区，8位（共阳极，0xff为全部熄灭）
char segcode[8] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
// 位选码（低电平有效，从左到右依次选中8个数码管）
short poscode[8] = {0x7F, 0xBF, 0xDF, 0xEF, 0xF7, 0xFB, 0xFD, 0xFE};

// UART接收状态跟踪（用于16位开关值分两次8bit接收）
int sw_byte_count = 0;      // 0=等待高字节，1=等待低字节
unsigned char sw_high_byte = 0;  // 暂存高字节


int main()
{
    xil_printf("\r\nRuning UART Test!\r\n");
    Initialization();
    while (1);
}

void Initialization(void)
{
    // ==========================================
    // GPIO 配置
    // ==========================================

    // GPIO_0: 通道1 = 16位开关（输入），通道2 = 16位LED（输出）
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_TRI_OFFSET, 0xffff);
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_TRI2_OFFSET, 0x0);

    // GPIO_1: 通道1 = 8位位选（输出），通道2 = 8位段码（输出）
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_TRI_OFFSET, 0x0);
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_TRI2_OFFSET, 0x0);

    // GPIO_2: 5位按键（输入）
    Xil_Out8(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_TRI_OFFSET, 0x1f);

    // ==========================================
    // GPIO 中断使能
    // ==========================================

    // GPIO_0（开关）中断使能
    Xil_Out32(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_ISR_OFFSET, XGPIO_IR_CH1_MASK);
    Xil_Out32(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_IER_OFFSET, XGPIO_IR_CH1_MASK);
    Xil_Out32(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_GIE_OFFSET,
              XGPIO_GIE_GINTR_ENABLE_MASK);

    // GPIO_2（按键）中断使能
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET, XGPIO_IR_CH1_MASK);
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_IER_OFFSET, XGPIO_IR_CH1_MASK);
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_GIE_OFFSET,
              XGPIO_GIE_GINTR_ENABLE_MASK);

    // UART中断：仅在INTC中使能，UART控制寄存器中暂不使能。
    // 发送数据时动态使能接收端UART中断，接收完成后关闭，
    // 以防止发送FIFO空导致的中断风暴。
    Xil_Out32(XPAR_AXI_UARTLITE_2_BASEADDR + XUL_CONTROL_REG_OFFSET, XUL_CR_ENABLE_INTR | XUL_CR_FIFO_TX_RESET | XUL_CR_FIFO_RX_RESET);
    Xil_Out32(XPAR_AXI_UARTLITE_1_BASEADDR + XUL_CONTROL_REG_OFFSET, XUL_CR_ENABLE_INTR | XUL_CR_FIFO_TX_RESET | XUL_CR_FIFO_RX_RESET);
    // ==========================================
    // 定时器T0配置（数码管动态扫描）
    // ==========================================
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET)
              & ~XTC_CSR_ENABLE_TMR_MASK);                        // 停止定时器
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TLR_OFFSET, RESET_VALUE0);
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET)
              | XTC_CSR_LOAD_MASK);                               // 装载重载值
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
              (Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET)
               & ~XTC_CSR_LOAD_MASK) |                           // 清除装载位
              XTC_CSR_ENABLE_INT_MASK |                          // 使能中断
              XTC_CSR_AUTO_RELOAD_MASK |                          // 自动重载
              XTC_CSR_DOWN_COUNT_MASK |                           // 递减计数模式
              XTC_CSR_INT_OCCURED_MASK |                          // 清除中断标志
              XTC_CSR_ENABLE_TMR_MASK);                           // 启动定时器

    // ==========================================
    // 中断控制器（INTC）配置
    // ==========================================
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IAR_OFFSET,
              XPAR_AXI_GPIO_0_IP2INTC_IRPT_MASK |
              XPAR_AXI_GPIO_2_IP2INTC_IRPT_MASK |
              XPAR_AXI_TIMER_0_INTERRUPT_MASK |
              XPAR_AXI_UARTLITE_1_INTERRUPT_MASK |
              XPAR_AXI_UARTLITE_2_INTERRUPT_MASK);               // 清除所有pending中断
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IER_OFFSET,
              XPAR_AXI_GPIO_0_IP2INTC_IRPT_MASK |
              XPAR_AXI_GPIO_2_IP2INTC_IRPT_MASK |
              XPAR_AXI_TIMER_0_INTERRUPT_MASK |
              XPAR_AXI_UARTLITE_1_INTERRUPT_MASK |
              XPAR_AXI_UARTLITE_2_INTERRUPT_MASK);               // 使能全部5个中断源
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_MER_OFFSET,
              XIN_INT_MASTER_ENABLE_MASK | XIN_INT_HARDWARE_ENABLE_MASK);

    // ==========================================
    // CPU中断总使能
    // ==========================================
    microblaze_enable_interrupts();
}

// 中断服务主程序：读取INTC ISR寄存器，判断中断源并分派到对应的处理函数。
// 使用多个"if"（而非"else if"）以支持同时处理多个并发中断。
void My_ISR()
{
    int status;
    status = Xil_In32(XPAR_AXI_INTC_0_BASEADDR + XIN_ISR_OFFSET);

    if ((status & XPAR_AXI_GPIO_0_IP2INTC_IRPT_MASK) == XPAR_AXI_GPIO_0_IP2INTC_IRPT_MASK)
        switch_handle();

    if ((status & XPAR_AXI_GPIO_2_IP2INTC_IRPT_MASK) == XPAR_AXI_GPIO_2_IP2INTC_IRPT_MASK)
        button_handle();

    if ((status & XPAR_AXI_TIMER_0_INTERRUPT_MASK) == XPAR_AXI_TIMER_0_INTERRUPT_MASK)
        timer_handle();

    if ((status & XPAR_AXI_UARTLITE_1_INTERRUPT_MASK) == XPAR_AXI_UARTLITE_1_INTERRUPT_MASK)
        uart1_handle();

    if ((status & XPAR_AXI_UARTLITE_2_INTERRUPT_MASK) == XPAR_AXI_UARTLITE_2_INTERRUPT_MASK)
        uart2_handle();

    // 向INTC确认所有中断
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IAR_OFFSET, status);
}

// 开关中断处理函数：
// 读取16位开关值，分两次（高8位+低8位）通过UART1 TX发送到UART2。
void switch_handle(void)
{
    short int sw;
    sw = Xil_In16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA_OFFSET);
    if(sw)
    {
        xil_printf("Switch value: %d\n", sw);
    }
    //无论开关值是否为0，都发送到UART1，以便UART2更新LED显示。
        // 发送高8位
        Xil_Out32(XPAR_AXI_UARTLITE_1_BASEADDR + XUL_TX_FIFO_OFFSET,
                  (sw >> 8) & 0xff);
        // 发送低8位
        Xil_Out32(XPAR_AXI_UARTLITE_1_BASEADDR + XUL_TX_FIFO_OFFSET,
                  sw & 0xff);
        // 重置接收端状态，准备接收新的两字节数据
        sw_byte_count = 0;
     //开启UART2中断使能
    //Xil_Out32(XPAR_AXI_UARTLITE_2_BASEADDR + XUL_CONTROL_REG_OFFSET,
             // XUL_CR_ENABLE_INTR | XUL_CR_FIFO_TX_RESET | XUL_CR_FIFO_RX_RESET);


    // 清除GPIO中断状态
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_ISR_OFFSET,
              Xil_In16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_ISR_OFFSET));
}

// 按键中断处理函数：
// 读取5位按键值，通过UART2 TX发送到UART1，然后使能UART1中断以接收数据。
void button_handle(void)
{
    char button;
    button = Xil_In8(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_DATA_OFFSET) & 0x1f;
    
    if (button == 0)
    {
        // 按键松开——忽略
        Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET,
                  Xil_In32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET));
        return;
    }
    xil_printf("Button value: %d\n", button);
    // 通过UART2 TX发送按键值
    Xil_Out32(XPAR_AXI_UARTLITE_2_BASEADDR + XUL_TX_FIFO_OFFSET, button);

    // 清除GPIO中断状态
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET,
              Xil_In32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET));
}

// UART1中断处理函数：
// 接收UART2 TX发来的按键码，判断按下的按键，更新数码管段码显示缓冲区。
// 使用while循环排空RX FIFO中的所有字节，再复位FIFO，防止数据丢失。
void uart1_handle(void)
{
    while (Xil_In32(XPAR_AXI_UARTLITE_1_BASEADDR + XUL_STATUS_REG_OFFSET) & XUL_SR_RX_FIFO_VALID_DATA)
    {
        unsigned char data = Xil_In32(XPAR_AXI_UARTLITE_1_BASEADDR + XUL_RX_FIFO_OFFSET);
        xil_printf("UART1_handle: 0x%x\n", data);

        // 判断哪个按键被按下，映射到对应的段码
        for (int j = 0; j < 5; j++)
        {
            if (data & (0x01 << j))
            {
                // 显示缓冲区左移一位，新字符从右侧进入
                for (int digit_index = 0; digit_index < 7; digit_index++)
                {
                    segcode[digit_index] = segcode[digit_index + 1];
                }
                segcode[7] = segtable[j];
                break;
            }
        }
    }
    // 清除UART1中断状态（保留中断使能位！）
    Xil_Out32(XPAR_AXI_UARTLITE_1_BASEADDR + XUL_CONTROL_REG_OFFSET,
              XUL_CR_ENABLE_INTR | XUL_CR_FIFO_TX_RESET | XUL_CR_FIFO_RX_RESET);
}

// UART2中断处理函数：
// 接收UART1 TX发来的开关值（分两次8bit接收：先高字节、后低字节），
// 组装为16位后在LED上显示。
void uart2_handle(void)
{
    // 循环读取RX FIFO中所有可用字节，防止xil_printf期间第二个字节被FIFO复位清空
    if(!Xil_In32(XPAR_AXI_UARTLITE_2_BASEADDR + XUL_STATUS_REG_OFFSET) & XUL_SR_RX_FIFO_VALID_DATA)
    return;
    
    unsigned char rx_byte = Xil_In32(XPAR_AXI_UARTLITE_2_BASEADDR + XUL_RX_FIFO_OFFSET);


    if (sw_byte_count == 0)
    {
        // 接收高字节
        sw_high_byte = rx_byte;
        sw_byte_count = 1;
    }
    else
    {
        // 接收低字节，组装完整16位开关值
        unsigned short sw = (sw_high_byte << 8) | rx_byte;
        Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET, sw);
        xil_printf("LED set to: %d\n", sw);
        sw_byte_count = 0;
        // 清除UART2中断状态（必须保留 ENABLE_INTR 位，写 0 等于禁止后续中断）
        //Xil_Out32(XPAR_AXI_UARTLITE_2_BASEADDR + XUL_CONTROL_REG_OFFSET,0); // 重置接收状态，准备下一次接收
}
    }
    

    

// 定时器中断处理函数：
// 8位数码管动态扫描，每次定时中断刷新一个位。
void timer_handle(void)
{
    // 消影——切换位选前先熄灭所有段和位选
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA_OFFSET, 0xff);   // 所有数码管关闭
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA2_OFFSET, 0xff);  // 所有段熄灭

    // 输出当前位置的位选码和段码
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA_OFFSET, poscode[pos]);
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA2_OFFSET, segcode[pos]);

    // 切换到下一位
    pos++;
    if (pos == 8)
        pos = 0;

    // 清除定时器中断标志
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET) | XTC_CSR_INT_OCCURED_MASK);
}