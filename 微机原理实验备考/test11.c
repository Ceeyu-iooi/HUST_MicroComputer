#include "xil_io.h"
#include "stdio.h"
#include "xintc_l.h"
#include "xgpio_l.h"
#include "xgpio.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "mb_interface.h"

/* ========== 宏定义 ========== */
/* 中断掩码（对应中断控制器的中断源编号） */
#define GPIO_2_IRQ_MASK  0x2             // GPIO2（按键）中断掩码

/* 按键位掩码（GPIO2_CH1 低5位对应5个独立按键） */
#define BTNC_MASK  0x01                  // 中间按键 C（bit0）
#define BTNU_MASK  0x02                  // 上键 U（bit1）
#define BTNL_MASK  0x04                  // 左键 L（bit2）
#define BTNR_MASK  0x08                  // 右键 R（bit3）
#define BTND_MASK  0x10                  // 下键 D（bit4）

unsigned short led_val = 0x0000;         // 当前LED灯的状态值

/* ========== 函数声明 ========== */
void Initialization(void);
void My_ISR() __attribute__ ((interrupt_handler));
void button_handle(void);

/* ========== main ========== */
int main()
{
    xil_printf("\r\nTest11: 按键C显示开关值到高8位LED, 按键R循环右移LED\r\n");
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

    /* ---- GPIO2（按键）中断使能 ---- */
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET, XGPIO_IR_CH1_MASK);  // 清除中断标志
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_IER_OFFSET, XGPIO_IR_CH1_MASK);  // 使能通道中断
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_GIE_OFFSET, XGPIO_GIE_GINTR_ENABLE_MASK);  // 使能GPIO全局中断

    /* ---- 中断控制器初始化 ---- */
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IAR_OFFSET, GPIO_2_IRQ_MASK);
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IER_OFFSET, GPIO_2_IRQ_MASK);
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_MER_OFFSET,
              XIN_INT_MASTER_ENABLE_MASK | XIN_INT_HARDWARE_ENABLE_MASK);

    microblaze_enable_interrupts();

    /* ---- 初始状态：全部熄灭 ---- */
    led_val = 0x0000;
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET, led_val);

    xil_printf("Ready. Press C to show switch[7:0] on LED[15:8], press R to right-rotate.\r\n");
}

/* ========== 主中断服务函数 ========== */
void My_ISR()
{
    int status = Xil_In32(XPAR_AXI_INTC_0_BASEADDR + XIN_ISR_OFFSET);

    if ((status & GPIO_2_IRQ_MASK) == GPIO_2_IRQ_MASK)
        button_handle();

    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IAR_OFFSET, status);
}

/* ========== GPIO2 按键处理函数 ========== */
/* 功能：按C→U→L→R→D顺序判断按键，执行对应操作
 * C(bit0)：读取开关低8位，映射到LED高8位(bit15-8)，低8位熄灭
 * R(bit3)：LED整体循环右移1位（超出bit0的数据回到bit15）
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

    /* ========== BTNC（bit0）：读取低8位开关值 → LED高8位 ========== */
    if (button & BTNC_MASK)
    {
        /* 读取GPIO0_CH1的低8位，左移8位到高8位 */
        led_val = (Xil_In16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA_OFFSET) & 0x00ff) << 8;
        Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET, led_val);
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

    /* ========== BTNR（bit3）：LED整体循环右移1位 ========== */
    else if (button & BTNR_MASK)
    {
        /* 循环右移：bit0的数据移动到bit15，其余位右移1位 */
        unsigned short lsb = led_val & 0x0001;   // 取出最低位
        led_val = (led_val >> 1) | (lsb << 15);   // 右移1位 + 最低位移到最高位
        Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET, led_val);
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