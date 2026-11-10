/*----------------------------------------------------------------\
@ Embedded Controller by Young-Keun Kim - Handong Global University
Author           : Jisu Park
Created          : 02-11-2026
Modified         : 02-11-2026
Language/ver     : C in VS Code

Description      : LAB - RC servo motor
/----------------------------------------------------------------*/

#include "ecSTM32F411v2.h"

#define TIM2_CH2    PA_1


volatile int angle = 0;                     // present angle [deg], 0 ~ 180
volatile int dir   = 1;                     // 1 : angle increases,  -1 : angle decreases


// Initialization 
void setup(void){

    RCC_PLL_init();                             // System Clock = 84MHz

    GPIO_init(PC_13, INPUT);
    GPIO_pupd(PC_13, PULL_UP);
    
    PWM_init(TIM2_CH2, 20);
    PWM_pulsewidth_us(TIM2_CH2, 500);

    TIM_UI_init(TIM3, 500);
    
    EXTI_init(BUTTON_PIN, FALL, 0);             // push-button PC13 : falling edge trigger

}


int main(void) {
    setup();
    while (1) {}
}


void TIM3_IRQHandler(void){
	if (is_UIF(TIM3)){
 
		angle += dir * 10;          
		if (angle >= 180){  
            dir = -1; }    // reached 180 : go down
		else if (angle <= 0)  { 
            dir =  1; }    // reached 0   : go up

		PWM_pulsewidth_us(TIM2_CH2, 500 + angle * 2000 / 180);
        clear_UIF(TIM3);                    // clear the flag
	}
}
 
// EXTI Interrupt : push-button PC13
void EXTI15_10_IRQHandler(void){
	if (is_pending_EXTI(BUTTON_PIN)){
		
		angle = 0;                          // back to 0 degree, go up again
		dir   = 1;
		PWM_pulsewidth_us(TIM2_CH2, 500);
        clear_pending_EXTI(BUTTON_PIN);     // clear the flag
	}
}