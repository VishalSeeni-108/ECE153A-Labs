/******************************************************************************
*
* Copyright (C) 2009 - 2014 Xilinx, Inc.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* Use of the Software is limited solely to applications:
* (a) running on a Xilinx device, or
* (b) that interact with a Xilinx device through a bus or interconnect.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* XILINX  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
* OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
* Except as contained in this notice, the name of the Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in
* this Software without prior written authorization from Xilinx.
*
******************************************************************************/

/*
 * helloworld.c: simple test application
 *
 * This application configures UART 16550 to baud rate 9600.
 * PS7 UART (Zynq) is not initialized by this application, since
 * bootrom/bsp configures it to baud rate 115200
 *
 * ------------------------------------------------
 * | UART TYPE   BAUD RATE                        |
 * ------------------------------------------------
 *   uartns550   9600
 *   uartlite    Configurable only in HW design
 *   ps7_uart    115200 (configured by bootrom/bsp)
 */

#include <stdio.h>
#include <stdbool.h>
#include "platform.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xgpio.h"
#include "xtmrctr.h"
#include "xintc.h"
#include "xtmrctr_l.h"
#include "xintc_l.h"
#include "mb_interface.h"
#include <xbasic_types.h>
#include <xio.h>


XIntc sys_intc; //Interrupt controller
XGpio led; //Display LEDs
XGpio rgbled; //Status LED
XGpio BtnGpio;
XGpio EncGpio; //Encoder
XTmrCtr sys_tmrctr; //Timer
int display_num;
int curr_state; //CBA, use binary
bool display_on;
int return_value;
int last_trigger;

void led_left()
{
	if(display_num < 32768)
	{
		display_num *= 2;
	}
	else
	{
		display_num = 1;
	}
}

void led_right()
{
	if(display_num > 1)
	{
		display_num /= 2;
	}
	else
	{
		display_num = 32768;
	}
}

void encoder_handler(void *CallbackRef){
	XGpio *GpioPtr = (XGpio *)CallbackRef;
	unsigned int enc = XGpio_DiscreteRead(&EncGpio, 1);
	int curr_time = XTmrCtr_GetValue(&sys_tmrctr, 0);

	if(enc > 3)
	{
		if((curr_time - last_trigger) > 1000000)
		{
			last_trigger = curr_time;
			if(display_on)
			{
				return_value = display_num;
			}
			else
			{
				display_num = return_value;
			}
			display_on = !display_on;
		}
		enc -= 4;
	}

	switch(curr_state)
	{
	case 0: //Idle
		if(enc == 1)
		{
			curr_state = 1;
		}
		else if(enc == 2)
		{
			curr_state = 4;
		}
		break;
	case 1:
		if(enc == 0)
		{
			curr_state = 2;
		}
		else if(enc == 3)
		{
			curr_state = 0;
		}
		break;
	case 2:
		if(enc == 2)
		{
			curr_state = 3;
		}
		else if(enc == 1)
		{
			curr_state = 1;
		}
		break;
	case 3:
		if(enc == 3)
		{
			curr_state = 0;
			led_right();
		}
		else if(enc == 0)
		{
			curr_state = 2;
		}
		break;
	case 4:
		if(enc == 0)
		{
			curr_state = 5;
		}
		else if(enc == 3)
		{
			curr_state = 0;
		}
		break;
	case 5:
		if(enc == 1)
		{
			curr_state = 6;
		}
		else if(enc == 2)
		{
			curr_state = 4;
		}
		break;
	case 6:
		if(enc == 3)
		{
			curr_state = 0;
			led_left();
		}
		else if(enc == 0)
		{
			curr_state = 5;
		}
		break;
	}

	XGpio_InterruptClear(GpioPtr, 1);
}

void button_handler(void *CallbackRef){
	XGpio *GpioPtr = (XGpio *)CallbackRef;
	unsigned int btn = XGpio_DiscreteRead(&BtnGpio, 1);

	if(btn == 2)
	{
		led_left();
	}
	else if(btn == 4)
	{
		led_right();
	}

	XGpio_InterruptClear(GpioPtr, 1);
}

int main()
{
    init_platform();

    print("Hello World\n\r");
    print("Successfully ran Hello World application");

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

    		//Timer initialization
    		XTmrCtr_Initialize(&sys_tmrctr, XPAR_AXI_TIMER_0_DEVICE_ID);
    		XTmrCtr_SetOptions(&sys_tmrctr, 0, XTC_INT_MODE_OPTION | XTC_AUTO_RELOAD_OPTION);
    		XTmrCtr_SetResetValue(&sys_tmrctr, 0, 0);
    		XTmrCtr_Start(&sys_tmrctr, 0);


    		//Button initialization
    		XGpio_Initialize(&BtnGpio, XPAR_AXI_GPIO_BTN_DEVICE_ID);
    		XGpio_InterruptEnable(&BtnGpio, 1);
    		XGpio_InterruptGlobalEnable(&BtnGpio);

    		//Button interrupt setup
    		XIntc_Connect(&sys_intc, XPAR_MICROBLAZE_0_AXI_INTC_AXI_GPIO_BTN_IP2INTC_IRPT_INTR,
    					(XInterruptHandler) button_handler, &BtnGpio);
    		XIntc_Enable(&sys_intc, XPAR_MICROBLAZE_0_AXI_INTC_AXI_GPIO_BTN_IP2INTC_IRPT_INTR);

    		//Encoder initialization
    		XGpio_Initialize(&EncGpio, XPAR_ENCODER_DEVICE_ID);
    		XGpio_InterruptEnable(&EncGpio, 1);
    		XGpio_InterruptGlobalEnable(&EncGpio);

    		//Encoder interrupt setup
    		XIntc_Connect(&sys_intc, XPAR_MICROBLAZE_0_AXI_INTC_ENCODER_IP2INTC_IRPT_INTR,
    				(XInterruptHandler) encoder_handler, &EncGpio);
    		XIntc_Enable(&sys_intc, XPAR_MICROBLAZE_0_AXI_INTC_ENCODER_IP2INTC_IRPT_INTR);
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

    display_num = 1;
    curr_state = 0;
    display_on = true;
    return_value = 1;
    last_trigger = 0;

    //Peripheral initialization
    XGpio_Initialize(&led, XPAR_AXI_GPIO_LED_DEVICE_ID);
    XGpio_Initialize(&rgbled, XPAR_RGBLED_DEVICE_ID);

    //Button (for testing)
//    XGpio_Initialize(&BtnGpio, XPAR_AXI_GPIO_BTN_DEVICE_ID);
//    XGpio_InterruptEnable(&BtnGpio, 1);
//    XGpio_InterruptGlobalEnable(&BtnGpio);
//    XIntc_Connect(&sys_intc, XPAR_MICROBLAZE_0_AXI_INTC_AXI_GPIO_BTN_IP2INTC_IRPT_INTR,
//    					(XInterruptHandler) button_handler, &BtnGpio);
//    XIntc_Enable(&sys_intc, XPAR_MICROBLAZE_0_AXI_INTC_AXI_GPIO_BTN_IP2INTC_IRPT_INTR);

    XIntc_Start(&sys_intc, XIN_REAL_MODE);



    while(true)
    {
    	if(display_on)
    	{
    	    XGpio_DiscreteWrite(&led, 1, display_num);
    	}
    	else
    	{
    		XGpio_DiscreteWrite(&led, 1, 0);
    	}
    	XGpio_DiscreteWrite(&rgbled, 1, 2);
    }
    cleanup_platform();
    return 0;
}
