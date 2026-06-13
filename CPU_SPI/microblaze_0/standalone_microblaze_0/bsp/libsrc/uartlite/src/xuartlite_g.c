#include "xuartlite.h"

XUartLite_Config XUartLite_ConfigTable[] __attribute__ ((section (".drvcfg_sec"))) = {

	{
		"xlnx,axi-uartlite-2.0", /* compatible */
		0x40600000, /* reg */
		0x2580, /* xlnx,baudrate */
		0x0, /* xlnx,use-parity */
		0x0, /* xlnx,odd-parity */
		0x8, /* xlnx,data-bits */
		0xffff, /* interrupts */
		0xffff /* interrupt-parent */
	},
	{
		"xlnx,axi-uartlite-2.0", /* compatible */
		0x40610000, /* reg */
		0x2580, /* xlnx,baudrate */
		0x0, /* xlnx,use-parity */
		0x0, /* xlnx,odd-parity */
		0x8, /* xlnx,data-bits */
		0x3, /* interrupts */
		0x41200001 /* interrupt-parent */
	},
	{
		"xlnx,axi-uartlite-2.0", /* compatible */
		0x40620000, /* reg */
		0x2580, /* xlnx,baudrate */
		0x0, /* xlnx,use-parity */
		0x0, /* xlnx,odd-parity */
		0x8, /* xlnx,data-bits */
		0x4, /* interrupts */
		0x41200001 /* interrupt-parent */
	},
	 {
		 NULL
	}
};