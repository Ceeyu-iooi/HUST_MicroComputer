#include "xparameters.h" // The hardware configuration describing constants
#include <stdio.h>
#include "xil_io.h"
#include "xil_printf.h"
#include "xgpio.h"


int main()
{
    char button;
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_TRI_OFFSET,0xffff); // 设置开关为输入
    Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_TRI2_OFFSET,0x0); // 设置LED为输出
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_TRI_OFFSET,0x0); // 设置数码管位选为输出
    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_TRI2_OFFSET,0x0); // 设置数码管段码为输出
    Xil_Out8(XPAR_AXI_GPIO_2_BASEADDR + XGPIO_TRI_OFFSET,0x1f); // 设置按键为输入
    short int sw,swFlag;
    sw = 0;
    swFlag = 0;
    xil_printf("\r\nRunning GPIO Test(Poll)\r\n");

    char segtable[5]={0xc6,0xc1,0xc7,0x88,0xa1};//段码表CULRd
    char segcode[8]={0xff,0xff,0xff,0xff,0xFF,0xFF,0xFF,0xFF};//显示缓冲区
    short poscode[8] = {0x7F, 0xBF, 0xDF, 0xEF, 0xf7, 0xfb, 0xfd, 0xfe}; // 8位数码管位码表，低电平选中，从左到右对应第1~8个
    int mask;

    while(1)//主循环，持续检测按键状态并更新数码管显示
    {
        while(((Xil_In8(XPAR_AXI_GPIO_2_BASEADDR+XGPIO_DATA_OFFSET))&0x1f)!=0)//等待按键按下
        {
            button = Xil_In8(XPAR_AXI_GPIO_2_BASEADDR+XGPIO_DATA_OFFSET)&0x1f;
            while((Xil_In8(XPAR_AXI_GPIO_2_BASEADDR+XGPIO_DATA_OFFSET)&0x1f)!=0);//等待按键松开

            xil_printf("The pushed button's code is 0x%x\n",button);

            for(int j=0;j<5;j++)
            {
                if(button&(0x01<<j))
                {
                    mask = j;
                    break;
                }
            }//判断按下的按键，并将对应的段码索引保存在mask中

            //将显示缓冲区向左移动一位，将新段码插入到最右边
            for(int digit_index=0;digit_index<7;digit_index++)
            {
                segcode[digit_index]=segcode[digit_index+1];  
            }
            segcode[7]=segtable[mask];
        }
            //while((Xil_In8(XPAR_AXI_GPIO_2_BASEADDR+XGPIO_DATA_OFFSET)&0x1f)==0)
            //如果没有按键按下，继续循环（或者删掉这个，把前面一个while的括号放在判断按键前，有button按下才更新button值）
                for(int i=0;i<8;i++)//循环显示8位数码管
                {
                    //根据按键选择段码并刷新显示缓冲区
                    
                    
                    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA2_OFFSET,segcode[i]);//输出段码到数码管段码引脚
                    Xil_Out8(XPAR_AXI_GPIO_1_BASEADDR + XGPIO_DATA_OFFSET,poscode[i]); // 输出位码到数码管位选引脚

                    //延时控制，确保数码管显示稳定
                    for(int j=0;j<1000;j++)
                    {
                        swFlag = Xil_In16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_ISR_OFFSET);// 读取状态标志寄存器，查询开关状态
                        if(swFlag&0x0001) // 查询开关状态，若有拨动则处理
                    {
                        sw = Xil_In16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA_OFFSET); // 读取开关状态
                        Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_DATA2_OFFSET,sw); // 点亮对应的 Leds
                        if(sw)//如果开关被按下,因为开关关闭时也会触发中断,所以这里要判断是否是开关按下
                        {
                        xil_printf("Switch = 0x%X\r\n",sw); // 通过 Uart 打印开关状态
                        }
                        Xil_Out16(XPAR_AXI_GPIO_0_BASEADDR + XGPIO_ISR_OFFSET,0x01); // 清除状态标志
                    }
                    };
                }
                

    }
        
}
    


 