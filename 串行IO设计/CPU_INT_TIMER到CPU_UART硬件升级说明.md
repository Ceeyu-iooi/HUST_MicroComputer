# CPU_INT_TIMER 项目关键头文件总结

## TASK_INT_0/src/main.c 中引用的头文件

| 头文件 | 路径 | 主要功能 | 关键宏/函数/类型 |
|--------|------|----------|-------------------|
| **xil_types.h** | `bsp/include/xil_types.h` | 定义Xilinx基础数据类型 | `u32`, `s32`, `u16`, `s16`, `u8`, `s8`, `char8`, `UINTPTR`, `XInterruptHandler` 等基本类型 |
| **xil_assert.h** | `bsp/include/xil_assert.h` | 断言API和宏，用于运行时条件检查 | `Xil_AssertVoid()`, `Xil_AssertNonvoid()`, `Xil_AssertVoidAlways()`, `Xil_AssertNonvoidAlways()`, `Xil_AssertSetCallback()`, `Xil_AssertStatus`, `Xil_AssertWait` |
| **xil_io.h** | `bsp/include/xil_io.h` | 提供底层I/O读写操作（32位/16位/8位） | `Xil_In32()`, `Xil_Out32()`, `Xil_In16()`, `Xil_Out16()`, `Xil_In8()`, `Xil_Out8()` |
| **xil_exception.h** | `bsp/include/xil_exception.h` | 异常处理初始化与管理 | `Xil_ExceptionInit()`, `Xil_ExceptionRegisterHandler()`, `Xil_ExceptionEnable()`, `Xil_ExceptionDisable()`, `Xil_ExceptionHandler` 类型 |
| **mb_interface.h** | `bsp/include/mb_interface.h` | MicroBlaze处理器底层接口：中断控制、缓存控制、伪汇编宏、FSL宏 | `microblaze_enable_interrupts()`, `microblaze_disable_interrupts()`, `microblaze_enable_exceptions()`, `microblaze_disable_exceptions()`, `microblaze_register_handler()`, `microblaze_register_exception_handler()`, 伪汇编宏：`mfmsr()`, `mtmsr()`, `mfslr()`, `mtslr()`, FSL访问宏：`getfsl()`, `putfsl()` |
| **xgpio_l.h** | `bsp/include/xgpio_l.h` | GPIO底层驱动：寄存器偏移宏和读写宏 | `XGPIO_DATA_OFFSET (0x0)`, `XGPIO_TRI_OFFSET (0x4)`, `XGPIO_DATA2_OFFSET (0x8)`, `XGPIO_TRI2_OFFSET (0xC)`, `XGPIO_GIE_OFFSET (0x11C)`, `XGPIO_ISR_OFFSET (0x120)`, `XGPIO_IER_OFFSET (0x128)`, `XGPIO_IR_MASK (0x3)`, `XGPIO_IR_CH1_MASK (0x1)`, `XGPIO_IR_CH2_MASK (0x2)`, `XGPIO_GIE_GINTR_ENABLE_MASK (0x80000000)`, `XGpio_WriteReg()`, `XGpio_ReadReg()` |
| **xtmrctr_l.h** | `bsp/include/xtmrctr_l.h` | 定时器/计数器底层驱动：寄存器偏移宏和读写宏 | `XTC_TCSR_OFFSET (0x0)`, `XTC_TLR_OFFSET (0x4)`, `XTC_TCR_OFFSET (0x8)`, `XTC_TCSR_ENABLE_TMR_MASK (0x80)`, `XTC_TCSR_LOAD_MASK (0x20)`, `XTC_TCSR_INT_OCCURED_MASK (0x100)`, `XTC_TCSR_AUTO_RELOAD_MASK (0x10)`, `XTmrCtr_WriteReg()`, `XTmrCtr_ReadReg()`, `XTmrCtr_mWriteReg()`, `XTmrCtr_mReadReg()` |
| **xintc_l.h** | `bsp/include/xintc_l.h` | 中断控制器底层驱动：寄存器偏移宏和读写宏 | `XIN_MER_OFFSET (0x1C)`, `XIN_IER_OFFSET (0x8)`, `XIN_IAR_OFFSET (0xC)`, `XIN_SIE_OFFSET (0x10)`, `XIN_CIE_OFFSET (0x14)`, `XIN_IVR_OFFSET (0x18)`, `XIN_ISR_OFFSET (0x0)`, `XIN_IPR_OFFSET (0x4)`, `XIntc_Out32()`, `XIntc_In32()`, `XIntc_WriteReg()`, `XIntc_ReadReg()` |
| **xparameters.h** | `bsp/include/xparameters.h` | 硬件平台参数定义（自动生成） | `XPAR_MICROBLAZE_0_AXI_INTC_DEVICE_ID`, `XPAR_AXI_INTC_0_BASEADDR`, `XPAR_AXI_INTC_0_GPIO_0_VEC_ID`, `XPAR_AXI_INTC_0_TMRCTR_0_VEC_ID`, `XPAR_GPIO_0_BASEADDR`, `XPAR_GPIO_0_DEVICE_ID`, `XPAR_TMRCTR_0_BASEADDR`, `XPAR_TMRCTR_0_DEVICE_ID`, `XPAR_CPU_CORE_CLOCK_FREQ_HZ` 等硬件地址和ID宏 |
| **xstatus.h** | `bsp/include/xstatus.h` | 通用状态码定义 | `XST_SUCCESS (0)`, `XST_FAILURE (1)`, `XST_DEVICE_NOT_FOUND (2)` 等约100个状态码；`XStatus` 类型定义（即 `s32`） |
| **xil_printf.h** | `bsp/include/xil_printf.h` | 轻量级printf函数（无需标准库大开销） | `xil_printf()`, `xil_vprintf()`, `print()`, `outbyte()`, `inbyte()` |

---

## 头文件层次关系

```
main.c
├── xparameters.h          (硬件平台参数)
│   └── (自动生成，包含所有外设地址和ID)
├── xstatus.h              (状态码定义)
│   ├── xil_types.h        (基础类型)
│   ├── xil_assert.h       (断言)
│   └── bspconfig.h        (BSP配置)
├── xgpio_l.h              (GPIO底层驱动)
│   ├── xil_types.h
│   ├── xil_assert.h
│   └── xil_io.h           (I/O读写)
├── xtmrctr_l.h            (定时器底层驱动)
│   ├── xil_types.h
│   ├── xil_assert.h
│   └── xil_io.h
├── xintc_l.h              (中断控制器底层驱动)
│   ├── xil_types.h
│   ├── xil_assert.h
│   └── xil_io.h
├── xil_exception.h        (异常处理)
│   └── xil_types.h
├── mb_interface.h         (MicroBlaze处理器接口)
│   ├── xil_types.h
│   ├── xil_assert.h
│   ├── xil_exception.h
│   └── bspconfig.h
└── xil_printf.h           (打印输出)
    ├── xil_types.h
    └── xparameters.h