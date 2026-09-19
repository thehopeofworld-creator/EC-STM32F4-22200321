
//#include "ecSTM32F4v2.h"
#include "ecSTM32_simple.h"
#define LED_PIN PA_5
#define BUTTON_PIN PC_13

void setup(void);

int main(void) {
	setup();

	while (1) {
		delay_ms(1000);
		GPIO_write(LED_PIN, LOW);
		delay_ms(1000);
		GPIO_write(LED_PIN, HIGH);
	}
}


// Initialiization
void setup(void) {
	RCC_HSI_init();
	SysTick_init();

	// initialize the pushbutton pin as an input:
	GPIO_init(BUTTON_PIN, INPUT);
	// initialize the LED pin as an output:
	GPIO_init(LED_PIN, OUTPUT);
}
