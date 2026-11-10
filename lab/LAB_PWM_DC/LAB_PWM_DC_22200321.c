/*----------------------------------------------------------------\
@ Embedded Controller by Young-Keun Kim - Handong Global University
Name             : Jisu Park
Created          : 2026-11-05
Language/ver     : C in VS Code

Description      : LAB - PWM DC motor
/----------------------------------------------------------------*/

#include <stdio.h>
#include "ecSTM32F411v2.h"
#include "ecPWM2.h"                     
#include "ecUART2.h"                    

// ---------- pins ----------
#define PWM_PIN   PA_0                  // TIM2_CH1 : motor driver PWM input
#define DIR_PIN   PC_2                  // motor driver direction input 
#define IR_PIN    PA_4                  // IR proximity sensor 

#define IR_NEAR_LEVEL   0               // pin level when an object is near (IR proximity sensor : LOW)
#define BLINK_TICKS     2               // PAUSE : LED toggles every 2 x 500 msec = 1 sec
#define PRINT_TICKS     4               // print every 4 x 500 msec = 2 sec

// ---------- FSM ----------
typedef enum { ST_OFF = 0, ST_MID, ST_HIGH, ST_PAUSE_MID, ST_PAUSE_HIGH, N_STATE } State_t;
typedef enum { LED_OFF = 0, LED_ON, LED_BLINK } LedMode_t;

// input = (object << 1) | button :  0 = nothing,  1 = button,  2 = object near,  3 = object near + button
typedef struct {
	float duty;             	      // output : speed of the fan (0 ~ 1.0)
	LedMode_t ledMode;                // output : LED
	State_t next[4];             	  // next state for each input
	const char *name;                 // text for the serial display
} FSM_t;

const FSM_t fsm[N_STATE] = {
	/* OFF        */ { 0.0f, LED_OFF,   { ST_OFF,  ST_MID,  ST_OFF, 		ST_MID}, 		 "OFF"         },
	/* MID        */ { 0.5f, LED_ON,    { ST_MID,  ST_HIGH, ST_PAUSE_MID,   ST_PAUSE_MID  }, "MID"         },
	/* HIGH       */ { 1.0f, LED_ON,    { ST_HIGH, ST_OFF,  ST_PAUSE_HIGH,  ST_PAUSE_HIGH }, "HIGH"        },
	/* PAUSE_MID  */ { 0.0f, LED_BLINK, { ST_MID,  ST_MID,  ST_PAUSE_MID,   ST_PAUSE_MID  }, "PAUSE(MID)"  },
	/* PAUSE_HIGH */ { 0.0f, LED_BLINK, { ST_HIGH, ST_HIGH, ST_PAUSE_HIGH,  ST_PAUSE_HIGH }, "PAUSE(HIGH)" }
};

volatile State_t state = ST_OFF;    	// present state
volatile uint8_t bButton = 0;        	// 1 : B1 was pressed
volatile uint8_t bPrint = 0;         	// 1 : main() prints the status
volatile uint32_t tick = 0;         	// number of TIM3 interrupts (500 msec each)
volatile uint8_t blink = 0;         	// blink counter in PAUSE
volatile uint8_t led = 0;        		// LED level

uint8_t DIR = 0;               			// level of the direction pin
float   targetPWM = 0.0f;               // target speed command 0 ~ 1.0

// LED on / off
static void led_set(uint8_t on){
	led = on;
	if (on) GPIO_write(LED_PIN, HIGH);
	else    GPIO_write(LED_PIN, LOW);
}

// Motor : (DIR = 1 : duty = 1 - target)
static void motor(float target){
	targetPWM = target;
	PWM_duty(PWM_PIN, fabsf((float)DIR - targetPWM));
}

// Outputs of a state
static void state_output(State_t s){
	motor(fsm[s].duty);
	if (fsm[s].ledMode == LED_OFF) led_set(0);
	if (fsm[s].ledMode == LED_ON)  led_set(1);
	if (fsm[s].ledMode == LED_BLINK){   // the LED starts blinking (toggled in the TIM3 handler)
		blink = 0;
		led_set(1);
	}
}

// Initialization
void setup(void){
	RCC_PLL_init();                     // System Clock = 84MHz
	SysTick_init();                     // SysTick 1 msec (msTicks)
	USART2_init();                      // serial display (USB, 9600 baud)

	GPIO_init(LED_PIN, OUTPUT);         

	GPIO_init(DIR_PIN, OUTPUT);         
	GPIO_otype(DIR_PIN, PUSH_PULL);
	GPIO_write(DIR_PIN, DIR);

	GPIO_init(IR_PIN, INPUT);
	GPIO_pupd(IR_PIN, PULL_UP);

	GPIO_init(BUTTON_PIN, INPUT);       
	GPIO_pupd(BUTTON_PIN, PULL_UP);
	EXTI_init(BUTTON_PIN, FALL, 3);     // priority 3 (TIM3 : 2 in TIM_UI_init, USART2 : 1 in USART_init)

	PWM_init(PWM_PIN, 1);               // PWM period 1 msec (1 kHz) on PA0
	PWM_duty(PWM_PIN, 0.0f);            // the default duty of PWM_init is 50 % : stop the motor at once
	TIM_UI_init(TIM3, 500);             // TIM3 interrupt every 500 msec
}


int main(void){
	setup();
	state_output(state);                // start in OFF : duty 0 %, LED off
	printf("Smart mini-fan started\r\n");

	// Infinite Loop ----------------------------------------------------------
	while(1){
		// 1. Inputs
		uint8_t object = 0;                             // 1 : an object is near
		if (GPIO_read(IR_PIN) == IR_NEAR_LEVEL) object = 1;
		uint8_t btn = bButton;                      	// take the button event
		bButton = 0;                                   
		uint8_t input = (object << 1) | btn;            // 0 ~ 3

		// 2. Next state : read the FSM table
		State_t next = fsm[state].next[input];

		// 3. Outputs, only when the state changes
		if (next != state){
			state = next;
			state_output(state);
		}

		// 4. Serial display every 2 sec
		if (bPrint){
			bPrint = 0;

			const char *ledStr = "OFF";                 // text of the LED
			if (led) ledStr = "ON";
			const char *irStr = "AWAY";                 // text of the sensor
			if (object) irStr = "NEAR";

			printf("Mode=%s  Duty=%d%%  DIR=%d  LED=%s  IR=%s\r\n",
			       fsm[state].name,
			       (int)(fabsf((float)DIR - targetPWM) * 100.0f + 0.5f), DIR,
			       ledStr, irStr);
		}
	}
}


// TIM3 Interrupt
void TIM3_IRQHandler(void){
	if (is_UIF(TIM3)){
		clear_UIF(TIM3);                // clear the flag

		tick++;
		if (fsm[state].ledMode == LED_BLINK){           // PAUSE : the LED blinks every 1 sec
			if (++blink >= BLINK_TICKS){
				blink = 0;
				led_set(!led);
			}
		}
		if (tick % PRINT_TICKS == 0) bPrint = 1;        // every 2 sec
	}
}


// EXTI Interrupt : button B1 (PC13)
void EXTI15_10_IRQHandler(void){
	if (is_pending_EXTI(BUTTON_PIN)){
		clear_pending_EXTI(BUTTON_PIN);                 // clear the pending flag first
		bButton = 1;                                   
	}
}


// USART2 RX Interrupt
void USART2_IRQHandler(void){
	if (is_USART2_RXNE()){
		USART2_read();
	}
}