/*----------------------------------------------------------------\
@ Embedded Controller by Young-Keun Kim - Handong Global University
Author           : Jisu Park / JeYun Oh
Created          : 05-03-2021
Modified         : 18-11-2026
Language/ver     : C++ in VS Code
Description      : Smart-Farm Management System (Full FSM: MANUAL / AUTO / ALARM)
/----------------------------------------------------------------*/

#include "ecSTM32F411v2.h"
#include <stdlib.h>

// === Actuator PWM & Dir Pins ===
#define FAN_PWM_PIN             PA_15  // TIM2 CH1 (1 kHz)
#define FAN_DIR_PIN             PC_2   // DC Motor Direction Pin
#define SERVO_PWM_PIN           PC_7   // TIM3 CH2 (50 Hz)
#define LED_PWM_PIN             PB_7   // TIM4 CH2 (1 kHz)

// === Analog Sensors (JADC1) ===
#define LIGHT_SENSOR_PIN        PA_0   // JDR1 (TIM5 TRGO 125ms)
#define MOISTURE_SENSOR_PIN     PA_1   // JDR2 (TIM5 TRGO 125ms)

// === Digital Sensor & Button ===
#define DHT11_PIN               PA_6   // GPIO Bit-banging (ecDHT11)
#define BUTTON_PIN              PC_13  // EXTI13 (B1 Button)

// === Communication Pins ===
#define LCD_I2C_SCL_PIN         PB_8   // I2C1 SCL (Arduino D15)
#define LCD_I2C_SDA_PIN         PB_9   // I2C1 SDA (Arduino D14)

#define BT_TX_PIN               PA_11  // USART6 TX (9600 Baud)
#define BT_RX_PIN               PA_12  // USART6 RX (9600 Baud)

// ---------- values to tune (read them from the serial output) ----------
#define LIGHT_INVERT            1      // 1: ADC gets larger when darker -> invert so that a larger value means brighter
#define DARK_TH                 300    // light <= DARK_TH : duty 100%
#define BRIGHT_TH               3800   // light >= BRIGHT_TH : duty 0%
#define LIGHT_HYST              200    // hysteresis to stop flicker around the threshold
#define LED_MIN_ON_TICKS        4      // minimum LED on-time (500 ms * 4 = 2 s)

#define MOTOR_DIR               0      // 0 or 1 (automatically handled by compensation formula)
#define PWM_PERIOD_US           1000   // 1 msec = 1 kHz

// === Moisture thresholds ===
#define MOIST_INVERT            1      // 1: ADC gets smaller when wetter -> invert so that a larger value means wetter
#define MOIST_DRY_TH            1000   // DRY alarm is raised when below this value for 5 s
#define MOIST_RECOVER_TH        1500   // DRY alarm is cleared when above this value for 5 s

// === Ventilation thresholds (ON limit > OFF limit = hysteresis) ===
#define TEMP_HIGH               27     // vent ON when temperature is above this
#define TEMP_LOW                27     // vent OFF only when temperature is below this AND humidity is also low
#define HUM_HIGH                60     // vent ON when humidity is above this
#define HUM_LOW                 55
#define ALARM_REPEAT_TICKS      4      // BT "ADD WATER" repeat period (500 ms * 4 = 2 s)

// === Window Angles ===
#define WINDOW_OPEN_DEG         30
#define WINDOW_CLOSE_DEG        90

// === FSM States ===
typedef enum {
   STATE_AUTO   = 0,
   STATE_MANUAL = 1,
   STATE_ALARM  = 2
} FSM_State_t;

volatile FSM_State_t selected_mode = STATE_MANUAL; // default mode at power-on is MANUAL
FSM_State_t current_state = STATE_MANUAL;

volatile uint8_t auto_delay_ticks = 0;             // 5 s (10 ticks) wait after entering AUTO
volatile uint8_t tick_500ms = 0;                   // TIM1 500 ms tick flag

// Manual Mode Setpoints (power-on: LED OFF, Fan OFF, Window CLOSED)
volatile float   manual_led_duty     = 0.0f;
volatile uint8_t manual_fan_on       = 0;
volatile int     manual_window_angle = WINDOW_CLOSE_DEG;

// LED
volatile uint32_t led_ADCvalue = 0;
float led_duty = 0.0f;
uint8_t led_active = 0;
uint8_t led_on_ticks = 0;

// Ventilation
static volatile int window_angle = WINDOW_CLOSE_DEG;
uint8_t fan_is_on = 0;

uint32_t temp = 0;
uint32_t hum  = 0;
uint32_t dht_err_count = 0;
uint8_t dht_fail_streak = 0;
uint8_t dht_fault = 0;              // 1: DHT11 consecutive errors -> forced ventilation in AUTO mode

// Moisture
volatile uint32_t moisture_ADCvalue = 0;
uint32_t moist = 0;
uint8_t alarm_dry = 0;              // 0: normal, 1: ALARM(DRY) active
uint8_t dry_cnt = 0;                // counted in 500 ms ticks: 10 = 5 s
uint8_t recover_cnt = 0;
volatile uint8_t alarm_ack = 0;     // 1: alarm acknowledged via BT -> only the repeated message stops (alarm stays)
uint8_t alarm_msg_cnt = 0;

// Function Prototypes
void setup(void);
void control_FSM(uint32_t light);
void update_Lighting(uint32_t light);
void update_HumTemp(void);
void control_window(int deg);
void control_Ventilation_Auto(void);
void apply_Ventilation_Outputs(uint8_t fan_on, int angle);
void update_Moisture(void);
void update_LCD(void);
void process_BT_char(uint8_t ch);
void BT_send_str(const char *str);
void notify_changes(void);

void setup(void){
   RCC_PLL_init();                                 // System Clock = 84MHz
   SysTick_init();                                 // Systick init
   USART2_init();                                  // USB Serial Debug (9600 baud)

   // Bluetooth USART6 init (PA_11 = TX, PA_12 = RX, 9600 baud)
   USART_init(USART6, BT_TX_PIN, BT_RX_PIN, BAUD_9600);
   NVIC_SetPriority(USART6_IRQn, 1);               // ecUART2.c only enables the USART1/2 NVIC lines, so USART6 is enabled here
   NVIC_EnableIRQ(USART6_IRQn);

   // Push-Button B1 (PC_13, Active-Low) EXTI13 init
   GPIO_init(BUTTON_PIN, INPUT);
   GPIO_pupd(BUTTON_PIN, PULL_UP);
   EXTI_init(BUTTON_PIN, FALL, 3);

   // I2C1 & 16x2 LCD init (PB_8 = SCL, PB_9 = SDA)
   I2C1_init();
   LCD_init();
   LCD_setCursor(0, 0);
   LCD_print((char *)"MANUAL          ");
   LCD_setCursor(0, 1);
   LCD_print((char *)"00\xDF,00%,LED  0% ");

   // Power-on Outputs: LED OFF, Fan OFF, Window CLOSED
   PWM_init(LED_PWM_PIN, 1);
   PWM_duty(LED_PWM_PIN, 0.0f);

   GPIO_init(FAN_DIR_PIN, OUTPUT);
   GPIO_otype(FAN_DIR_PIN, PUSH_PULL);
   GPIO_write(FAN_DIR_PIN, MOTOR_DIR);

   PWM_init_us(FAN_PWM_PIN, PWM_PERIOD_US);
   float initial_duty = fabsf((float)MOTOR_DIR - 0.0f);
   PWM_duty(FAN_PWM_PIN, initial_duty);

   PWM_init(SERVO_PWM_PIN, 20);
   control_window(WINDOW_CLOSE_DEG);

   // DHT11 init on PA_6
   DHT11_init(DHT11_PIN);

   // JADC 2-channel sequence (JDR1: light PA_0, JDR2: moisture PA_1, every 125 ms)
   PinName_t jadc_pins[2] = {LIGHT_SENSOR_PIN, MOISTURE_SENSOR_PIN};
   JADC_init(LIGHT_SENSOR_PIN);
   JADC_init(MOISTURE_SENSOR_PIN);
   JADC_sequence(jadc_pins, 2);
   JADC_trigger_init(TIM5, 125, ADC_RISE);

   // Start the TIM1 500 ms control-loop interrupt
   TIM_UI_init(TIM1, 500);
}

int main(void){
   setup();
   uint8_t loop_cnt = 0;

   // Read temperature/humidity once right after start-up
   update_HumTemp();

   // Infinite Loop ----------------------------------------------------------
   while(1){
      if (tick_500ms) {
         tick_500ms = 0;

         uint32_t LED   = led_ADCvalue;
         uint32_t light = LIGHT_INVERT ? (4095 - LED) : LED;

         // 1. Read DHT11 once every 2 s (500 ms * 4)
         if (++loop_cnt >= 4) {
            loop_cnt = 0;
            update_HumTemp();
         }

         // 2. Update soil-moisture alarm state (5 s hold check)
         update_Moisture();

         // 3. FSM state transition and actuator outputs
         control_FSM(light);

         // 3b. Serial debug output (before the LCD update, because the I2C LCD can block for seconds when it is missing)
         const char *st_str = (current_state == STATE_ALARM)  ? "ALARM " :
                              (current_state == STATE_AUTO)   ? "AUTO  " : "MANUAL";
         printf("[%s] light=%4lu LED_duty=%3d%% hum=%2lu%% temp=%2luC moist=%4lu fan=%d win=%d delay=%d dhtErr=%lu\r\n",
                st_str, (unsigned long)light, (int)(led_duty * 100.0f + 0.5f),
                (unsigned long)hum, (unsigned long)temp, (unsigned long)moist,
                fan_is_on, window_angle, auto_delay_ticks, (unsigned long)dht_err_count);

         // 4. Update I2C LCD
         update_LCD();

         // 5. Send BT messages when a condition changed
         notify_changes();
      }
   }
}

void control_FSM(uint32_t light){
   // 1. Decide state: DRY alarm has top priority; when it clears, return to the selected mode (AUTO/MANUAL)
   if (alarm_dry) {
      current_state = STATE_ALARM;
   } else {
      current_state = selected_mode;
   }

   // 2. Behaviour of the current state
   switch (current_state) {
      case STATE_AUTO:

         // When AUTO is entered, wait 5 s (10 ticks) before the light sensing starts
         if (auto_delay_ticks > 0) {
            auto_delay_ticks--;
            led_active = 0;
            led_on_ticks = 0;
            led_duty = 0.0f;
         } else {
            float prev_duty = led_duty;
            update_Lighting(light);
            led_duty = prev_duty + (led_duty - prev_duty) * 0.25f;   // smooth the duty (stops the LED light from feeding back into the sensor and flickering)
            if (led_duty < 0.02f) led_duty = 0.0f;
            if (led_duty > 0.98f) led_duty = 1.0f;
         }

         // Automatic ventilation (T/H hysteresis + forced ventilation on DHT11 fault)
         control_Ventilation_Auto();

         // LED PWM output
         GPIO_mode(LED_PWM_PIN, AF);
         PWM_duty(LED_PWM_PIN, led_duty);
         break;

      case STATE_MANUAL:
         auto_delay_ticks = 0;
         led_active = 0;
         led_on_ticks = 0;

         // Apply the manual values set by Bluetooth
         led_duty = manual_led_duty;
         GPIO_mode(LED_PWM_PIN, AF);
         PWM_duty(LED_PWM_PIN, led_duty);

         apply_Ventilation_Outputs(manual_fan_on, manual_window_angle);
         break;

      case STATE_ALARM:
         // Soil-dry alarm: LED blinks with a 1 s period (toggle every 500 ms)
         GPIO_mode(LED_PWM_PIN, OUTPUT);
         led_toggle(LED_PWM_PIN);

         // Ventilation follows the selected mode
         if (selected_mode == STATE_AUTO) {
            control_Ventilation_Auto();
         } else {
            apply_Ventilation_Outputs(manual_fan_on, manual_window_angle);
         }
         break;
   }
}

void update_Lighting(uint32_t light){
   float raw_duty = 0.0f;

   if      (light <= DARK_TH)   raw_duty = 1.0f;
   else if (light >= BRIGHT_TH) raw_duty = 0.0f;
   else                         raw_duty = (float)(BRIGHT_TH - light) / (float)(BRIGHT_TH - DARK_TH);

   if (!led_active) {
      if (light <= (BRIGHT_TH - LIGHT_HYST)) {
         led_active = 1;
         led_on_ticks = 1;
         led_duty = (raw_duty < 0.05f) ? 0.05f : raw_duty;
      } else {
         led_duty = 0.0f;
      }
   } else {
      if (led_on_ticks < LED_MIN_ON_TICKS) {
         led_on_ticks++;
         led_duty = (raw_duty < 0.05f) ? 0.05f : raw_duty;
      } else if (light >= BRIGHT_TH) {
         led_active = 0;
         led_on_ticks = 0;
         led_duty = 0.0f;
      } else {
         led_duty = raw_duty;
      }
   }
}

void update_HumTemp(void){
   int status = DHT11_read();

   if (status == 0) {
      temp = DHT11_temperature();
      hum  = DHT11_humidity();
      dht_fail_streak = 0;
      dht_fault = 0;
   } else if (status != DHT11_ERR_TOOSOON) {        // TOOSOON (read too early) is not a sensor error, so it is not counted
      dht_err_count++;
      if (++dht_fail_streak >= 3) {
         dht_fault = 1;                            // consecutive failures -> set the forced-ventilation flag
      }
   }
}

// Convert Angle (0 ~ 180 deg) to Pulse Width (500 ~ 2500 us)
void control_window(int deg){
   if (deg > 180)      deg = 180;
   else if (deg < 0)   deg = 0;

   float pulse_us = 500.0f + ((float)deg / 180.0f) * 2000.0f;
   float duty = pulse_us / 20000.0f;
   PWM_duty(SERVO_PWM_PIN, duty);
}

void apply_Ventilation_Outputs(uint8_t fan_on, int angle){
   fan_is_on = fan_on ? 1 : 0;
   window_angle = angle;

   float targetPWM = fan_is_on ? 1.0f : 0.0f;
   float fan_duty = fabsf((float)MOTOR_DIR - targetPWM);
   PWM_duty(FAN_PWM_PIN, fan_duty);

   control_window(window_angle);
}

void control_Ventilation_Auto(void){
   static uint8_t auto_fan_state = 0;
   static int     auto_win_angle = WINDOW_CLOSE_DEG;

   // On DHT11 fault or T/H above the upper limit: fan ON + window OPEN
   if (dht_fault || temp > TEMP_HIGH || hum > HUM_HIGH) {
      auto_fan_state = 1;
      auto_win_angle = WINDOW_OPEN_DEG;
   } else if (temp <= TEMP_LOW && hum <= HUM_LOW) {
      auto_fan_state = 0;
      auto_win_angle = WINDOW_CLOSE_DEG;
   }

   apply_Ventilation_Outputs(auto_fan_state, auto_win_angle);
}

void update_Moisture(void) {
   uint32_t raw = moisture_ADCvalue;
   moist = MOIST_INVERT ? (4095 - raw) : raw;

   if (!alarm_dry) {
      if (moist < MOIST_DRY_TH) {
         if (++dry_cnt >= 10) {
            alarm_dry = 1;
            alarm_ack = 0;                         // a new alarm starts as not acknowledged
            dry_cnt = 0;
         }
      } else {
         dry_cnt = 0;
      }
   } else {
      if (moist > MOIST_RECOVER_TH) {
         if (++recover_cnt >= 10) {
            alarm_dry = 0;
            alarm_ack = 0;
            recover_cnt = 0;
         }
      } else {
         recover_cnt = 0;
      }
   }
}

void update_LCD(void){
   char line0[17];
   char line1[17];
   static uint8_t alarm_blink = 0;                 // toggled every 500 ms -> 1 s blink period

   int duty_pct = (int)(led_duty * 100.0f + 0.5f);

   if (current_state == STATE_ALARM) {
      // State = ALARM : line 1 "ALARM", line 2 "ADD WATER" blinking with a 1 s period
      snprintf(line0, sizeof(line0), alarm_ack ? "ALARM (ACK)     " : "ALARM           ");
      alarm_blink ^= 1;
      if (alarm_ack || alarm_blink) {
         snprintf(line1, sizeof(line1), "ADD WATER       ");
      } else {
         snprintf(line1, sizeof(line1), "                ");
      }
   } else {
      alarm_blink = 0;

      // State = AUTO or MANUAL
      if (current_state == STATE_MANUAL) {
         snprintf(line0, sizeof(line0), "MANUAL          ");
      } else {
         snprintf(line0, sizeof(line0), "AUTO            ");
      }

      if (fan_is_on) {
         // State + Ventilation : (00°, 00%, VENT)
         snprintf(line1, sizeof(line1), "%02lu\xDF, %02lu%%, VENT  ",
                  (unsigned long)temp, (unsigned long)hum);
      } else {
         // State = AUTO / MANUAL : (00°, 00%, LED 70%)
         if (duty_pct >= 100) {
            snprintf(line1, sizeof(line1), "%02lu\xDF,%02lu%%,LED 100%%",
                     (unsigned long)temp, (unsigned long)hum);
         } else {
            snprintf(line1, sizeof(line1), "%02lu\xDF,%02lu%%,LED %2d%% ",
                     (unsigned long)temp, (unsigned long)hum, duty_pct);
         }
      }
   }

   LCD_setCursor(0, 0);
   LCD_print(line0);
   LCD_setCursor(0, 1);
   LCD_print(line1);
}

void BT_send_str(const char *str){
   uint32_t len = 0;
   while (str[len] != '\0') len++;
   if (len > 0) {
      USART_write(USART6, (uint8_t *)str, len);
   }
}

// Handle one received Bluetooth character (single-char commands 'A','M','L','l','F','f','W','w','K')
void process_BT_char(uint8_t ch){
   switch (ch) {
      case 'A':
      case 'a':
         selected_mode = STATE_AUTO;
         auto_delay_ticks = 10;                    // AUTO selected via BT also waits 5 s before the light sensing starts
         led_active = 0;
         led_on_ticks = 0;
         break;                                    // MODE message is sent from notify_changes()

      case 'M':
      case 'm':
         selected_mode = STATE_MANUAL;
         auto_delay_ticks = 0;
         break;

      case 'L':
         if (selected_mode == STATE_MANUAL) {
            manual_led_duty = 1.0f;                // a lone 'L' turns the LED on at 100%
            BT_send_str("LED: ON\r\n");
         }
         break;

      case 'l':
         if (selected_mode == STATE_MANUAL) {
            manual_led_duty = 0.0f;                // a lone 'l' turns the LED off
            BT_send_str("LED: OFF\r\n");
         }
         break;

      case 'F':
         if (selected_mode == STATE_MANUAL) {
            manual_fan_on = 1;                     // 'F' turns the fan ON
            BT_send_str("FAN: ON\r\n");
         }
         break;

      case 'f':
         if (selected_mode == STATE_MANUAL) {
            manual_fan_on = 0;                     // 'f' turns the fan OFF
            BT_send_str("FAN: OFF\r\n");
         }
         break;

      case 'W':
         if (selected_mode == STATE_MANUAL) {
            manual_window_angle = WINDOW_OPEN_DEG; // 'W' opens the window
            BT_send_str("WIN: OPEN\r\n");
         }
         break;

      case 'w':
         if (selected_mode == STATE_MANUAL) {
            manual_window_angle = WINDOW_CLOSE_DEG;// 'w' closes the window
            BT_send_str("WIN: CLOSE\r\n");
         }
         break;

      case 'K':
      case 'k':                                    // acknowledge the alarm: only the repeated ADD WATER message stops
         if (alarm_dry && !alarm_ack) {
            alarm_ack = 1;
            BT_send_str("ALARM ACK: repeat stopped\r\n");
         } else {
            BT_send_str("ACK: no active alarm\r\n");
         }
         break;

      default:
         break;
   }
}

// Send a BT message whenever a condition changes (called from the main loop every 500 ms)
void notify_changes(void){
   static FSM_State_t prev_mode = STATE_MANUAL;
   static uint8_t prev_alarm = 0, prev_fault = 0, prev_fan = 0, prev_led_on = 0;

   if (selected_mode != prev_mode) {
      prev_mode = selected_mode;
      if (selected_mode == STATE_AUTO) BT_send_str("MODE: AUTO (light control starts in 5 s)\r\n");
      else                             BT_send_str("MODE: MANUAL\r\n");
   }

   if (alarm_dry != prev_alarm) {
      prev_alarm = alarm_dry;
      if (alarm_dry) { BT_send_str("ALARM: ADD WATER\r\n"); alarm_msg_cnt = 0; }
      else           { BT_send_str("ALARM CLEARED: soil moisture recovered\r\n"); }
   } else if (alarm_dry && !alarm_ack) {           // repeat until acknowledged (K)
      if (++alarm_msg_cnt >= ALARM_REPEAT_TICKS) {
         alarm_msg_cnt = 0;
         BT_send_str("ALARM: ADD WATER\r\n");
      }
   }

   if (dht_fault != prev_fault) {
      prev_fault = dht_fault;
      BT_send_str(dht_fault ? "DHT11 FAULT: invalid readings\r\n" : "DHT11 OK: valid reading again\r\n");
   }

   if (fan_is_on != prev_fan) {                    // automatic ventilation changes only (MANUAL is reported by the command reply)
      prev_fan = fan_is_on;
      if (selected_mode == STATE_AUTO)
         BT_send_str(fan_is_on ? "VENT: ON (fan on, window open)\r\n" : "VENT: OFF (fan off, window closed)\r\n");
   }

   uint8_t led_on = (led_duty > 0.0f);
   if (led_on != prev_led_on) {
      prev_led_on = led_on;
      if (current_state == STATE_AUTO)
         BT_send_str(led_on ? "LED: ON (AUTO)\r\n" : "LED: OFF (AUTO)\r\n");
   }
}

// TIM1 500ms Update Interrupt Handler
void TIM1_UP_TIM10_IRQHandler(void){
   if (is_UIF(TIM1)) {
      tick_500ms = 1;
      clear_UIF(TIM1);
   }
}

// Push-Button B1 (PC_13) External Interrupt Handler
void EXTI15_10_IRQHandler(void){
   static uint32_t last_btn_tick = 0;

   if (is_pending_EXTI(BUTTON_PIN)) {
      // 200 ms software debounce
      if ((msTicks - last_btn_tick) >= 200) {
         last_btn_tick = msTicks;

         if (selected_mode == STATE_MANUAL) {
            selected_mode = STATE_AUTO;
            auto_delay_ticks = 10;                 // 5 s delay starts when AUTO is entered with the push-button
            led_active = 0;
            led_on_ticks = 0;
         } else {
            selected_mode = STATE_MANUAL;
            auto_delay_ticks = 0;
         }
      }
      clear_pending_EXTI(BUTTON_PIN);
   }
}

// ADC interrupt : JDR1 (light), JDR2 (moisture)
void ADC_IRQHandler(void){
   if(is_ADC_JEOC()){
      led_ADCvalue = JADC_read(1);                 // JDR1 : light sensor
      moisture_ADCvalue = JADC_read(2);            // JDR2 : moisture sensor
      clear_ADC_JEOC();
   }
}

// USART2 RX interrupt (USB)
void USART2_IRQHandler(void){
   if(is_USART2_RXNE()){
      USART2_read();
   }
}

// USART6 RX interrupt (Bluetooth HC-06)
void USART6_IRQHandler(void){
   if(is_USART_RXNE(USART6)){
      uint8_t rx = USART_read(USART6);
      process_BT_char(rx);
   }
}