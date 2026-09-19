
#include "stm32f4xx.h"

#define LED_PIN    5
#define BUTTON_PIN 13

// LED ON or OFF 

int main(void) {
/* Part 1. RCC Register Setting */
	// RCC Control Register (HSI)
	RCC->CR |= ((uint32_t)RCC_CR_HSION);
	// wait until HSI is ready
	while ( (RCC->CR & (uint32_t) RCC_CR_HSIRDY) == 0 ) {;}
	// Select HSI as system clock source
	// RCC Configuration Register
	RCC->CFGR &= (uint32_t)((uint32_t) ~(RCC_CFGR_SW));
	RCC->CFGR |= (uint32_t)RCC_CFGR_SW_HSI;
	// Wait till HSI is used as system clock source
	while ((RCC->CFGR & (uint32_t)RCC_CFGR_SWS) != 0 ) {;}
	// HSI is used as system clock
	// RCC Peripheral Clock Enable Register
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

/* Part 2. GPIO Register Setting */
	// GPIO Mode Register
	GPIOA->MODER &= ~(3UL<<(2*LED_PIN));
	GPIOA->MODER |=   1UL<<(2*LED_PIN);


	// Dead loop & program hangs here
	while(1){
		// Turn ON LED2
		GPIOA->ODR |= (1UL << LED_PIN);

		// Turn OFF LED2
		//GPIOA->ODR &= ~(1UL << LED_PIN);
	}
}