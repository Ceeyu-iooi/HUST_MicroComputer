#include "xparameters.h" // The hardware configuration describing constants
#include <stdio.h>
#include "xil_io.h"
#include "xil_printf.h"
#include "xgpio.h"


//switch
#define Sw_DATA 0x40000000 // Switch 数据寄存器地址
#define Sw_TRI 0x40000004 // Switch 控制寄存器地址
#define Sw_ISR 0x40000120 // Switch 中断状态寄存器地址
//led
#define Led_DATA 0x40000008 // Leds 数据寄存器地址
#define Led_TRI 0x4000000C // Leds 控制寄存器地址


int main(void)
{
short int sw,swFlag;
sw = 0;
swFlag = 0;
 xil_printf("\r\nRunning GPIO Test(Poll)\r\n");
 Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_TRI_OFFSET,0xffff); // Sw 输入设置;
 Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_TRI2_OFFSET,0x0000); // Led 输出设置;
 while(1)
 {
 swFlag = Xil_In16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_ISR_OFFSET);
 if(swFlag&0x0001) // 查询开关状态，若有拨动则处理
 {
 sw = Xil_In16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA_OFFSET); // 读取开关状态
 Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET,sw); // 点亮对应的 Leds
 xil_printf("Switch = 0x%X\r\n",sw); // 通过 Uart 打印开关状态
 Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_ISR_OFFSET,0x01); // 清除状态标志
 }
 }
 return 0;
}