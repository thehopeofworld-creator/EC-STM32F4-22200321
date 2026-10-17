#include <stdio.h>
#include "ecRCC2.h"
#include "ecGPIO2.h"
#include "ecSysTick2.h"
#include "ecUART2.h"


uint8_t PC_string[]="MCU Initialized \r\n";

// Initialiization 
void setup(void)
{
	// MCU_init();
    RCC_PLL_init();
    SysTick_init();
    // USART2: PA_2=TX, PA_3=RX, 57600 baud
    USART_init(USART2, PA_2, PA_3, BAUD_57600);	
}


int main(void) {
	// Initialiization --------------------------------------------------------
	setup();
  USART2_write(PC_string, sizeof(PC_string));
	
  uint32_t lineNumber = 0;
  // Inifinite Loop ----------------------------------------------------------
	while (1){
        printf("Line %lu >>\r\n", lineNumber);
        delay_ms(2000);
        lineNumber++;
    }
}
