/*----------------------------------------------------------------\
@ Embedded Controller by Young-Keun Kim - Handong Global University
Author           : Jisu Park
Created          : 05-03-2021
Modified         : 06-10-2026
Language/ver     : C++ in VS Code

Description      : LAB - EXTI
/----------------------------------------------------------------*/


#include "ecSTM32F411v2.h"

#define SW1_PIN    PD_2         //EVAL board JKIT SW1
#define SW2_PIN    PA_4			//EVAL board JKIT SW2
PinName_t pinFND[8]    = {PB_0, PB_1, PB_2, PB_3, PB_4, PB_5, PB_6, PB_7};
PinName_t selectFND = PA_10;


// void led_toggle(PinName_t pinName);  // in ecGPIO2.h
volatile uint8_t count = 0;
volatile uint8_t count2 = 0;

// Initialiization 
void setup(void)
{
    RCC_PLL_init();                 // System Clock = 84MHz
    SysTick_init();                 // SysTick Timer Initialization
    FND_init(pinFND);                 // FND Initialization
    for (int i = 0; i < 8; i++) {
        GPIO_pupd(pinFND[i], NO_PUPD);
    }

    // Initialize GPIOD_2 for Input Button
    GPIO_init(SW1_PIN, INPUT);  // INPUT for EVAL board
    GPIO_pupd(SW1_PIN, PULL_UP);  // PULL-UP for EVAL board

    // Initialize GPIOA_4 for Input Button
    GPIO_init(SW2_PIN, INPUT);  // INPUT for EVAL board
    GPIO_pupd(SW2_PIN, PULL_UP);  // PULL-UP for EVAL board

    // Initialize EXTI 
    EXTI_init(SW1_PIN, FALL, 9);   
    EXTI_init(SW2_PIN, FALL, 10);

    // Initialize GPIOA_10 for Output FND Select
    GPIO_init(selectFND, OUTPUT);   // calls RCC_GPIOA_enable()
    GPIO_otype(selectFND, PUSH_PULL);   // Push-pull
    GPIO_write(selectFND, HIGH);    // Initially selectFND is HIGH
}


// MAIN  ----------------------------------------
int main(void) {
	setup();
    FND_display(count);

	while (1){
        delay_ms(1000);
        if(!count2){
            count++;
            if (count == 10 ){
                count = 0;
            }
            FND_display(count);
        }
    }
}

void EXTI2_IRQHandler(void) {        // SW1 (PD_2): reset
	if (is_pending_EXTI(SW1_PIN)) {
		clear_pending_EXTI(SW1_PIN);
        count = 0;
        FND_display(count);
	}
}

void EXTI4_IRQHandler(void) {       // SW2 (PA_4): pause / resume
	if (is_pending_EXTI(SW2_PIN)) {   
			clear_pending_EXTI(SW2_PIN);
            count2 = !count2;
	}
}
