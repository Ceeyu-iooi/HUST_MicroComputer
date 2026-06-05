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
#include "math.h"

// ============================================================
// 宏定义
// ============================================================

// 定时器初值：决定DAC采样率
// 系统时钟100MHz时：采样周期 = (RESET_VALUE + 2) * 10ns
// 默认 ~10kHz 采样率 → RESET_VALUE = 10000 - 2 = 9998
#define TIMER_RESET_VALUE  10000 - 2

// DAC输出幅值范围（8位）
#define MAX_AMPLITUDE 255
#define MIN_AMPLITUDE 16

// 波形查找表大小（每个周期256个采样点）
#define WAVEFORM_SIZE 256

// 任意波形数据点数（键盘输入1个周期8个数据）
#define ARBITRARY_POINTS 8

// 圆周率
#define PI 3.14159265

// ----------------------------------------------------------
// 中断掩码（必须与Vivado中INTC的中断连接顺序一致）
// ----------------------------------------------------------
#define BUTTON_INTC_MASK   0x1     // GPIO_0 按钮 → INTC Intr[0]
#define UART_INTC_MASK     0x2     // AXI UART Lite → INTC Intr[1]
#define TIMER_INTC_MASK    0x4     // Timer_0      → INTC Intr[2]

// ----------------------------------------------------------
// 波形类型定义
// ----------------------------------------------------------
#define WAVE_SINE        0
#define WAVE_TRIANGLE    1
#define WAVE_SAWTOOTH    2
#define WAVE_SQUARE      3
#define WAVE_ARBITRARY   4
#define WAVE_TYPE_COUNT  5

// ----------------------------------------------------------
// 按钮位掩码（Nexys4 DDR 5个独立按键）
// ----------------------------------------------------------
#define BTNU_MASK  0x01   // 上键：增大幅度
#define BTND_MASK  0x02   // 下键：减小幅度
#define BTNL_MASK  0x04   // 左键：减小频率
#define BTNR_MASK  0x08   // 右键：增大频率
#define BTNC_MASK  0x10   // 中键：切换波形类型

// ----------------------------------------------------------
// 频率/幅度调节步长
// ----------------------------------------------------------
#define AMP_STEP      16    // 幅度每次增减 16（共16级可调）
#define FREQ_STEP      1    // 频率步进（系数1~50）

// ============================================================
// 函数声明
// ============================================================
void Initialization(void);
void My_ISR(void) __attribute__((interrupt_handler));

void button_handle(void);
void uart_handle(void);
void timer_handle(void);

void change_waveform(void);
void update_amplitude(int delta);
void update_frequency(int delta);

void generate_sine_table(void);
void generate_triangle_table(void);
void generate_sawtooth_table(void);
void generate_square_table(void);

// ============================================================
// 全局变量
// ============================================================

// 当前波形参数
int current_waveform = WAVE_SINE;    // 当前波形类型
int amplitude = MAX_AMPLITUDE;       // 当前幅度 (16 ~ 255)
int frequency_step = 1;              // 频率系数 (1 ~ 50)
int phase = 0;                       // 当前相位索引 (0 ~ 255)

// 波形查找表（预先计算好的256点数据）
unsigned char sine_table[WAVEFORM_SIZE];
unsigned char triangle_table[WAVEFORM_SIZE];
unsigned char sawtooth_table[WAVEFORM_SIZE];
unsigned char square_table[WAVEFORM_SIZE];

// 任意波形数据（键盘输入，8个数据点）
unsigned char arbitrary_table[ARBITRARY_POINTS];
int arbitrary_count = 0;             // 已接收的任意波形数据计数

// UART接收状态
int uart_rx_pending = 0;             // 是否有新数据待处理

// ============================================================
// main() — 主函数
// ============================================================
int main()
{
    xil_printf("\r\n");
    xil_printf("============================================\r\n");
    xil_printf("  Digital Signal Generator (Interrupt Mode)\r\n");
    xil_printf("  Nexys4 DDR + MicroBlaze\r\n");
    xil_printf("============================================\r\n");

    // 预计算波形查找表
    generate_sine_table();
    generate_triangle_table();
    generate_sawtooth_table();
    generate_square_table();

    // 初始化任意波形表为0
    for (int i = 0; i < ARBITRARY_POINTS; i++) {
        arbitrary_table[i] = 0;
    }

    // 硬件初始化 & 中断使能
    Initialization();

    // 打印操作说明
    xil_printf("\r\n--- Operation Guide ---\r\n");
    xil_printf("BTNC : Switch waveform type\r\n");
    xil_printf("BTNU : Increase amplitude\r\n");
    xil_printf("BTND : Decrease amplitude\r\n");
    xil_printf("BTNR : Increase frequency\r\n");
    xil_printf("BTNL : Decrease frequency\r\n");
    xil_printf("UART : Send 8 bytes for arbitrary waveform\r\n");
    xil_printf("------------------------\r\n");
    xil_printf("Current: SINE | Amp=255 | Freq=1\r\n");
    xil_printf("============================================\r\n\r\n");

    // 主循环：等待中断
    while (1);
}

// ============================================================
// Initialization() — 外设初始化 & 中断配置
// ============================================================
void Initialization(void)
{
    // ----------------------------------------------------------
    // GPIO 方向配置
    // ----------------------------------------------------------
    // GPIO_0 Channel1: 5位按键输入 (BTNU/BTND/BTNL/BTNR/BTNC)
    Xil_Out8(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_TRI_OFFSET, 0x1f);

    // GPIO_1 Channel1: 8位DAC数据输出（连接Pmod JA[0:7]或外部R-2R DAC）
    // Channel2: 不使用
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_TRI_OFFSET, 0x00);

    // GPIO_2 Channel1: 8位LED输出（状态指示）
    // GPIO_2 Channel2: 16位拨码开关输入（预留）
    Xil_Out8(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_TRI_OFFSET, 0x00);
    Xil_Out16(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_TRI2_OFFSET, 0xffff);

    // ----------------------------------------------------------
    // GPIO_0 按键中断使能
    // ----------------------------------------------------------
    Xil_Out32(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_ISR_OFFSET,
              XGPIO_IR_CH1_MASK);        // 清除中断标志
    Xil_Out32(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_IER_OFFSET,
              XGPIO_IR_CH1_MASK);        // 使能通道1中断
    Xil_Out32(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_GIE_OFFSET,
              XGPIO_GIE_GINTR_ENABLE_MASK); // 使能GPIO全局中断

    // ----------------------------------------------------------
    // AXI UART Lite 接收中断使能
    //   控制寄存器偏移: 0xC
    //   使能中断位: bit 4 (0x10)
    // ----------------------------------------------------------
    Xil_Out32(XPAR_AXI_UARTLITE_0_BASEADDR + 0xC,
              Xil_In32(XPAR_AXI_UARTLITE_0_BASEADDR + 0xC) | 0x10);

    // ----------------------------------------------------------
    // Timer_0 初始化（DAC采样定时器）
    // ----------------------------------------------------------
    // 关闭定时器
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET)
              & ~XTC_CSR_ENABLE_TMR_MASK);
    // 设置初值
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TLR_OFFSET,
              TIMER_RESET_VALUE);
    // 加载初值
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET)
              | XTC_CSR_LOAD_MASK);
    // 配置：中断使能 + 自动重载 + 减计数 + 启动
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
              (Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET)
              & ~XTC_CSR_LOAD_MASK)      // 清除加载位
              | XTC_CSR_ENABLE_INT_MASK   // 使能定时器中断
              | XTC_CSR_AUTO_RELOAD_MASK  // 自动重载初值
              | XTC_CSR_DOWN_COUNT_MASK   // 减计数模式
              | XTC_CSR_INT_OCCURED_MASK  // 清除中断标志
              | XTC_CSR_ENABLE_TMR_MASK); // 启动定时器

    // ----------------------------------------------------------
    // AXI INTC 初始化
    // ----------------------------------------------------------
    // 清除所有中断源标志
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IAR_OFFSET,
              BUTTON_INTC_MASK | UART_INTC_MASK | TIMER_INTC_MASK);
    // 使能所有中断源
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IER_OFFSET,
              BUTTON_INTC_MASK | UART_INTC_MASK | TIMER_INTC_MASK);
    // 使能INTC主中断 + 硬件中断使能
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_MER_OFFSET,
              XIN_INT_MASTER_ENABLE_MASK | XIN_INT_HARDWARE_ENABLE_MASK);

    // ----------------------------------------------------------
    // CPU全局中断使能
    // ----------------------------------------------------------
    microblaze_enable_interrupts();
}

// ============================================================
// My_ISR() — 主中断服务程序（标准中断模式）
// 通过INTC状态寄存器判断中断源，分发给对应处理函数
// ============================================================
void My_ISR(void)
{
    int status;
    status = Xil_In32(XPAR_AXI_INTC_0_BASEADDR + XIN_ISR_OFFSET);

    // 使用三个独立的if（非else if），因为可能同时触发多个中断
    if ((status & BUTTON_INTC_MASK) == BUTTON_INTC_MASK)
    {
        button_handle();
    }
    if ((status & UART_INTC_MASK) == UART_INTC_MASK)
    {
        uart_handle();
    }
    if ((status & TIMER_INTC_MASK) == TIMER_INTC_MASK)
    {
        timer_handle();
    }

    // 清除INTC中断标志
    Xil_Out32(XPAR_AXI_INTC_0_BASEADDR + XIN_IAR_OFFSET, status);
}

// ============================================================
// button_handle() — 按键中断服务程序
// 功能：识别按键，切换波形/调节幅度/调节频率
// ============================================================
void button_handle(void)
{
    char button;
    button = Xil_In8(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA_OFFSET) & 0x1f;

    // 按键释放时触发的中断不处理
    if (button == 0)
    {
        Xil_Out32(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_ISR_OFFSET,
                  Xil_In32(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_ISR_OFFSET));
        return;
    }

    // 判断按下的按键并执行相应操作
    // 注意：可能同时按下多个键，逐一判断

    // BTNC（中键）：切换波形类型
    if (button & BTNC_MASK)
    {
        change_waveform();
    }
    // BTNU（上键）：增大幅度
    if (button & BTNU_MASK)
    {
        update_amplitude(AMP_STEP);
    }
    // BTND（下键）：减小幅度
    if (button & BTND_MASK)
    {
        update_amplitude(-AMP_STEP);
    }
    // BTNR（右键）：增大频率
    if (button & BTNR_MASK)
    {
        update_frequency(FREQ_STEP);
    }
    // BTNL（左键）：减小频率
    if (button & BTNL_MASK)
    {
        update_frequency(-FREQ_STEP);
    }

    // 清除GPIO中断标志
    Xil_Out32(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_ISR_OFFSET,
              Xil_In32(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_ISR_OFFSET));
}

// ============================================================
// uart_handle() — UART接收中断服务程序
// 功能：接收键盘输入的任意波形数据（8字节，对应8个采样值）
//       数据范围：0x00 ~ 0xFF（对应DAC输出 0 ~ 3.3V）
// ============================================================
void uart_handle(void)
{
    unsigned char data;

    // 从UART Lite RX FIFO读取接收到的字节（偏移地址0x0）
    data = (unsigned char)Xil_In32(XPAR_AXI_UARTLITE_0_BASEADDR + 0x0);

    // 仅在"任意波形"模式下处理键盘输入
    if (current_waveform == WAVE_ARBITRARY)
    {
        if (arbitrary_count < ARBITRARY_POINTS)
        {
            arbitrary_table[arbitrary_count] = data;
            xil_printf("Arbitrary[%d] = 0x%02X (%d)\r\n",
                       arbitrary_count, data, data);
            arbitrary_count++;

            // 收满8个数据后提示
            if (arbitrary_count >= ARBITRARY_POINTS)
            {
                xil_printf(">> Arbitrary waveform loaded! (8/8 points)\r\n");
                xil_printf(">> Send any 8 bytes again to update.\r\n");
                // 重置计数器，准备接收下一组数据
                arbitrary_count = 0;
            }
        }
    }
    else
    {
        // 非任意波形模式：提示切换波形类型
        xil_printf("UART: 0x%02X (switch to ARBITRARY mode first)\r\n", data);
    }

    // UART Lite中断通过读取RX FIFO自动清除，无需额外操作
}

// ============================================================
// timer_handle() — 定时器中断服务程序（DAC采样输出）
// 功能：根据当前波形类型和参数，输出下一个DAC采样值
//       波形查找表大小: 256点/周期
// ============================================================
void timer_handle(void)
{
    unsigned char dac_value = 0;
    int index;

    // 根据当前波形类型获取采样值
    if (current_waveform == WAVE_ARBITRARY)
    {
        // 任意波形：将8个点线性插值扩展到256点
        // 每32个采样点对应1个任意波形数据点
        index = phase / (WAVEFORM_SIZE / ARBITRARY_POINTS);
        if (index >= ARBITRARY_POINTS) index = ARBITRARY_POINTS - 1;
        dac_value = (arbitrary_table[index] * amplitude) / MAX_AMPLITUDE;
    }
    else
    {
        // 标准波形：从对应查找表中取值并乘以幅度系数
        switch (current_waveform)
        {
            case WAVE_SINE:
                dac_value = (sine_table[phase] * amplitude) / MAX_AMPLITUDE;
                break;
            case WAVE_TRIANGLE:
                dac_value = (triangle_table[phase] * amplitude) / MAX_AMPLITUDE;
                break;
            case WAVE_SAWTOOTH:
                dac_value = (sawtooth_table[phase] * amplitude) / MAX_AMPLITUDE;
                break;
            case WAVE_SQUARE:
                dac_value = (square_table[phase] * amplitude) / MAX_AMPLITUDE;
                break;
            default:
                dac_value = 0;
                break;
        }
    }

    // 输出DAC值到GPIO_1 Channel1（连接外部DAC）
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA_OFFSET, dac_value);

    // 更新LED指示（GPIO_2 Channel1）
    //   LED[2:0]：波形类型  LED[7]：幅度>50%指示灯
    {
        unsigned char led_status;
        led_status = (unsigned char)(current_waveform & 0x07);
        if (amplitude > (MAX_AMPLITUDE / 2))
        {
            led_status |= 0x80;
        }
        Xil_Out8(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_DATA_OFFSET, led_status);
    }

    // 更新相位（受频率系数控制）
    phase += frequency_step;
    if (phase >= WAVEFORM_SIZE)
    {
        phase -= WAVEFORM_SIZE;
    }

    // 清除定时器中断标志
    Xil_Out32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET,
              Xil_In32(XPAR_AXI_TIMER_0_BASEADDR + XTC_TCSR_OFFSET)
              | XTC_CSR_INT_OCCURED_MASK);
}

// ============================================================
// change_waveform() — 切换波形类型
// ============================================================
void change_waveform(void)
{
    current_waveform++;
    if (current_waveform >= WAVE_TYPE_COUNT)
    {
        current_waveform = WAVE_SINE;
    }
    phase = 0;      // 切换波形时复位相位

    // UART打印当前波形
    switch (current_waveform)
    {
        case WAVE_SINE:
            xil_printf("Waveform: SINE\r\n");
            break;
        case WAVE_TRIANGLE:
            xil_printf("Waveform: TRIANGLE\r\n");
            break;
        case WAVE_SAWTOOTH:
            xil_printf("Waveform: SAWTOOTH\r\n");
            break;
        case WAVE_SQUARE:
            xil_printf("Waveform: SQUARE\r\n");
            break;
        case WAVE_ARBITRARY:
            xil_printf("Waveform: ARBITRARY (UART input enabled)\r\n");
            xil_printf(">> Send 8 bytes (0x00~0xFF) via serial port\r\n");
            arbitrary_count = 0;  // 重置接收计数
            break;
    }
}

// ============================================================
// update_amplitude() — 调节幅度
// delta: 正数增大，负数减小
// ============================================================
void update_amplitude(int delta)
{
    int new_amp = amplitude + delta;

    // 限幅
    if (new_amp < MIN_AMPLITUDE)
    {
        new_amp = MIN_AMPLITUDE;
    }
    if (new_amp > MAX_AMPLITUDE)
    {
        new_amp = MAX_AMPLITUDE;
    }

    amplitude = new_amp;
    xil_printf("Amplitude: %d / %d\r\n", amplitude, MAX_AMPLITUDE);
}

// ============================================================
// update_frequency() — 调节频率
// delta: 正数增大频率，负数减小频率
// 频率通过改变"相位步进"实现：步进越大，输出波形频率越高
// ============================================================
void update_frequency(int delta)
{
    int new_step = frequency_step + delta;

    // 限幅
    if (new_step < 1)
    {
        new_step = 1;
    }
    if (new_step > 50)
    {
        new_step = 50;
    }

    frequency_step = new_step;
    xil_printf("Frequency step: %d (higher = faster)\r\n", frequency_step);
}

// ============================================================
// 波形查找表生成函数
// 每个表256个点，范围 0 ~ 255（对应DAC 8位满量程）
// 在初始化时调用一次，运行时直接从表中查值
// ============================================================

// 正弦波：sin(2*PI*i/N)  → 映射到 [0, 255]
void generate_sine_table(void)
{
    int i;
    for (i = 0; i < WAVEFORM_SIZE; i++)
    {
        double val = sin(2.0 * PI * i / WAVEFORM_SIZE);
        // val ∈ [-1, 1] → [0, 255]
        sine_table[i] = (unsigned char)((val + 1.0) * (MAX_AMPLITUDE / 2.0));
    }
}

// 三角波：前半段线性上升，后半段线性下降
void generate_triangle_table(void)
{
    int i;
    int half = WAVEFORM_SIZE / 2;
    for (i = 0; i < half; i++)
    {
        triangle_table[i] = (unsigned char)((unsigned int)(MAX_AMPLITUDE * 2) * i / WAVEFORM_SIZE);
    }
    for (i = half; i < WAVEFORM_SIZE; i++)
    {
        triangle_table[i] = MAX_AMPLITUDE
                            - (unsigned char)((unsigned int)(MAX_AMPLITUDE * 2)
                                              * (i - half) / WAVEFORM_SIZE);
    }
}

// 锯齿波：全程线性上升
void generate_sawtooth_table(void)
{
    int i;
    for (i = 0; i < WAVEFORM_SIZE; i++)
    {
        sawtooth_table[i] = (unsigned char)((unsigned int)MAX_AMPLITUDE * i / WAVEFORM_SIZE);
    }
}

// 方波：前半周期高电平，后半周期低电平
void generate_square_table(void)
{
    int i;
    int half = WAVEFORM_SIZE / 2;
    for (i = 0; i < half; i++)
    {
        square_table[i] = MAX_AMPLITUDE;
    }
    for (i = half; i < WAVEFORM_SIZE; i++)
    {
        square_table[i] = 0x00;
    }
}