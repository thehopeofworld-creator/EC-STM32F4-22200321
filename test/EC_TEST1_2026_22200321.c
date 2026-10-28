/*----------------------------------------------------------------\
@ Embedded Controller by Young-Keun Kim - Handong Global University
Author           : [ YOUR NAME GOES HERE !!!!! ]
Student ID       : [ YOUR ID GOES HERE !!!!! ]
Created          : 00-00-2026 [WRITE THE DATE!!!!]
Language/ver     : C in VS Code

Description      : EC Test 1 (2026) Programming Test
                   
                   Rename this file to  EC_Test1_yourID.c
/----------------------------------------------------------------*/


#include "stm32f411xe.h"

//==================================================================
//  EC Library Include
//==================================================================
// YOUR CODE GOES HERE
// YOUR CODE GOES HERE


//==================================================================
// User GPIO Pin definitions
//==================================================================
// YOUR CODE GOES HERE
// YOUR CODE GOES HERE

// Stepper ( A, B, AN, BN )
// YOUR CODE GOES HERE

// FND segments
// YOUR CODE GOES HERE

// FND digit select : FND_0 ~ FND_3
// YOUR CODE GOES HERE

// Global Variables
// use   volatile uint32_t   instead of uint32_t if used in Interrupt ISR
volatile uint32_t motor_mode   = 0;	
volatile uint32_t motor_dir    = 0;	
volatile uint32_t motor_speed  = 0;	
// YOUR CODE GOES HERE
// YOUR CODE GOES HERE
// YOUR CODE GOES HERE




//==================================================================
//  Setup 
//==================================================================
void setup(void){
	RCC_PLL_init();					// System Clock = 84MHz
	SysTick_init();					// SysTick, 1 msec
	
	// USART2, 9600 baud
	// YOUR CODE GOES HERE			
	// YOUR CODE GOES HERE			

	// GPIO Inputs
	// YOUR CODE GOES HERE
	// YOUR CODE GOES HERE

	// EXTI 
	// YOUR CODE GOES HERE
	// YOUR CODE GOES HERE
	
	// GPIO Outputs	
	// YOUR CODE GOES HERE
	// YOUR CODE GOES HERE
	// YOUR CODE GOES HERE

	// Other Setup
	// YOUR CODE GOES HERE
}


//==================================================================
//  Main
//==================================================================
int main(void){
	setup();

	// YOUR CODE GOES HERE

	while(1){

		// Check PAUSE/RESUME 
		// YOUR CODE GOES HERE
		// YOUR CODE GOES HERE

		// Related to controlling motor speed/dir etc
		// HINT:  use  stepper_speed(vel);	only when vel>0
		stepper_step(1, motor_dir);	 // Keep step=1 or a low value to stop blocking other code
		// YOUR CODE GOES HERE
		// YOUR CODE GOES HERE
		// YOUR CODE GOES HERE


		// Print to the serial monitor every 2 sec
		// PAUSE, blink at 2 sec
		//     e.g.  printf("Mode=%lu Speed=%lu DIR=%lu\r\n", ... );
		// HINT:  msTicks in ecSysTick2.h  is increasing at a 1 msec rate accurately.
		// You can use msTicks  to count every 2000 msec 
		// YOUR CODE GOES HERE
		// YOUR CODE GOES HERE

			
	delay_ms(1);  // use this delay
	}
}


//==================================================================
//  EXTI Interrupt Service Routines
//==================================================================

	// YOUR CODE GOES HERE
	// YOUR CODE GOES HERE
	// YOUR CODE GOES HERE
	// YOUR CODE GOES HERE


//==================================================================
//  UART2 Interrupt Service Routines
//==================================================================
void USART2_IRQHandler(void){
	if(is_USART2_RXNE()){
	
	// HINT: 	comparing a character:  
	//  		uint8_t ch = USART2_read();		
	//			if(ch == 'C' || ch == 'c')	 etc..
	//
	// YOUR CODE GOES HERE
	// YOUR CODE GOES HERE
	}
}
