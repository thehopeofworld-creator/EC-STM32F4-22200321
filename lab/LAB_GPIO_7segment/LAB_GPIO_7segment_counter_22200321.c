/*----------------------------------------------------------------\
@ Embedded Controller by Young-Keun Kim - Handong Global University
Author           : Jisu Park
Created          : 21-09-2021
Modified         : 22-09-2026 
Language/ver     : C++ in VS Code

Description      : LAB_GPIO_7segment counter 0~9
/----------------------------------------------------------------*/

#include "stm32f4xx.h"
#include "ecGPIO2.h"
#include "ecRCC2.h"

PinName_t pinFND[8]    = {PB_0, PB_1, PB_2, PB_3, PB_4, PB_5, PB_6, PB_7};
PinName_t button_pin = PA_4;
PinName_t selectFND = PA_10;


void setup(void){
    // Intialize System Clock
    RCC_HSI_init();    

    // Intialize FND pins and Others
    FND_init(pinFND);

    // Intialize  Others
    GPIO_init(button_pin, INPUT);  // calls RCC_GPIOC_enable()
    GPIO_pupd(button_pin, PULL_UP);     // Pull-up

    GPIO_init(selectFND, OUTPUT);  // calls RCC_GPIOA_enable()
    GPIO_otype(selectFND, PUSH_PULL);		// Push-pull
    GPIO_write(selectFND, HIGH);	// Initially selectFND is HIGH

    for (int i = 0; i < 8; i++) {
        GPIO_pupd(pinFND[i], NO_PUPD);
    }
}

int main(void) {
    setup();

    uint8_t count = 0;
    int buttonPrev = 1;  

    FND_display(count);

    while (1) {
        int buttonNow = GPIO_read(button_pin);

        if(buttonPrev == 1 && buttonNow == 0) {
            delay_ms_HSI(20);
            if (GPIO_read(button_pin) == 0) {
                count = count + 1;
                if (count == 10) {
                    count = 0;
                }
                FND_display(count);
            }
        }
        
        buttonPrev = buttonNow;
    }
    
}

    


