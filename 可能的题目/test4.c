#include "xparameters.h"
#include <stdio.h>
#include "xil_io.h"
#include "xil_printf.h"
#include "xgpio.h"

int main()
{
    char button;
    unsigned short data1 = 0, data2 = 0;
    unsigned int result;

    // GPIO方向配置
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_TRI_OFFSET, 0xffff); // 开关输入
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_TRI2_OFFSET, 0x0);   // LED输出
    Xil_Out8(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_TRI_OFFSET, 0x1f);    // 按键输入

    xil_printf("\r\nTest4: 程序控制——C读data1/R读data2/U加法/D乘法\r\n");

    while(1)
    {
        button = Xil_In8(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_DATA_OFFSET) & 0x1f;

        // 等待按键按下
        if(button != 0)
        {
            // 等待按键松开
            while((Xil_In8(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_DATA_OFFSET) & 0x1f) != 0);

            xil_printf("Button pressed: 0x%x\r\n", button);

            // C键(bit0)：读入第一个16位二进制数并显示到LED
            if(button & 0x01)
            {
                data1 = Xil_In16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA_OFFSET);
                Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET, data1);
                xil_printf("data1 = 0x%04X (%u)\r\n", data1, data1);
            }
            // R键(bit3)：读入第二个16位二进制数并显示到LED
            else if(button & 0x08)
            {
                data2 = Xil_In16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA_OFFSET);
                Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET, data2);
                xil_printf("data2 = 0x%04X (%u)\r\n", data2, data2);
            }
            // U键(bit1)：无符号加法，结果显示到LED
            else if(button & 0x02)
            {
                result = (unsigned int)data1 + (unsigned int)data2;
                Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET, (unsigned short)result);
                xil_printf("data1 + data2 = %u + %u = %u (0x%08X)\r\n",
                           data1, data2, result, result);
            }
            // D键(bit4)：无符号乘法，结果显示到LED
            else if(button & 0x10)
            {
                result = (unsigned int)data1 * (unsigned int)data2;
                Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET, (unsigned short)result);
                xil_printf("data1 * data2 = %u * %u = %u (0x%08X)\r\n",
                           data1, data2, result, result);
            }
        }
    }
}