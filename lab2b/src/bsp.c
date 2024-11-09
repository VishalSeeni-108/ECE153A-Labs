/*****************************************************************************
* bsp.c for Lab2A of ECE 153a at UCSB
* Date of the Last Update:  October 27,2019
*****************************************************************************/

/**/
#include "qpn_port.h"
#include "bsp.h"
#include "lab2a.h"
#include "xintc.h"
#include "xil_exception.h"
#include "xparameters.h"
#include "xgpio.h"
#include "xtmrctr.h"
#include "xintc.h"
#include "xtmrctr_l.h"
#include "xintc_l.h"
#include "mb_interface.h"
#include <xbasic_types.h>
#include <xio.h>
#include <stdbool.h>
#include "xspi.h"
#include "xspi_l.h"
#include "lcd.h"

/*****************************/

/* Define all variables and Gpio objects here  */
XGpio led; //Display LEDs
XGpio rgbled; //Status LED
XIntc sys_intc; //Interrupt controller
XGpio EncGpio; //Encoder
XTmrCtr sys_tmrctr; //Timer 1 for debouncing
XTmrCtr idle_tmr; //Timer 2 for screen idle
XGpio BtnGpio; //Buttons

int display_num;
int curr_state; //CBA, use binary
bool display_on;
int return_value;
int last_trigger;

//Screen stuff
static XGpio dc;
static XSpi spi;

XSpi_Config *spiConfig;	/* Pointer to Configuration data */

u32 status;
u32 controlReg;



#define GPIO_CHANNEL1 1

void debounceInterrupt(); // Write This function

// Create ONE interrupt controllers XIntc
// Create two static XGpio variables
// Suggest Creating two int's to use for determining the direction of twist

/*..........................................................................*/
void BSP_init(void) {
/* Setup LED's, etc */
		XGpio_Initialize(&led, XPAR_AXI_GPIO_LED_DEVICE_ID); //Display LED
		XGpio_Initialize(&rgbled, XPAR_RGBLED_DEVICE_ID); //Status LED

//Timer initialization
		XTmrCtr_Initialize(&sys_tmrctr, XPAR_AXI_TIMER_0_DEVICE_ID);
		XTmrCtr_SetOptions(&sys_tmrctr, 0, XTC_INT_MODE_OPTION | XTC_AUTO_RELOAD_OPTION);
		XTmrCtr_SetResetValue(&sys_tmrctr, 0, 0);
		XTmrCtr_Start(&sys_tmrctr, 0);

		XTmrCtr_Initialize(&idle_tmr, XPAR_AXI_TIMER_1_DEVICE_ID);
		XTmrCtr_SetOptions(&idle_tmr, 0, XTC_INT_MODE_OPTION | XTC_AUTO_RELOAD_OPTION);
		XTmrCtr_SetResetValue(&idle_tmr, 0, 0xFFFFFFFF - 200000000);
		XTmrCtr_Start(&idle_tmr, 0);

/* Setup interrupts and reference to interrupt handler function(s)  */

	/*
	 * Initialize the interrupt controller driver so that it's ready to use.
	 * specify the device ID that was generated in xparameters.h
	*/
		XIntc_Initialize(&sys_intc, XPAR_MICROBLAZE_0_AXI_INTC_DEVICE_ID); //Interrupt controller
	/*
	 * Initialize GPIO and connect the interrupt controller to the GPIO.
	 *
	 */
		//Button initialization
			XGpio_Initialize(&BtnGpio, XPAR_AXI_GPIO_BTN_DEVICE_ID);
			XGpio_InterruptEnable(&BtnGpio, 1);
			XGpio_InterruptGlobalEnable(&BtnGpio);

		//Button interrupt setup
			XIntc_Connect(&sys_intc, XPAR_MICROBLAZE_0_AXI_INTC_AXI_GPIO_BTN_IP2INTC_IRPT_INTR,
						(XInterruptHandler) button_handler, &BtnGpio);
			XIntc_Enable(&sys_intc, XPAR_MICROBLAZE_0_AXI_INTC_AXI_GPIO_BTN_IP2INTC_IRPT_INTR);

		//Idle timer interrupt setup
			XIntc_Connect(&sys_intc, XPAR_MICROBLAZE_0_AXI_INTC_AXI_TIMER_1_INTERRUPT_INTR,
						(XInterruptHandler) timer_handler, &idle_tmr);
			XIntc_Enable(&sys_intc, XPAR_MICROBLAZE_0_AXI_INTC_AXI_TIMER_1_INTERRUPT_INTR);


		//LED Screen Setup
		/*
			 * Initialize the GPIO driver so that it's ready to use,
			 * specify the device ID that is generated in xparameters.h
			 */
			status = XGpio_Initialize(&dc, XPAR_SPI_DC_DEVICE_ID);
			if (status != XST_SUCCESS)  {
				xil_printf("Initialize GPIO dc fail!\n");
				return XST_FAILURE;
			}

			/*
			 * Set the direction for all signals to be outputs
			 */
			XGpio_SetDataDirection(&dc, 1, 0x0);



			/*
			 * Initialize the SPI driver so that it is  ready to use.
			 */
			spiConfig = XSpi_LookupConfig(XPAR_SPI_DEVICE_ID);
			if (spiConfig == NULL) {
				xil_printf("Can't find spi device!\n");
				return XST_DEVICE_NOT_FOUND;
			}

			status = XSpi_CfgInitialize(&spi, spiConfig, spiConfig->BaseAddress);
			if (status != XST_SUCCESS) {
				xil_printf("Initialize spi fail!\n");
				return XST_FAILURE;
			}

			/*
			 * Reset the SPI device to leave it in a known good state.
			 */
			XSpi_Reset(&spi);

			/*
			 * Setup the control register to enable master mode
			 */
			controlReg = XSpi_GetControlReg(&spi);
			XSpi_SetControlReg(&spi,
					(controlReg | XSP_CR_ENABLE_MASK | XSP_CR_MASTER_MODE_MASK) &
					(~XSP_CR_TRANS_INHIBIT_MASK));

			// Select 1st slave device
			XSpi_SetSlaveSelectReg(&spi, ~0x01);

			initLCD();

			drawBackground();


	//Encoder initialization
		XGpio_Initialize(&EncGpio, XPAR_ENCODER_DEVICE_ID);
		XGpio_InterruptEnable(&EncGpio, 1);
		XGpio_InterruptGlobalEnable(&EncGpio);
	//Encoder interrupt setup
		XIntc_Connect(&sys_intc, XPAR_MICROBLAZE_0_AXI_INTC_ENCODER_IP2INTC_IRPT_INTR,
				(XInterruptHandler) GpioHandler, &EncGpio); //TODO - figure out if this is the correct handler call
		XIntc_Enable(&sys_intc, XPAR_MICROBLAZE_0_AXI_INTC_ENCODER_IP2INTC_IRPT_INTR);

	// Press Knob

	// Twist Knob
		
}
/*..........................................................................*/
void QF_onStartup(void) {                 /* entered with interrupts locked */

/* Enable interrupts */
	xil_printf("\n\rQF_onStartup\n"); // Comment out once you are in your complete program
	XIntc_Start(&sys_intc, XIN_REAL_MODE);

	// Press Knob
	// Enable interrupt controller
	// Start interupt controller
	// register handler with Microblaze
	// Global enable of interrupt
	// Enable interrupt on the GPIO

	// Twist Knob

	// General
	// Initialize Exceptions
	// Press Knob
	// Register Exception
	// Twist Knob
	// Register Exception
	// General
	// Enable Exception

	// Variables for reading Microblaze registers to debug your interrupts.
//	{
//		u32 axi_ISR =  Xil_In32(intcPress.BaseAddress + XIN_ISR_OFFSET);
//		u32 axi_IPR =  Xil_In32(intcPress.BaseAddress + XIN_IPR_OFFSET);
//		u32 axi_IER =  Xil_In32(intcPress.BaseAddress + XIN_IER_OFFSET);
//		u32 axi_IAR =  Xil_In32(intcPress.BaseAddress + XIN_IAR_OFFSET);
//		u32 axi_SIE =  Xil_In32(intcPress.BaseAddress + XIN_SIE_OFFSET);
//		u32 axi_CIE =  Xil_In32(intcPress.BaseAddress + XIN_CIE_OFFSET);
//		u32 axi_IVR =  Xil_In32(intcPress.BaseAddress + XIN_IVR_OFFSET);
//		u32 axi_MER =  Xil_In32(intcPress.BaseAddress + XIN_MER_OFFSET);
//		u32 axi_IMR =  Xil_In32(intcPress.BaseAddress + XIN_IMR_OFFSET);
//		u32 axi_ILR =  Xil_In32(intcPress.BaseAddress + XIN_ILR_OFFSET) ;
//		u32 axi_IVAR = Xil_In32(intcPress.BaseAddress + XIN_IVAR_OFFSET);
//		u32 gpioTestIER  = Xil_In32(sw_Gpio.BaseAddress + XGPIO_IER_OFFSET);
//		u32 gpioTestISR  = Xil_In32(sw_Gpio.BaseAddress  + XGPIO_ISR_OFFSET ) & 0x00000003; // & 0xMASK
//		u32 gpioTestGIER = Xil_In32(sw_Gpio.BaseAddress  + XGPIO_GIE_OFFSET ) & 0x80000000; // & 0xMASK
//	}
}


void QF_onIdle(void) {        /* entered with interrupts locked */

    QF_INT_UNLOCK();                       /* unlock interrupts */

    {
    	// Write code to increment your interrupt counter here.
    	// QActive_postISR((QActive *)&AO_Lab2A, ENCODER_DOWN); is used to post an event to your FSM



// 			Useful for Debugging, and understanding your Microblaze registers.
//    		u32 axi_ISR =  Xil_In32(intcPress.BaseAddress + XIN_ISR_OFFSET);
//    	    u32 axi_IPR =  Xil_In32(intcPress.BaseAddress + XIN_IPR_OFFSET);
//    	    u32 axi_IER =  Xil_In32(intcPress.BaseAddress + XIN_IER_OFFSET);
//
//    	    u32 axi_IAR =  Xil_In32(intcPress.BaseAddress + XIN_IAR_OFFSET);
//    	    u32 axi_SIE =  Xil_In32(intcPress.BaseAddress + XIN_SIE_OFFSET);
//    	    u32 axi_CIE =  Xil_In32(intcPress.BaseAddress + XIN_CIE_OFFSET);
//    	    u32 axi_IVR =  Xil_In32(intcPress.BaseAddress + XIN_IVR_OFFSET);
//    	    u32 axi_MER =  Xil_In32(intcPress.BaseAddress + XIN_MER_OFFSET);
//    	    u32 axi_IMR =  Xil_In32(intcPress.BaseAddress + XIN_IMR_OFFSET);
//    	    u32 axi_ILR =  Xil_In32(intcPress.BaseAddress + XIN_ILR_OFFSET) ;
//    	    u32 axi_IVAR = Xil_In32(intcPress.BaseAddress + XIN_IVAR_OFFSET);
//
//    	    // Expect to see 0x00000001
//    	    u32 gpioTestIER  = Xil_In32(sw_Gpio.BaseAddress + XGPIO_IER_OFFSET);
//    	    // Expect to see 0x00000001
//    	    u32 gpioTestISR  = Xil_In32(sw_Gpio.BaseAddress  + XGPIO_ISR_OFFSET ) & 0x00000003;
//
//    	    // Expect to see 0x80000000 in GIER
//    		u32 gpioTestGIER = Xil_In32(sw_Gpio.BaseAddress  + XGPIO_GIE_OFFSET ) & 0x80000000;


    }
}

/* Q_onAssert is called only when the program encounters an error*/
/*..........................................................................*/
void Q_onAssert(char const Q_ROM * const Q_ROM_VAR file, int line) {
    (void)file;                                   /* name of the file that throws the error */
    (void)line;                                   /* line of the code that throws the error */
    QF_INT_LOCK();
    printDebugLog();
    for (;;) {
    }
}

/* Interrupt handler functions here.  Do not forget to include them in lab2a.h!
To post an event from an ISR, use this template:
QActive_postISR((QActive *)&AO_Lab2A, SIGNALHERE);
Where the Signals are defined in lab2a.h  */

/******************************************************************************
*
* This is the interrupt handler routine for the GPIO for this example.
*
******************************************************************************/
void GpioHandler(void *CallbackRef) {
	// Increment A counter
	XGpio *GpioPtr = (XGpio *)CallbackRef;

	XTmrCtr_Reset(&idle_tmr, 0);

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
				QActive_postISR((QActive *)&AO_Lab2A, ENCODER_CLICK);

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
				QActive_postISR((QActive *)&AO_Lab2A, ENCODER_UP);
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
				QActive_postISR((QActive *)&AO_Lab2A, ENCODER_DOWN);
			}
			else if(enc == 0)
			{
				curr_state = 5;
			}
			break;
		}


		XGpio_InterruptClear(GpioPtr, 1);
}

void TwistHandler(void *CallbackRef) {
	//XGpio_DiscreteRead( &twist_Gpio, 1);

}

void debounceTwistInterrupt(){
	// Read both lines here? What is twist[0] and twist[1]?
	// How can you use reading from the two GPIO twist input pins to figure out which way the twist is going?
}

void debounceInterrupt() {
	QActive_postISR((QActive *)&AO_Lab2A, ENCODER_CLICK);
	// XGpio_InterruptClear(&sw_Gpio, GPIO_CHANNEL1); // (Example, need to fill in your own parameters
}

void button_handler(void *CallbackRef){
	XGpio *GpioPtr = (XGpio *)CallbackRef;
	unsigned int btn = XGpio_DiscreteRead(&BtnGpio, 1);

	XTmrCtr_Reset(&idle_tmr, 0);

	switch(btn)
	{
	case 1:
		QActive_postISR((QActive *)&AO_Lab2A, BUTTON_1);
		break;
	case 2:
		QActive_postISR((QActive *)&AO_Lab2A, BUTTON_2);
		break;
	case 16:
		QActive_postISR((QActive *)&AO_Lab2A, BUTTON_3);
		break;
	case 4:
		QActive_postISR((QActive *)&AO_Lab2A, BUTTON_4);
		break;
	case 8:
		QActive_postISR((QActive *)&AO_Lab2A, BUTTON_5);
		break;
	}

	XGpio_InterruptClear(GpioPtr, 1);
}

void timer_handler(){
	Xuint32 ControlStatusReg;
	ControlStatusReg =
				XTimerCtr_ReadReg(idle_tmr.BaseAddress, 0, XTC_TCSR_OFFSET);
	QActive_postISR((QActive *)&AO_Lab2A, TIMER_RESET);

	XTmrCtr_WriteReg(idle_tmr.BaseAddress, 0, XTC_TCSR_OFFSET,
				ControlStatusReg |XTC_CSR_INT_OCCURED_MASK);
}
