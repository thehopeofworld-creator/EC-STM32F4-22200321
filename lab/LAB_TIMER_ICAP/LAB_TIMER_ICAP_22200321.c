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
#define PWM_PIN   PA_6                  // TIM3_CH1 : motor driver PWM input
#define IC_PIN   PB_6                   // TIM4_CH1 : input capture pin

// Initialiization 
void setup(void){	
	RCC_PLL_init();                                 // System Clock = 84MHz
	SysTick_init();                                 // Systick init
	
	
}