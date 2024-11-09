/*****************************************************************************
* lab2a.c for Lab2A of ECE 153a at UCSB
* Date of the Last Update:  October 23,2014
*****************************************************************************/

#define AO_LAB2A

#include "qpn_port.h"
#include "bsp.h"
#include "lab2a.h"
#include "xspi.h"
#include "xspi_l.h"
#include "lcd.h"
#include <stdbool.h>

int volume = 63;
int mode = 1;
bool mute = false;

typedef struct Lab2ATag  {               //Lab2A State machine
	QActive super;
}  Lab2A;

/* Setup state machines */
/**********************************************************************/
static QState Lab2A_initial (Lab2A *me);
static QState Lab2A_on      (Lab2A *me);
static QState Lab2A_stateA  (Lab2A *me);
static QState Lab2A_stateB  (Lab2A *me);


/**********************************************************************/


Lab2A AO_Lab2A;


void Lab2A_ctor(void)  {
	Lab2A *me = &AO_Lab2A;
	QActive_ctor(&me->super, (QStateHandler)&Lab2A_initial);
}


QState Lab2A_initial(Lab2A *me) {
	xil_printf("\n\rInitialization");
    return Q_TRAN(&Lab2A_on);
}

QState Lab2A_on(Lab2A *me) {
	switch (Q_SIG(me)) {
		case Q_ENTRY_SIG: {
			xil_printf("\n\rOn");
			}
			
		case Q_INIT_SIG: {
			return Q_TRAN(&Lab2A_stateA);
			}
	}

	return Q_SUPER(&QHsm_top);
}


/* Create Lab2A_on state and do any initialization code if needed */
/******************************************************************/

QState Lab2A_stateA(Lab2A *me) {
	switch (Q_SIG(me)) {
		case Q_ENTRY_SIG: {
			xil_printf("Startup State A\n");

			setFont(BigFont);
			setColor(0, 255, 0);
			fillRect(57, 90, 57 + (volume * 2), 110);
			switch(mode)
			{
				case 1:
				{
					lcdPrint("Mode: 1", 65, 130);
					break;
				}
				case 2:
				{
					lcdPrint("Mode: 2", 65, 130);
					break;
				}
				case 3:
				{
					lcdPrint("Mode: 3", 65, 130);
					break;
				}
				case 4:
				{
					lcdPrint("Mode: 4", 65, 130);
					break;
				}
				case 5:
				{
					lcdPrint("Mode: 5", 65, 130);
					break;
				}
			}

			return Q_HANDLED();
		}
		
		case ENCODER_UP: {
			xil_printf("Encoder Up from State A\n");
			if(volume < 63)
			{
				volume++;
				clearVolume();
				setColor(0, 255, 0);
				fillRect(57, 90, 57 + (volume * 2), 110);
			}
			return Q_HANDLED();
		}

		case ENCODER_DOWN: {
			xil_printf("Encoder Down from State A\n");
			if(volume > 0)
			{
				volume--;
				clearVolume();
				setColor(0, 255, 0);
				fillRect(57, 90, 57 + (volume * 2), 110);
			}
			return Q_HANDLED();
		}

		case ENCODER_CLICK:  {
			if(mute)
			{
				clearVolume();
				setColor(0, 255, 0);
				fillRect(57, 90, 57 + (volume * 2), 110);
			}
			else
			{
				clearVolume();
				setColor(0, 255, 0);
				fillRect(57, 90, 57 + (0 * 2), 110);
			}

			mute = !mute;
			return Q_HANDLED();
		}
		case BUTTON_1: {
			lcdPrint("Mode: 1", 65, 130);
			mode = 1;
			return Q_HANDLED();
		}
		case BUTTON_2:{
			lcdPrint("Mode: 2", 65, 130);
			mode = 2;
			return Q_HANDLED();
		}
		case BUTTON_3:{
			lcdPrint("Mode: 3", 65, 130);
			mode = 3;
			return Q_HANDLED();
		}
		case BUTTON_4:{
			lcdPrint("Mode: 4", 65, 130);
			mode = 4;
			return Q_HANDLED();
		}
		case BUTTON_5:{
			lcdPrint("Mode: 5", 65, 130);
			mode = 5;
			return Q_HANDLED();
		}
		case TIMER_RESET:{
			return Q_TRAN(&Lab2A_stateB);
		}

	}
	return Q_SUPER(&Lab2A_on);

}

QState Lab2A_stateB(Lab2A *me) {
	switch (Q_SIG(me)) {
		case Q_ENTRY_SIG: {
			xil_printf("Startup State B\n");

			clearVolume();
			clearMode();

			return Q_HANDLED();
		}
		
		case ENCODER_UP:
		case ENCODER_DOWN:
		case ENCODER_CLICK:
		case BUTTON_1:
		case BUTTON_2:
		case BUTTON_3:
		case BUTTON_4:
		case BUTTON_5:
		{
			return Q_TRAN(&Lab2A_stateA);
		}

	}

	return Q_SUPER(&Lab2A_on);

}

