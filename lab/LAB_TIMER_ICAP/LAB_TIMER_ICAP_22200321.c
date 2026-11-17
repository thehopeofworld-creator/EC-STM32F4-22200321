/*----------------------------------------------------------------\
@ Embedded Controller by Young-Keun Kim - Handong Global University
Name             : Jisu Park
Created          : 2026-11-05
Language/ver     : C in VS Code

Description      : LAB : Input Capture - Ultrasonic Sensor
/----------------------------------------------------------------*/

#include <stdio.h>
#include "ecSTM32F411v2.h"

// ---------- pins ----------
#define PWM_PIN   PA_6                  // TIM3_CH1 : trigger pulse to the sensor
#define IC_PIN    PB_6                  // TIM4_CH1 : input capture pin (echo pulse from the sensor)

#define TICK_US     10                  // input capture counter clock : 0.1 MHz = 10 usec per tick
#define PRINT_MS    500                 // print period
#define MIN_CM      2                   // HC-SR04 measuring range : 2 cm ~ 400 cm
#define MAX_CM      400

// ---------- measurement ----------
uint32_t timer_arr = 65536;             // counts of one TIM4 overflow (ARR + 1)

volatile uint32_t ovf_cnt   = 0;        // number of overflows of TIM4 since the rising edge
volatile uint32_t cap_rise  = 0;        // counter value at the rising edge  (IC1)
volatile uint32_t cap_fall  = 0;        // counter value at the falling edge (IC2)
volatile uint32_t counts    = 0;        // width of the echo pulse [ticks]
volatile uint32_t echo_us   = 0;        // width of the echo pulse [usec]
volatile uint8_t  have_rise = 0;        // 1 : the rising edge of this pulse was captured
volatile uint8_t  bNew      = 0;        // 1 : a new echo pulse was measured


// Initialization
void setup(void){
	RCC_PLL_init();                                 // System Clock = 84MHz
	SysTick_init();                                 // Systick init (delay_ms)
	USART2_init();                                  // serial display (USB, 9600 baud)

	PWM_init(PWM_PIN, 100);                         // PA6 : AF, TIM3 CH1 PWM mode 1, period 100 msec
	GPIO_otype(PWM_PIN, PUSH_PULL);                 // push-pull
	GPIO_pupd(PWM_PIN, NO_PUPD);                    // no pull-up, no pull-down
	GPIO_ospeed(PWM_PIN, FAST_SPEED);               // fast

	TIM3->PSC  = (EC_SYSCLK / 100000UL) - 1;        // 84 MHz / 840 = 100 kHz  ->  PSC = 839
	TIM3->ARR  = 10000 - 1;                         // 10,000 ticks x 10 usec = 100 msec
	TIM3->CCR1 = 1;                                 // 1 tick = 10 usec : high while CNT < CCR1
	TIM3->EGR |= TIM_EGR_UG;                        // load PSC, ARR and CCR1 (preload) now

	// Input capture on PB6 (TIM4_CH1) : initialize in this order
	ICAP_init(IC_PIN, IC_RISE);                     // TI1 -> IC1, rising edge
	ICAP_ICn_init(IC_PIN, IC_FALL, IC_2);           // TI1 -> IC2, falling edge (the same pin)
	ICAP_tick(IC_PIN, TICK_US);                     // counter clock 0.1 MHz : call this LAST

	timer_arr = TIM4->ARR + 1;                      // 65,536 ticks (16-bit counter)
}


int main(void){
	setup();
	printf("HC-SR04 : TRIG PA6, ECHO PB6, tick = %d usec\r\n", TICK_US);

	// Infinite Loop ----------------------------------------------------------
	while(1){
		if (bNew){
			bNew = 0;
			uint32_t us  = echo_us;                 // width of the echo pulse [usec]
			uint32_t d10 = us * 10 / 58;            // distance [cm] x 10 : 1 cm = 58 usec

			if (d10 < MIN_CM * 10 || d10 > MAX_CM * 10){       // out of the range 2 cm ~ 400 cm : not a valid distance
				printf("Distance = out of range   (echo = %lu usec)\r\n", (unsigned long)us);
			}
			else{
				printf("Distance = %lu.%lu cm   (echo = %lu usec)\r\n", (unsigned long)(d10 / 10), (unsigned long)(d10 % 10), (unsigned long)us);
			}
		}
		else{
			printf("no echo   : out of range or no object\r\n");     // no falling edge in this period
		}
		delay_ms(PRINT_MS);
	}
}


// TIM4 Interrupt : counter overflow, rising edge (IC1), falling edge (IC2)
void TIM4_IRQHandler(void){
	// Counter overflow : the counter counts 65,536 ticks (655 msec)
	if (is_UIF(TIM4)){
		ovf_cnt++;
		clear_UIF(TIM4);
	}

	// Rising edge : the echo pulse starts
	if (is_CCIF(TIM4, IC_1)){
		cap_rise  = ICAP_read(IC_PIN, IC_1);
		ovf_cnt   = 0;                              // count the overflows from this edge
		have_rise = 1;
		clear_CCIF(TIM4, IC_1);
	}

	// Falling edge : the echo pulse ends
	if (is_CCIF(TIM4, IC_2)){
		cap_fall = ICAP_read(IC_PIN, IC_2);
		if (have_rise){
			counts  = (cap_fall - cap_rise) + (ovf_cnt * timer_arr);   // ticks between the two edges
			echo_us = counts * TICK_US;                                 // [usec]
			bNew    = 1;
			have_rise = 0;
		}
		clear_CCIF(TIM4, IC_2);
	}
}


// USART2 RX Interrupt : a key typed in the USB terminal (not used, read it to clear the flag)
void USART2_IRQHandler(void){
	if (is_USART2_RXNE()){
		USART2_read();
	}
}