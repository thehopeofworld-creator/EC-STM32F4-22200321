/*----------------------------------------------------------------\
@ Embedded Controller by Young-Keun Kim - Handong Global University
Author           : Jisu Park
Created          : 17-10-2026
Modified         : 17-10-2026
Language/ver     : C in VS Code

Description      : LAB - USART LED
/----------------------------------------------------------------*/

#include <string.h>                 // memset(), memcpy()
#include "ecSTM32F411v2.h"          // this should include ecUART2.h

#define MAX_BUF 50                  // max length of a string
#define END_CHAR 13                 // Enter key

static volatile uint8_t buffer[MAX_BUF]={0, };
static volatile uint8_t PC_string[MAX_BUF]={0, };
static volatile uint8_t PC_data = 0;

static volatile int idx = 0;
static volatile int bReceive =0;

void setup(void){
    RCC_PLL_init();                 // System Clock = 84MHz
    SysTick_init();                 // SysTick for delay_ms()

    GPIO_init(LED_PIN, OUTPUT);     // LED (LD2, PA_5)

    // USART2: USB serial init
    USART2_init();
    // USART1: other MCU serial init (PA_9 = TX, PA_10 = RX)
    USART1_init();
    USART1_baud(BAUD_9600);         // baud rate setting for USART1
}

int main(void){
    setup();
    printf("MCU Initialized\r\n");

    while(1){
        // A complete string (until the Enter key) is displayed on the PC
        if (bReceive == 1){
            printf("PC_string: %s\r\n", PC_string);
            bReceive = 0;
        }
    }
}

void USART2_IRQHandler(){       // USART2 RX Interrupt : key typed on the PC
    if(is_USART2_RXNE()){
        PC_data = USART2_read();             // RX from UART2 (PC)

        if (PC_data == END_CHAR)
            USART2_write((uint8_t*)"\r\n", 2);   // Enter : new line on the PC
        else
            USART2_write(&PC_data,1);        // TX to USART2 (PC) Echo of keyboard typing

        USART1_write(&PC_data,1);            // TX to the other MCU (USART1)
    }
}

// Receive a string from the other MCU through PA9/PA10.
void USART1_IRQHandler(){       // USART1 RX Interrupt : character from the other MCU
    if(is_USART1_RXNE()){
        PC_data = USART1_read();

        // LED control : 'H' = ON, 'L' = OFF
        if (PC_data == 'H')      GPIO_write(LED_PIN, HIGH);
        else if (PC_data == 'L') GPIO_write(LED_PIN, LOW);

        // Creates a String from received serial character
        if(PC_data != END_CHAR && (idx < MAX_BUF)){
            buffer[idx] = PC_data;
            idx++;
        }
        else if (PC_data == END_CHAR){
            bReceive = 1;
            // reset PC_string
            memset(PC_string, 0, sizeof(char) * MAX_BUF);
            // copy to PC_string
            memcpy(PC_string, buffer, sizeof(char) * idx);
            // reset buffer
            memset(buffer, 0, sizeof(char) * MAX_BUF);
            idx = 0;
        }
        else{       // if(idx >= MAX_BUF)
            idx = 0;
            // reset PC_string
            memset(PC_string, 0, sizeof(char) * MAX_BUF);
            // reset buffer
            memset(buffer, 0, sizeof(char) * MAX_BUF);
            printf("ERROR : Too long string\r\n");
        }
    }
}