/*----------------------------------------------------------------\
@ Embedded Controller by Young-Keun Kim - Handong Global University
Author           : Jisu Park
Created          : 10-16-2026
Modified         : 11-05-2026 
Language/ver     : C in VS Code

Description      : Tutorial - Timer Input Capture example 1

                   Measures the signal with ICAP of  PB_6 (TIM4_CH1).
                   A test PWM signal is generated on PA_6 (TIM3_CH1) 
                   Connect  PA_5 --> PA_6  
/----------------------------------------------------------------*/

#include "stm32f411xe.h"
#include "ecGPIO2.h"
#include "ecRCC2.h"
#include "ecSysTick2.h"
#include "ecTIM2.h"
#include "ecICAP2.h"
#include "ecPWM2.h"
#include "ecUART2.h"

#define ICAP_PIN    PB_6        // TIM4_CH1 : capture input
#define PWM_PIN     PA_6        // TIM3_CH1 : TRIG test signal (jumper PA_6 -> PB_6)
#define LED_PIN     PA_5        // TIM2_CH1 : LED output

uint32_t pwm_period_ms=50;
uint32_t uTick=100;
uint32_t timer_arr;   // ticks per counter wrap (ARR+1)

volatile uint32_t ovf_cnt = 0;      //counts overflows
volatile uint32_t cap_prev   = 0;   // CCR of the previous rising edge
volatile uint32_t cap_curr    = 0;  // CCR of the current rising edge
volatile uint32_t counts = 0;       // [pulses]
volatile float timeInterval = 0;    // [usec]
volatile uint8_t  _started   = 0;   

void setup(void){
    RCC_PLL_init();
    SysTick_init();
    GPIO_init(LED_PIN,OUTPUT);
    USART2_init();                  

    // PWM: Test signal of period_ms of 50% duty 
    PWM_init(PWM_PIN, pwm_period_ms);
    PWM_duty(PWM_PIN, 0.5f);

    // Input capture  rising edge TIn=ICn, tick = uTick usec    
    ICAP_init(ICAP_PIN, IC_RISE);   
    ICAP_tick(ICAP_PIN, uTick);         

    timer_arr=TIM4->ARR + 1;
}

int main(void){
    setup();
    printf("TU_TIM_ICAP : jumper PA_6 -> PB_6, expect %lu msec\r\n", pwm_period_ms);

    while(1){
        printf("counts %lu, timeInterval = (%.2f msec)\r\n", counts, timeInterval/1000);        
        led_toggle(LED_PIN);
        delay_ms(1000);
    }
}

void TIM4_IRQHandler(void){
    // Counter overflow 
    if(is_UIF(TIM4)){
        ovf_cnt++;
        clear_UIF(TIM4);
    }

    // Rising edge captured on IC1
    if(is_CCIF(TIM4, IC_1)){
        // capture the counter
        cap_curr = ICAP_read(ICAP_PIN, IC_1);

        // ignore the first capture: when cap_prev=0
        if(_started){
            // Ticks between the two edges, then scaled to usec.
            counts = (cap_curr-cap_prev) + (ovf_cnt*timer_arr);
            timeInterval=counts*uTick;
        }
        cap_prev=cap_curr;      
        ovf_cnt    = 0;
        _started = 1;
        clear_CCIF(TIM4, IC_1);
    }
}