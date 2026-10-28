// motor arrow key control with Bluetooth (HC-06) and Tera Term
#include <stdio.h>
#include "ecSTM32F411v2.h"          // this should include ecUART2.h, ecStepper2.h, ecEXTI2.h, ecSysTick2.h

#define T_FIRST_MS     700          // timeout after the FIRST arrow code (the PC waits ~500 msec before it repeats)
#define T_REPEAT_MS    150          // timeout while the key is held (the PC repeats a code every ~33 msec)
#define REPEAT_GAP_MS  100          // two codes closer than this : the key is held (auto repeat)

#define MIN_RPM   1
#define MAX_RPM   14                // the motor could not follow a step delay shorter than 2 msec (Stepper lab)

#define MSG_NONE   0
#define MSG_RIGHT  1
#define MSG_LEFT   2
#define MSG_STOP   3
#define MSG_SPEED  4

// ( A, B,  AN,  BN)
PinName_t pinStepper[4] = {PB_10, PB_4, PB_5, PB_3};

static volatile uint8_t  run       = 0;           // 1: the motor rotates
static volatile uint32_t dir       = DIR_CW;      // rotation direction
static volatile uint8_t  repeating = 0;           // 1: the key is held (codes come with a short period)
static volatile uint32_t last_rx   = 0;           // msTicks at the last key code
static volatile uint8_t  msg       = MSG_NONE;    // state message that main() prints
static volatile uint8_t  rpm       = 8;           // speed [rpm]
static volatile uint32_t period_us = 0;           // time of one step [usec] = 60000000 / (STEP_PER_REV * rpm)
static uint8_t esc = 0;                           // arrow key parser: 0 = idle, 1 = got ESC, 2 = got ESC [

// Set the speed.
// stepper_speed() rounds the step delay down to an integer [msec] (6 and 7 rpm would give the same 4 msec),
// so the timing is made here with a [usec] period. The library delay is set to 0 (stepper_speed(60) = 0 msec).
static void set_speed(uint8_t r){
	rpm = r;
	period_us = 60000000UL / (STEP_PER_REV * r);
}

// Stop the motor (called from main() and from the handlers)
static void halt(void){
	run = 0;
	repeating = 0;
	stepper_stop();                               // all coils off
	GPIO_write(LED_PIN, LOW);                     // LD2 OFF
	msg = MSG_STOP;
}

// Initialization
void setup(void){
	RCC_PLL_init();                               // System Clock = 84MHz
	SysTick_init();                               // SysTick 1 msec : delay_ms(), msTicks

	GPIO_init(LED_PIN, OUTPUT);                   // LED (LD2, PA_5)

	GPIO_init(BUTTON_PIN, INPUT);                 // push-button PC13 : emergency stop
	GPIO_pupd(BUTTON_PIN, PULL_UP);
	EXTI_init(BUTTON_PIN, FALL, 0);               // priority 0 : higher than USART1 (priority 1)

	USART2_init();                                // USART2 : USB terminal (messages)
	USART1_init();                                // USART1 : Bluetooth module, PA_9 = TX, PA_10 = RX
	USART1_baud(BAUD_9600);                       // HC-06 default baud rate

	stepper_init(FULL, pinStepper);               // full-stepping
	stepper_speed(60);                            // library delay = 0 msec : the step timing is made in main()
	set_speed(rpm);                               // start speed
}


int main(void){
	setup();
	printf("Arrow key control ready\r\n");

	uint8_t  was_run = 0;                         // run in the last loop
	uint32_t prev_ms = 0;                         // msTicks of the last time check
	uint32_t acc_us  = 0;                         // elapsed time since the last step [usec]

	// Infinite Loop ----------------------------------------------------------
	while(1){
		if (run){
			if (!was_run){                        // the motor has just started : the first step at once
				was_run = 1;
				prev_ms = msTicks;
				acc_us  = period_us;
			}

			// Step timing : add the elapsed time, step when it is longer than the step period
			uint32_t now = msTicks;
			uint32_t el  = now - prev_ms;         // elapsed [msec] since the last check (normally 0 or 1)
			if (el){
				prev_ms = now;
				acc_us += el * 1000;
			}
			if (acc_us >= period_us){
				acc_us -= period_us;
				if (acc_us > 2 * period_us) acc_us = period_us;   // after a long pause (printf) : do not make a burst of steps
				stepper_step(1, dir);             // one step (no delay)
			}

			// Key released : no key code for 'limit' msec
			uint32_t limit = repeating ? T_REPEAT_MS : T_FIRST_MS;
			if (run && (msTicks - last_rx) > limit){
				__disable_irq();                  // the handler must not change the state between the check and halt()
				if (run && (msTicks - last_rx) > limit) halt();
				__enable_irq();
			}
		}
		else if (was_run){                        // the motor was stopped (by main() or by a handler)
			was_run = 0;
			stepper_stop();                       // release all coils (the handler may have stopped in the middle of a step)
		}

		// State message (printed here, not in the handlers, because printf() is slow)
		if (msg != MSG_NONE){
			uint8_t m = msg;
			msg = MSG_NONE;
			if (m == MSG_RIGHT)      printf("RIGHT\r\n");
			else if (m == MSG_LEFT)  printf("LEFT\r\n");
			else if (m == MSG_SPEED) printf("SPEED %d rpm\r\n", (int)rpm);
			else                     printf("STOP\r\n");
		}
	}
}


// USART1 RX Interrupt : a byte from the Bluetooth module
// An arrow key is NOT one character. Tera Term sends 3 bytes :
//   ESC [ C (right)   ESC [ D (left)   ESC [ A (up)   ESC [ B (down)
void USART1_IRQHandler(void){
	if (is_USART1_RXNE()){
		uint8_t c = USART1_read();                // RX from USART1 (Bluetooth)
		int arrow = 0;                            // 0 : none, 1 : right, 2 : left, 3 : up, 4 : down

		if (esc == 0){
			if (c == 0x1B)     esc = 1;           // ESC : start of an arrow key sequence
			else if (c == ' ') halt();            // space bar : emergency stop
		}
		else if (esc == 1){
			esc = (c == '[' || c == 'O') ? 2 : 0; // ESC [  (or ESC O : application cursor mode)
		}
		else{                                     // esc == 2 : the last byte of the sequence
			if (c == 'C')      arrow = 1;         // right arrow
			else if (c == 'D') arrow = 2;         // left arrow
			else if (c == 'A') arrow = 3;         // up arrow
			else if (c == 'B') arrow = 4;         // down arrow
			esc = 0;
		}

		if (arrow >= 3){                          // up / down : speed (the motor keeps its state)
			if (arrow == 3 && rpm < MAX_RPM) set_speed(rpm + 1);
			if (arrow == 4 && rpm > MIN_RPM) set_speed(rpm - 1);
			msg = MSG_SPEED;
			if (run) last_rx = msTicks;           // while the motor rotates, a speed key also keeps it running
		}
		else if (arrow){                          // right / left : rotation
			uint32_t newdir = (arrow == 1) ? DIR_CW : DIR_CCW;
			uint32_t gap = msTicks - last_rx;     // time since the last key code

			if (run && newdir == dir){            // the same key again
				if (gap < REPEAT_GAP_MS) repeating = 1;     // short period : the key is held
			}
			else{                                 // new press, or the opposite key
				dir = newdir;
				repeating = 0;
				run = 1;
				GPIO_write(LED_PIN, HIGH);        // LD2 ON while the motor rotates
				msg = (arrow == 1) ? MSG_RIGHT : MSG_LEFT;
			}
			last_rx = msTicks;
		}
	}
}


// EXTI Interrupt : push-button PC13 = emergency stop
void EXTI15_10_IRQHandler(void){
	if (is_pending_EXTI(BUTTON_PIN)){
		clear_pending_EXTI(BUTTON_PIN);           // clear the pending flag first
		halt();
	}
}


// USART2 RX Interrupt : a key typed in the USB terminal (not used, read it to clear the flag)
void USART2_IRQHandler(void){
	if (is_USART2_RXNE()){
		USART2_read();
	}
}




//count push-button presses and send the count to the Bluetooth terminal (HC-06) and Tera Term
/*

#include <stdio.h>                  // snprintf()
#include <string.h>                 // strlen()
#include "ecSTM32F411v2.h"          // this should include ecUART2.h, ecEXTI2.h, ecSysTick2.h

#define DEBOUNCE_MS   20            // ignore the bounce shorter than this

static volatile uint32_t count      = 0;      // number of presses
static volatile uint32_t last_press = 0;      // msTicks of the last accepted press
static volatile uint8_t  bPressed   = 0;      // 1 : main() sends the count
static volatile uint8_t  bCleared   = 0;      // 1 : main() sends "CLEARED"

// Send a string to the Bluetooth terminal (blocking, so it is called only from main())
static void bt_send(const char *s){
	USART1_write((uint8_t*)s, strlen(s));
}

// Initialization
void setup(void){
	RCC_PLL_init();                               // System Clock = 84MHz
	SysTick_init();                               // SysTick 1 msec : msTicks

	GPIO_init(LED_PIN, OUTPUT);                   // LED (LD2, PA_5)

	GPIO_init(BUTTON_PIN, INPUT);                 // push-button PC13
	GPIO_pupd(BUTTON_PIN, PULL_UP);
	EXTI_init(BUTTON_PIN, FALL, 0);               // priority 0 : higher than USART1 (priority 1),
	                                              // so count++ is never interrupted by the handler that clears the count
	USART1_init();                                // USART1 : Bluetooth module, PA_9 = TX, PA_10 = RX
	USART1_baud(BAUD_9600);                       // HC-06 default baud rate
}


int main(void){
	setup();

	char buf[24];

	// Infinite Loop ----------------------------------------------------------
	while(1){
		// A press : show the count
		if (bPressed){
			bPressed = 0;
			uint32_t n = count;                   // read once : count is changed in the handlers
			snprintf(buf, sizeof(buf), "COUNT=%lu\r\n", (unsigned long)n);
			bt_send(buf);
		}

		// Answer of the clear command
		if (bCleared){
			bCleared = 0;
			bt_send("CLEARED\r\n");
		}
	}
}


// EXTI Interrupt : push-button PC13
void EXTI15_10_IRQHandler(void){
	if (is_pending_EXTI(BUTTON_PIN)){
		clear_pending_EXTI(BUTTON_PIN);           // clear the pending flag first
		if ((msTicks - last_press) >= DEBOUNCE_MS){   // not a bounce
			count++;
			last_press = msTicks;
			bPressed = 1;                         // main() sends the count
		}
	}
}


// USART1 RX Interrupt : a key from the Bluetooth module
void USART1_IRQHandler(void){
	if (is_USART1_RXNE()){
		uint8_t c = USART1_read();                // RX from USART1 (Bluetooth)

		if (c == 'C'){                            // clear the count
			count = 0;
			bCleared = 1;                         // main() sends "CLEARED"
		}
		else if (c == 'H') GPIO_write(LED_PIN, HIGH);   // LED ON
		else if (c == 'L') GPIO_write(LED_PIN, LOW);    // LED OFF
	}
}
*/


// Stepper motor control with Bluetooth (HC-06) and Tera Term
/*

#include <stdio.h>                  // snprintf()
#include <string.h>                 // strlen(), memcpy()
#include "ecSTM32F411v2.h"          // this should include ecUART2.h, ecStepper2.h, ecSysTick2.h

#define STEPS_REV   (2 * STEP_PER_REV)                  // half-stepping : 4096 steps per revolution
#define RPM         8
#define PERIOD_US   (60000000UL / (STEPS_REV * RPM))    // time of one step [usec] = 1831

// ( A, B,  AN,  BN)
PinName_t pinStepper[4] = {PB_10, PB_4, PB_5, PB_3};

// ---- begin pure functions (no hardware) ----
// Integer division with rounding to the nearest integer (b > 0). C division cuts toward 0, so a < 0 is handled separately.
static int32_t round_div(int32_t a, int32_t b){
	return (a >= 0) ? (a + b / 2) / b : -((-a + b / 2) / b);
}

// Integer division that rounds toward minus infinity (b > 0).  C : -1 / 360 = 0,  here : -1 -> -1
static int32_t floor_div(int32_t a, int32_t b){
	int32_t q = a / b;
	if ((a % b != 0) && (a < 0)) q--;
	return q;
}

// Parse the command "+090" / "-045" : sign + 3 digits. Returns 1 if the command is right.
static int parse_cmd(const char *s, int32_t *deg){
	if (strlen(s) != 4) return 0;
	if (s[0] != '+' && s[0] != '-') return 0;
	for (int i = 1; i < 4; i++){
		if (s[i] < '0' || s[i] > '9') return 0;
	}
	int32_t v = (s[1] - '0') * 100 + (s[2] - '0') * 10 + (s[3] - '0');
	*deg = (s[0] == '+') ? v : -v;
	return 1;
}
// ---- end pure functions ----

static int32_t total_deg = 0;       // absolute angle [deg], kept in 0 ~ 359
static int32_t pos_steps = 0;       // absolute position [step], kept in 0 ~ 4095

// receive : the handler collects one line, main() executes it
#define LINE_MAX  5
static char line[LINE_MAX + 1];     // line that is being received
static uint8_t idx = 0;             // number of received characters
static uint8_t over = 0;            // 1 : the line is too long
static char cmd[LINE_MAX + 1];      // finished line (waiting for main())
static volatile uint8_t cmd_ready = 0;
static volatile uint8_t cmd_ok    = 0;      // 0 : the line was too long
static volatile uint8_t bBusy     = 0;      // 1 : a line was lost (the previous one was not taken yet)

static void bt_send(const char *s){
	USART1_write((uint8_t*)s, strlen(s));
}

// Rotate 'delta' steps (+ : CW, - : CCW). The time of each step is made from msTicks in [usec],
// because stepper_speed() rounds the delay to an integer [msec] (the ideal delay is 1.83 msec).
static void rotate(int32_t delta){
	uint32_t dir = (delta >= 0) ? DIR_CW : DIR_CCW;
	uint32_t n   = (delta >= 0) ? (uint32_t)delta : (uint32_t)(-delta);
	uint32_t acc = PERIOD_US;                   // the first step at once
	uint32_t prev = msTicks;

	while (n > 0){
		uint32_t now = msTicks;
		uint32_t el  = now - prev;              // elapsed [msec]
		if (el){
			prev = now;
			acc += el * 1000;
		}
		if (acc >= PERIOD_US){
			acc -= PERIOD_US;
			stepper_step(1, dir);               // one step (library delay is 0)
			n--;
		}
	}
	stepper_stop();                             // release the coils
}

// Rotate by 'deg' degrees from the present position
static void move(int32_t deg){
	int32_t new_deg = total_deg + deg;                          // target angle [deg]
	// The target position is rounded from the TOTAL angle, not from each command.
	// If each command is rounded, the error of every command is added (1 deg = 11.38 steps -> 11 steps, 10 times : 3.8 steps off).
	int32_t target  = round_div(new_deg * STEPS_REV, 360);      // target position [step]
	int32_t delta   = target - pos_steps;                       // steps to move

	rotate(delta);

	pos_steps = target;
	total_deg = new_deg;

	// keep the position in one revolution (360 deg = 4096 steps exactly)
	int32_t k = floor_div(total_deg, 360);
	total_deg -= 360 * k;
	pos_steps -= STEPS_REV * k;
}

// Initialization
void setup(void){
	RCC_PLL_init();                               // System Clock = 84MHz
	SysTick_init();                               // SysTick 1 msec : msTicks

	USART1_init();                                // USART1 : Bluetooth module, PA_9 = TX, PA_10 = RX
	USART1_baud(BAUD_9600);                       // HC-06 default baud rate

	stepper_init(HALF, pinStepper);               // half-stepping
	stepper_speed(60);                            // library delay = 0 msec : the step timing is made in rotate()
}


int main(void){
	setup();

	char buf[24];
	char local[LINE_MAX + 1];

	// Infinite Loop ----------------------------------------------------------
	while(1){
		if (cmd_ready){
			memcpy(local, cmd, sizeof(local));    // take the line,
			uint8_t ok = cmd_ok;
			cmd_ready = 0;                        // so the handler can receive the next one while the motor rotates

			int32_t deg;
			if (ok && parse_cmd(local, &deg)){
				move(deg);
				snprintf(buf, sizeof(buf), "POS=%03ld deg\r\n", (long)total_deg);
				bt_send(buf);
			}
			else{
				bt_send("ERR\r\n");
			}
		}

		if (bBusy){
			bBusy = 0;
			bt_send("BUSY\r\n");
		}
	}
}


// USART1 RX Interrupt : collect one line (until Enter)
void USART1_IRQHandler(void){
	if (is_USART1_RXNE()){
		char c = (char)USART1_read();             // RX from USART1 (Bluetooth)

		if (c == '\r' || c == '\n'){              // Enter (CR or LF)
			if (idx > 0){                         // an empty line (the second character of CR LF) is ignored
				if (!cmd_ready){
					memcpy(cmd, line, idx);
					cmd[idx] = '\0';
					cmd_ok = !over;
					cmd_ready = 1;
				}
				else bBusy = 1;                   // the previous line is not taken yet
				idx = 0;
				over = 0;
			}
		}
		else if (idx < LINE_MAX) line[idx++] = c;
		else over = 1;                            // too long : the line will be wrong
	}
}
*/