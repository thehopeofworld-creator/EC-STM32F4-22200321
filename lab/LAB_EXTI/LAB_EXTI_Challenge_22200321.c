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
PinName_t selectFND[2] = {PA_10, PA_11};


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
    for (int i = 0; i < 2; i++) {
        GPIO_init(selectFND[i], OUTPUT);  // calls RCC_GPIOA_enable()
        GPIO_otype(selectFND[i], PUSH_PULL);		// Push-pull
        GPIO_write(selectFND[i], HIGH);	// Initially selectFND is HIGH
    }
}


// MAIN  ----------------------------------------
int main(void) {
	setup();
    FND_display(count);

		while (1){
        // 250 cycles x (2 ms + 2 ms) = about 1 second, display keeps refreshing
        for (int i = 0; i<250; i++){
            uint8_t tens = count / 10;   // ten's place
            uint8_t ones = count % 10;   // one's place

            // light the tens digit (FND_1) for 2 ms
            GPIO_write(selectFND[0], LOW);
            GPIO_write(selectFND[1], HIGH);
            FND_display(tens);
            delay_ms(2);

            // light the ones digit (FND_0) for 2 ms
            GPIO_write(selectFND[0], HIGH);
            GPIO_write(selectFND[1], LOW);
            FND_display(ones);
            delay_ms(2);
        }
        
        // 1 second passed: count up only when not paused (SW2)
        if(!count2){
            count++;
            if (count == 60 ){           // after 59, back to 0
                count = 0;
            }
        }
    }
}

// SW1 (PD_2, EXTI line 2): reset the counter to 0
void EXTI2_IRQHandler(void) {        
	if (is_pending_EXTI(SW1_PIN)) {
		clear_pending_EXTI(SW1_PIN);
        count = 0;
	}
}

// SW2 (PA_4, EXTI line 4): toggle pause / resume
void EXTI4_IRQHandler(void) {
	if (is_pending_EXTI(SW2_PIN)) {   
			clear_pending_EXTI(SW2_PIN);
            count2 = !count2;
	}
}
