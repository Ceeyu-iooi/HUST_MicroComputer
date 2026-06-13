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

#define RESET_VALUE0  1000 - 2  // T0初值，用于数码管动态扫描

#define XPAR_AXI_GPIO_0_IP2INTC_IRPT_MASK 0x1
#define XPAR_AXI_GPIO_2_IP2INTC_IRPT_MASK 0x2
#define XPAR_AXI_TIMER_0_INTERRUPT_MASK   0x4

void Initialization(void);
void My_ISR() __attribute__ ((interrupt_handler));
void switch_handle();
void button_handle();
void timer_handle();

int pos = 0;
unsigned short display_val = 0;
int mode = 0;  // 0=A:4位十六进制, 1=B:带符号十进制, 2=C:开关控制流动方向, 3=D:最高位低8位, 4=E:滚动显示

// 段码表
char segtable[16] = {
    0xC0, 0xF9, 0xA4, 0xB0, 0x99, 0x92, 0x82, 0xF8,
    0x80, 0x90, 0x88, 0x83, 0xC6, 0xA1, 0x86, 0x8E
};
#define SEG_OFF 0xFF
#define SEG_NEG 0xBF

char segcode[8] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
short poscode[8] = {0x7F,0xBF,0xDF,0xEF,0xf7,0xfb,0xfd,0xfe};

// 模式C/D/E使用的滚动（shift）参数
int shift_dir = 0;   // 0=左移,1=右移
int shift_val = 0;

int main()
{
    xil_printf("\r\nTest9: 综合思考题 A~E\r\n");
    Initialization();
    while(1);
}

void Initialization(void)
{
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_TRI_OFFSET, 0xffff);
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_TRI2_OFFSET, 0x0);
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_TRI_OFFSET, 0x0);
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_TRI2_OFFSET, 0x0);
    Xil_Out8(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_TRI_OFFSET, 0x1f);

    Xil_Out32(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_ISR_OFFSET, XGPIO_IR_CH1_MASK);
    Xil_Out32(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_IER_OFFSET, XGPIO_IR_CH1_MASK);
    Xil_Out32(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_GIE_OFFSET,
              XGPIO_GIE_GINTR_ENABLE_MASK);

    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET, XGPIO_IR_CH1_MASK);
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_IER_OFFSET, XGPIO_IR_CH1_MASK);
    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_GIE_OFFSET,
              XGPIO_GIE_GINTR_ENABLE_MASK);

    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET) & ~XTC_CSR_ENABLE_TMR_MASK);
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TLR_OFFSET, RESET_VALUE0);
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET) | XTC_CSR_LOAD_MASK);
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
              (Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET) & ~XTC_CSR_LOAD_MASK) |
              XTC_CSR_ENABLE_INT_MASK | XTC_CSR_AUTO_RELOAD_MASK |
              XTC_CSR_DOWN_COUNT_MASK | XTC_CSR_INT_OCCURED_MASK | XTC_CSR_ENABLE_TMR_MASK);

    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IAR_OFFSET,
              XPAR_AXI_GPIO_0_IP2INTC_IRPT_MASK | XPAR_AXI_GPIO_2_IP2INTC_IRPT_MASK |
              XPAR_AXI_TIMER_0_INTERRUPT_MASK);
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IER_OFFSET,
              XPAR_AXI_GPIO_0_IP2INTC_IRPT_MASK | XPAR_AXI_GPIO_2_IP2INTC_IRPT_MASK |
              XPAR_AXI_TIMER_0_INTERRUPT_MASK);
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_MER_OFFSET,
              XIN_INT_MASTER_ENABLE_MASK | XIN_INT_HARDWARE_ENABLE_MASK);

    microblaze_enable_interrupts();
}

void My_ISR()
{
    int status = Xil_In32(XPAR_AXI_INTC_0_BASEADDR + XIN_ISR_OFFSET);
    if ((status & XPAR_AXI_GPIO_0_IP2INTC_IRPT_MASK) == XPAR_AXI_GPIO_0_IP2INTC_IRPT_MASK) switch_handle();
    if ((status & XPAR_AXI_GPIO_2_IP2INTC_IRPT_MASK) == XPAR_AXI_GPIO_2_IP2INTC_IRPT_MASK) button_handle();
    if ((status & XPAR_AXI_TIMER_0_INTERRUPT_MASK) == XPAR_AXI_TIMER_0_INTERRUPT_MASK) timer_handle();
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IAR_OFFSET, status);
}

void switch_handle()
{
    short sw = Xil_In16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA_OFFSET);
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET, sw);
    display_val = sw;
    xil_printf("Switch = 0x%04X\r\n", sw);
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_ISR_OFFSET,
              Xil_In16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_ISR_OFFSET));
}

void button_handle()
{
    char btn = Xil_In8(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_DATA_OFFSET) & 0x1f;
    if (btn == 0) {
        Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET,
                  Xil_In32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET));
        return;
    }

    // C键 -> 模式A: 4位十六进制显示
    if (btn & 0x01) {
        mode = 0;
        shift_val = display_val;
        xil_printf("Mode A: 16BIT HEX display\r\n");
    }
    // U键 -> 模式B: 带符号十进制(补码)
    else if (btn & 0x02) {
        mode = 1;
        shift_val = display_val;
        xil_printf("Mode B: signed DEC display\r\n");
    }
    // L键 -> 模式C: 开关高4位控制方向(0左移/非0右移)
    else if (btn & 0x04) {
        mode = 2;
        shift_dir = (display_val & 0x8000) ? 1 : 0;
        shift_val = display_val;
        xil_printf("Mode C: direction=%s\r\n", shift_dir ? "Right" : "Left");
    }
    // R键 -> 模式D: 最高位显示低8位
    else if (btn & 0x08) {
        mode = 3;
        shift_val = display_val;
        xil_printf("Mode D: high digit shows low 8 bits\r\n");
    }
    // D键 -> 模式E: 滚动显示3456
    else if (btn & 0x10) {
        mode = 4;
        xil_printf("Mode E: scroll 3456\r\n");
    }

    Xil_Out32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET,
              Xil_In32(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_ISR_OFFSET));
}

// 滚动显示"3456"使用T0定时器
int scroll_frame = 0;
int scroll_offset = 0;

void timer_handle()
{
    // 消隐
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA_OFFSET, 0xff);
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA2_OFFSET, 0xff);

    // 根据当前模式计算段码
    for (int i = 0; i < 8; i++) segcode[i] = SEG_OFF;

    if (mode == 0) {
        // 4位十六进制
        unsigned short v = display_val;
        for (int i = 0; i < 4; i++)
            segcode[i] = segtable[(v >> (12 - 4*i)) & 0xF];
    }
    else if (mode == 1) {
        // 带符号十进制
        short sv = (short)display_val;
        int neg = 0; unsigned short absv;
        if (sv < 0) { neg = 1; absv = (unsigned short)(-sv); }
        else absv = (unsigned short)sv;
        int dc = 0; char d[5] = {0};
        if (absv == 0) { d[0] = 0; dc = 1; }
        else { unsigned short t = absv; while (t) { d[dc++] = t % 10; t /= 10; } }
        if (neg) {
            segcode[0] = SEG_NEG;
            for (int i = 0; i < dc && i < 4; i++)
                segcode[4-dc+i] = segtable[d[dc-1-i]];
        } else {
            for (int i = 0; i < dc && i < 5; i++)
                segcode[5-dc+i] = segtable[d[dc-1-i]];
        }
    }
    else if (mode == 2) {
        // 开关控制方向：每帧移位
        if (scroll_frame == 0) {
            if (shift_dir == 0)
                shift_val = (shift_val << 1) & 0xFFFF;
            else
                shift_val = shift_val >> 1;
        }
        unsigned short v = shift_val;
        for (int i = 0; i < 4; i++)
            segcode[i] = segtable[(v >> (12 - 4*i)) & 0xF];
    }
    else if (mode == 3) {
        // 最高位显示低8位
        unsigned char lo = display_val & 0xFF;
        segcode[0] = segtable[lo >> 4];
        segcode[1] = segtable[lo & 0x0F];
        unsigned short v = display_val;
        for (int i = 2; i < 4; i++)
            segcode[i] = segtable[(v >> (12 - 4*i)) & 0xF];
    }
    else if (mode == 4) {
        // 滚动显示3456
        // 用滚动偏移决定当前显示的子串
        char seg_3456[4] = {0xB0, 0x99, 0x92, 0x82}; // 3,4,5,6
        for (int i = 0; i < 4; i++) {
            int si = (scroll_offset + i) % 4;
            segcode[i] = seg_3456[si];
        }
    }

    // 输出当前位
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA_OFFSET, poscode[pos]);
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA2_OFFSET, segcode[pos]);
    pos++;
    if (pos == 8) {
        pos = 0;
        scroll_frame++;
        if (mode == 4 && scroll_frame >= 200) {  // 约0.2秒滚一次
            scroll_frame = 0;
            scroll_offset = (scroll_offset + 1) % 4;
        }
        else if (mode == 2) {
            scroll_frame = 0;  // 一帧移一位
        }
    }

    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET) | XTC_CSR_INT_OCCURED_MASK);
}