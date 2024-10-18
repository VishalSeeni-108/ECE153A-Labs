#include "extra.h"
#include <stdlib.h>
#include "xparameters.h"
#include "xgpio.h"
#include "sevenSeg_new.h"

XIntc sys_intc;
XTmrCtr sys_tmrctr;
Xuint32 data;

unsigned int count = 1;
XGpio BtnGpio;
//XGpio led;

int value = 0;
int work_value;
int inc_value = 0;

void button_handler(void *CallbackRef){
	XGpio *GpioPtr = (XGpio *)CallbackRef;
	unsigned int btn = XGpio_DiscreteRead(&BtnGpio, 1);

	if(btn == 1)
	{
		value = -(inc_value); //This way when inc is zero -i.e. count is stopped - value resets directly to
		//zero, and when inc is one -i.e. count is running - it is set to -1 and increments up to 0
	}
	else if(btn == 2)
	{
		inc_value = -1;
	}
	else if(btn == 4)
	{
		inc_value = 1;
	}
	else if(btn == 8)
	{
		inc_value = 0;
	}

	XGpio_InterruptClear(GpioPtr, 1);
}


void timer_handler() {
	// This is the interrupt handler function
	// Do not print inside of this function. 
	Xuint32 ControlStatusReg;
	/*
	 * Read the new Control/Status Register content.
	 */
	ControlStatusReg =
			XTimerCtr_ReadReg(sys_tmrctr.BaseAddress, 0, XTC_TCSR_OFFSET);

	//xil_printf("Timer interrupt occurred. Count= %d\r\n", count);
	//XGpio_DiscreteWrite(&led,1,count);

	//xil_printf("Current value from buttons: %d\n", BtnGpio);

	//Write to display
	int seg_num = count % 8;
	if(seg_num == 0) //Increment value and reset working value
	{
		//Check if limits reached
		if(!((value == 0 && inc_value == -1) || (value == 999999999 && inc_value == 1)))
		{
			value += inc_value;
		}
		work_value = value;
	}
	int digit_val = work_value % 10;
	work_value /= 10;
	sevenseg_draw_digit (seg_num,digit_val);



	count++;			// increment count
	/*
	 * Acknowledge the interrupt by clearing the interrupt
	 * bit in the timer control status register
	 */
	XTmrCtr_WriteReg(sys_tmrctr.BaseAddress, 0, XTC_TCSR_OFFSET,
			ControlStatusReg |XTC_CSR_INT_OCCURED_MASK);

}

void extra_disable() {
	XIntc_Disable(&sys_intc,
			XPAR_MICROBLAZE_0_AXI_INTC_AXI_TIMER_0_INTERRUPT_INTR);
}

void extra_enable() {
	XIntc_Enable(&sys_intc,
			XPAR_MICROBLAZE_0_AXI_INTC_AXI_TIMER_0_INTERRUPT_INTR);
}

int extra_method() {

	//XGpio_Initialize(&led, XPAR_AXI_GPIO_LED_DEVICE_ID);

	//xil_printf("I'm in the main() method\r\n");

	XStatus Status;
	/*
	 * Initialize the interrupt controller driver so that
	 * it is ready to use, specify the device ID that is generated in
	 * xparameters.h
	 */
	Status = XST_SUCCESS;
	Status = XIntc_Initialize(&sys_intc, XPAR_MICROBLAZE_0_AXI_INTC_DEVICE_ID);

	if (Status != XST_SUCCESS) {
		if (Status == XST_DEVICE_NOT_FOUND) {
			xil_printf("XST_DEVICE_NOT_FOUND...\r\n");
		} else {
			xil_printf("a different error from XST_DEVICE_NOT_FOUND...\r\n");
		}
		xil_printf("Interrupt controller driver failed to be initialized...\r\n");
		return XST_FAILURE;
	}
	xil_printf("Interrupt controller driver initialized!\r\n");

	//Button initialization

		XGpio_Initialize(&BtnGpio, XPAR_AXI_GPIO_BTN_DEVICE_ID);
		XGpio_InterruptEnable(&BtnGpio, 1);
		XGpio_InterruptGlobalEnable(&BtnGpio);

		//Button interrupt setup
		XIntc_Connect(&sys_intc, XPAR_MICROBLAZE_0_AXI_INTC_AXI_GPIO_BTN_IP2INTC_IRPT_INTR,
					(XInterruptHandler) button_handler, &BtnGpio);
		XIntc_Enable(&sys_intc, XPAR_MICROBLAZE_0_AXI_INTC_AXI_GPIO_BTN_IP2INTC_IRPT_INTR);





	/*
	 * Connect the application handler that will be called when an interrupt
	 * for the timer occurs
	 */
	Status = XIntc_Connect(&sys_intc, XPAR_MICROBLAZE_0_AXI_INTC_AXI_TIMER_0_INTERRUPT_INTR,
			(XInterruptHandler) timer_handler, &sys_tmrctr);
	if (Status != XST_SUCCESS) {
		xil_printf("Failed to connect the application handlers to the interrupt controller...\r\n");
		return XST_FAILURE;
	}
	xil_printf("Connected to Interrupt Controller!\r\n");

	/*
	 * Start the interrupt controller such that interrupts are enabled for
	 * all devices that cause interrupts.
	 */
	Status = XIntc_Start(&sys_intc, XIN_REAL_MODE);
	if (Status != XST_SUCCESS) {
		xil_printf("Interrupt controller driver failed to start...\r\n");
		return XST_FAILURE;
	}
	xil_printf("Started Interrupt Controller!\r\n");
	/*
	 * Enable the interrupt for the timer counter
	 */
	XIntc_Enable(&sys_intc, XPAR_MICROBLAZE_0_AXI_INTC_AXI_TIMER_0_INTERRUPT_INTR);
	/*
	 * Initialize the timer counter so that it's ready to use,
	 * specify the device ID that is generated in xparameters.h
	 */
	Status = XTmrCtr_Initialize(&sys_tmrctr, XPAR_AXI_TIMER_0_DEVICE_ID);
	if (Status != XST_SUCCESS) {
		xil_printf("Timer initialization failed...\r\n");
		return XST_FAILURE;
	}
	xil_printf("Initialized Timer!\r\n");
	/*
	 * Enable the interrupt of the timer counter so interrupts will occur
	 * and use auto reload mode such that the timer counter will reload
	 * itself automatically and continue repeatedly, without this option
	 * it would expire once only
	 */
	XTmrCtr_SetOptions(&sys_tmrctr, 0, XTC_INT_MODE_OPTION | XTC_AUTO_RELOAD_OPTION);
	/*
	 * Set a reset value for the timer counter such that it will expire
	 * eariler than letting it roll over from 0, the reset value is loaded
	 * into the timer counter when it is started
	 */
	XTmrCtr_SetResetValue(&sys_tmrctr, 0, 0xFFFFFFFF - 208333);// 1000 clk cycles @ 100MHz = 10us
	/*
	 * Start the timer counter such that it's incrementing by default,
	 * then wait for it to timeout a number of times
	 */
	XTmrCtr_Start(&sys_tmrctr, 0);
	/*
	 * Register the intc device driver’s handler with the Standalone
	 * software platform’s interrupt table
	 */
	microblaze_register_handler(
			(XInterruptHandler) XIntc_DeviceInterruptHandler,
			(void*) XPAR_MICROBLAZE_0_AXI_INTC_DEVICE_ID);
//	microblaze_register_handler((XInterruptHandler)XIntc_DeviceInterruptHandler,
//			(void*)PUSHBUTTON_DEVICE_ID);
	//xil_printf("Registers handled!\r\n");

	/*
	 * Enable interrupts on MicroBlaze
	 */
	microblaze_enable_interrupts();
	xil_printf("Interrupts enabled!\r\n");
	/*
	 * At this point, the system is ready to respond to interrupts from the timer
	 */

	return XST_SUCCESS;
}
