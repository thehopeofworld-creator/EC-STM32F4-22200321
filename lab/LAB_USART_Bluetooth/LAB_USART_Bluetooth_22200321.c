/*----------------------------------------------------------------\
@ Embedded Controller by Young-Keun Kim - Handong Global University
Author           : Jisu Park
Created          : 17-10-2026
Modified         : 19-10-2026
Language/ver     : C in VS Code
/----------------------------------------------------------------*/

#include <stdio.h>
#include "ecSTM32F411v2.h"          // this include ecUART2.h and ecStepper2.h

// ( A, B,  AN,  BN)
PinName_t pinStepper[4] = {PB_10, PB_4, PB_5, PB_3};

static volatile uint8_t BT_data  = 0;           // character received from the Bluetooth module
static volatile uint8_t cmd      = 0;           // motor command waiting for main(): 'J' or 'K' (0 = none)
static volatile uint8_t bStop    = 0;           // 1: emergency stop requested
static volatile uint8_t bStopMsg = 0;           // 1: main() prints the stop message

// Initialization
void setup(void){
	RCC_PLL_init();                             // System Clock = 84MHz
	SysTick_init();                             // SysTick for delay_ms()

	GPIO_init(LED_PIN, OUTPUT);                 // LED (LD2, PA_5)

	USART2_init();                              // USART2 : PC (USB), messages
	USART1_init();                              // USART1 : Bluetooth module, PA_9 = TX, PA_10 = RX
	USART1_baud(BAUD_9600);                     // HC-06 default baud rate

	stepper_init(FULL, pinStepper);             // full-stepping, A, B, A', B'
	stepper_speed(8);                           // motor speed [rpm]
}


int main(void){
	setup();
	printf("MCU Initialized\r\n");

	uint32_t rev = 2;                                   // number of revolutions
	uint32_t steps = STEP_PER_REV * rev;                // 4096 steps = 2 revolutions in full-stepping

	// Infinite Loop ----------------------------------------------------------
	while(1){
		// A motor command from the Bluetooth (set by the interrupt handler)
		if (cmd != 0){
			uint8_t c = cmd;
			cmd = 0;                                            // the command is taken
			uint32_t dir = (c == 'J') ? DIR_CW : DIR_CCW;       // J : CW, K : CCW
			printf("RX: %c -> %s %d rev\r\n", c, (c == 'J') ? "CW" : "CCW", (int)rev);

			// Rotate step by step, so that the stop request is checked at every step
			for (uint32_t i = 0; i < steps && bStop == 0; i++){
				stepper_step(1, dir);
			}
			stepper_stop();                                     // release all coils
			if (bStop == 0) printf("Done\r\n");                 // finished without a stop request
		}

		// Emergency stop message (the handler only sets the flag, because printf() is slow)
		if (bStopMsg == 1){
			printf("RX: P -> STOP\r\n");
			bStopMsg = 0;
		}
	}
}


// USART1 RX Interrupt : a key from the Bluetooth module
void USART1_IRQHandler(void){
	if (is_USART1_RXNE()){
		BT_data = USART1_read();                                // RX from USART1 (Bluetooth)

		if (BT_data == 'H'){
            GPIO_write(LED_PIN, HIGH);     // LED ON
            printf("RX: H -> LED ON\r\n");
        }

		else if (BT_data == 'L') {
            GPIO_write(LED_PIN, LOW);      // LED OFF
            printf("RX: L -> LED OFF\r\n");
        }

		else if (BT_data == 'J' || BT_data == 'K'){
			bStop = 0;                                          // new motion: clear the stop request
			cmd = BT_data;                                      // main() rotates the motor
		}
		else if (BT_data == 'P'){
			bStop = 1;                                          // emergency stop
			cmd = 0;                                            // cancel a waiting command
			stepper_stop();                                     // all coils off at once
			bStopMsg = 1;                                       // main() prints the message
		}
	}
}


// USART2 RX Interrupt : a key typed in the USB terminal (not used, read it to clear the flag)
void USART2_IRQHandler(void){
	if (is_USART2_RXNE()){
		USART2_read();
	}
}