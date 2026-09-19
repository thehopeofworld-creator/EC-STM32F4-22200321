/*----------------------------------------------------------------\
@ Embedded Controller by Young-Keun Kim - Handong Global University
Author           : Jisu Park
Created          : 05-03-2021
Modified         : 14-09-2026 
Language/ver     : C++ in VS Code

Description      : Toggle multiple LEDs by Button B1 pressing
/----------------------------------------------------------------*/

#include "stm32f4xx.h"
#include "ecRCC2.h"
#include "ecGPIO2.h"

PinName_t LED_pin [4] = {PB_12, PB_13, PB_14, PB_15};
PinName_t button_pin = PA_4;

void setup(void);
	
int main(void) { 
	// Initialiization --------------------------------------------------------
	setup();

	int currentLED = 0;
	int buttonPrev = 1;   

	// Inifinite Loop ----------------------------------------------------------
	while(1){
		// TODO
		// TODO
			int buttonNow = GPIO_read(button_pin);

			if (buttonPrev == 1 && buttonNow == 0) {
				
				if (GPIO_read(button_pin) == 0) {
					
					GPIO_write(LED_pin[currentLED], LOW);
					currentLED = currentLED + 1;
					if (currentLED == 4) {
					currentLED = 0;
				}

				GPIO_write(LED_pin[currentLED], HIGH);    
			}
		}
		buttonPrev = buttonNow;
		delay_ms_HSI(100);
	}
}
// Initialiization 
void setup(void)
{
	RCC_HSI_init();

	// Configuration button pin
	GPIO_init(button_pin, INPUT);  // calls RCC_GPIOC_enable()
	GPIO_pupd(button_pin, 01);     // Pull-up

	// Configuration LED pins
	for (int i = 0; i < 4; i++) {
		GPIO_init(LED_pin[i], OUTPUT);  // calls RCC_GPIOA_enable()
		GPIO_pupd(LED_pin[i], PULL_UP);		// Pull-up
		GPIO_otype(LED_pin[i], OPEN_DRAIN);		// Open-drain
		GPIO_ospeed(LED_pin[i], MEDIUM_SPEED);	// Medium speed
	}

	GPIO_write(LED_pin[0], HIGH);	// Initially only LED0 is ON
	GPIO_write(LED_pin[1], LOW);
	GPIO_write(LED_pin[2], LOW);
	GPIO_write(LED_pin[3], LOW);

}