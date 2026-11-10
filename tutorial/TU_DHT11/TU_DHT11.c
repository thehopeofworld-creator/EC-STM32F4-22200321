/*----------------------------------------------------------------\
@ Embedded Controller by Young-Keun Kim - Handong Global University
Author           : [ YOUR NAME GOES HERE !!!!!]
Created          : 10-27-2026
Modified         : 00-00-2026 [WRITE THE DATE!!!!]
Language/ver     : C in VS Code

Description      : Tutorial - DHT11 temperature / humidity sensor
                   DATA = PB_12, external pull-up on the data line
/----------------------------------------------------------------*/
#include <stdio.h>
#include "ecRCC2.h"
#include "ecSysTick2.h"
#include "ecUART2.h"
#include "ecDHT11.h"

#define DHT11_PIN	PB_12

int err = 0;

// Initialiization
void setup(void)
{
	RCC_PLL_init();			// System Clock = 84MHz
	SysTick_init();
	USART2_init();			// USB serial init
	DHT11_init(DHT11_PIN);	// DATA = PB_12
}


int main(void) {
	// Initialiization --------------------------------------------------------
	setup();
	printf("DHT11 start\r\n");

	// Inifinite Loop ----------------------------------------------------------
	while(1){
		err = DHT11_read();

		if(err == DHT11_OK)
			printf("T = %d C,  RH = %d %% \r\n", DHT11_temperature(), DHT11_humidity());
		else
			printf("DHT11 error = %d \r\n", err);

		delay_ms(2000);		// the sensor needs 2 s between reads
	}
}
