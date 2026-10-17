/*----------------------------------------------------------------\
@ Embedded Controller by Young-Keun Kim - Handong Global University
Author           : Jisu Park
Created          : 05-03-2021
Modified         : 14-10-2026
Language/ver     : C++ in VS Code

Description      : LAB - Stepper Motor HALF MODE
/----------------------------------------------------------------*/
#include "stm32f411xe.h"
#include "ecGPIO2.h"
#include "ecRCC2.h"
#include "ecEXTI2.h"
#include "ecSysTick2.h"
#include "ecStepper2.h"

// ( A, B,  AN,  BN)
PinName_t pinStepper[4] = {PB_10,PB_4,PB_5,PB_3};


// Initialiization 
void setup(void){	
	RCC_PLL_init();                                 // System Clock = 84MHz
	SysTick_init();                                 // Systick init
	
	GPIO_init(BUTTON_PIN, INPUT);           		// GPIOC pin13 initialization
    EXTI_init(BUTTON_PIN, FALL,0);           		// External Interrupt Setting

	stepper_init(HALF, pinStepper); 				// Stepper GPIO pin initialization
	stepper_speed(5);                          		//  set stepper motor speed
}


int main(void) { 
	// Initialiization --------------------------------------------------------
	setup();
	uint32_t rev=1;
	uint32_t steps=STEP_PER_REV*rev*2;  // HALF mode: 4096 steps/rev
	
	// Inifinite Loop ----------------------------------------------------------
	while(1){
		stepper_step(steps, DIR_CW);  // (Step : 2048, Direction : 0 , Mode : FULL)
		delay_ms(2000);
		stepper_step(steps, DIR_CCW);  // (Step : 2048, Direction : 0, Mode : FULL)
		delay_ms(2000);

	}
}



void EXTI15_10_IRQHandler(void) {  
	if (is_pending_EXTI(BUTTON_PIN)) {
		stepper_stop();
		clear_pending_EXTI(BUTTON_PIN); // cleared by writing '1'
	}
}