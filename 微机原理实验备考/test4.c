#include "xparameters.h"
#include <stdio.h>
#include "xil_io.h"
#include "xil_printf.h"
#include "xgpio_l.h"

/* ========== 程序控制方式（无中断） ========== */
/* 描述：按键按下时读入开关数据，进行加法/乘法运算，结果输出到LED */

int main()
{
    unsigned short data1 = 0, data2 = 0;
    unsigned int result;
    char button;

    /* GPIO方向配置 */
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_TRI_OFFSET,  0xffff);  // 开关输入
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_TRI2_OFFSET, 0x0);     // LED输出
    Xil_Out8( XPAR_AXI_GPIO_2_BASEADDR + XGPIO_TRI_OFFSET,  0x1f);    // 按键输入

    xil_printf("\r\nTest4: 程序控制——C读data1 / R读data2 / U加法 / D乘法\r\n");

    while(1)
    {
        button = Xil_In8(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_DATA_OFFSET) & 0x1f;

        if (button != 0)
        {
            /* 等待按键松开（防抖+程序控制关键） */
            while ((Xil_In8(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_DATA_OFFSET) & 0x1f) != 0);

            xil_printf("Button pressed: 0x%02X\r\n", button);

            /* A. BTNC(bit0)：读入第一个16位数据并显示到LED */
            if (button & 0x01)
            {
                data1 = Xil_In16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA_OFFSET);
                Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET, data1);
                xil_printf("data1 = 0x%04X (%u)\r\n", data1, data1);
            }
            /* B. BTNR(bit3)：读入第二个16位数据并显示到LED */
            else if (button & 0x08)
            {
                data2 = Xil_In16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA_OFFSET);
                Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET, data2);
                xil_printf("data2 = 0x%04X (%u)\r\n", data2, data2);
            }
            /* C. BTNU(bit1)：无符号加法 */
            else if (button & 0x02)
            {
                result = (unsigned int)data1 + (unsigned int)data2;
                Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET, (unsigned short)result);
                xil_printf("data1 + data2 = %u + %u = %u (0x%08X)\r\n",
                           data1, data2, result, result);
            }
            /* D. BTND(bit4)：无符号乘法 */
            else if (button & 0x10)
            {
                result = (unsigned int)data1 * (unsigned int)data2;
                Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET, (unsigned short)result);
                xil_printf("data1 * data2 = %u * %u = %u (0x%08X)\r\n",
                           data1, data2, result, result);
            }
        }
    }

    return 0;
}