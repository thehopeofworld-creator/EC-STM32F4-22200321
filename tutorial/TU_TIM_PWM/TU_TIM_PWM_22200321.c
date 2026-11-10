/*----------------------------------------------------------------@ Embedded Controller by Young-Keun Kim - Handong Global University
Author           : Jisu Park
Created          : 10-13-2026
Modified         : 31-10-2026
Language/ver     : C in VS Code

Description      : Tutorial - PWM
                   PA_5 (TIM2_CH1, Nucleo LD2) : 100 Hz, duty sweep 0 -> 100%                   
/----------------------------------------------------------------*/

#include "ecSTM32F411v2.h"

// Button Pin & PWM Pin
#define PWM_LED PA_5

// Initialiization 
void setup(void) {  
    RCC_PLL_init();
    SysTick_init();

    // PWM of 10 msec:  TIM2_CH1 (PA_5) 
    PWM_init(PWM_LED, 10);          // 10 msec period -> 100 Hz 
    PWM_duty(PWM_LED, 0.2f);        // 20% of duty
    // PWM_pulsewidth(PWM_LED, 2); //  a Pulse of 2msec =(duty=1/10=20%)

    // PWM of 1 msec:  TIM2_CH1 (PA_5)
    // PWM_init_us(PWM_LED, 1000);      // 1 msec period -> 1 kHz
    // PWM_duty(PWM_LED, 0.2f);     // 20% of duty
}



int main(void) {
    // Initialization --------------------------------------------------
    setup();    

    // Infinite Loop ---------------------------------------------------
    while(1){       
        for (int i=0; i<5; i++) {                       
            PWM_duty(PWM_LED, (float)0.2*i);                    
            delay_ms(1000);
        }       
    }
}