/*----------------------------------------------------------------\
@ Embedded Controller by Young-Keun Kim - Handong Global University
Author           : JeYun Oh
Created          : 05-03-2021
Modified         : 02-11-2026
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
#define LIGHT_INVERT            1      // 1: 어두울수록 ADC 커짐 -> 반전하여 클수록 밝음
#define DARK_TH                 300    // light <= DARK_TH : duty 100%
#define BRIGHT_TH               3800   // light >= BRIGHT_TH : duty 0%
#define LIGHT_HYST              200    // 경계값 깜빡임 방지 히스테리시스
#define LED_MIN_ON_TICKS        4      // 최소 점등 시간 (500ms * 4 = 2초)

#define MOTOR_DIR               0      // 0 or 1 (automatically handled by compensation formula)
#define PWM_PERIOD_US           1000   // 1 msec = 1 kHz

// === Moisture 임계값 ===
#define MOIST_INVERT            1      // 1: 물에 닿을수록 ADC 작아짐 -> 반전하여 클수록 습함
#define MOIST_DRY_TH            1000   // 이 값 미만으로 5초 유지 시 DRY 알람 발생
#define MOIST_RECOVER_TH        2500   // 이 값 초과로 5초 유지 시 DRY 알람 해제

// === Window Angles ===
#define WINDOW_OPEN_DEG         30
#define WINDOW_CLOSE_DEG        90

// === FSM States ===
typedef enum {
   STATE_AUTO   = 0,
   STATE_MANUAL = 1,
   STATE_ALARM  = 2
} FSM_State_t;

volatile FSM_State_t selected_mode = STATE_MANUAL; // 전원 인가 시 기본 모드는 MANUAL
FSM_State_t current_state = STATE_MANUAL;

volatile uint8_t auto_delay_ticks = 5000;             // 버튼으로 AUTO 진입 시 5초(10틱) 대기
volatile uint8_t tick_500ms = 0;                   // TIM1 500ms 주기 플래그

// Manual Mode Setpoints (전원 인가 시 LED OFF, Fan OFF, Window CLOSED)
volatile float   manual_led_duty     = 0.0f;
volatile uint8_t manual_fan_on       = 0;
volatile int     manual_window_angle = WINDOW_CLOSE_DEG;

// LED
volatile uint32_t led_ADCvalue = 0;
float led_duty = 0.0f;
uint8_t led_active = 0;
uint8_t led_on_ticks = 0;

// Ventilation
static volatile int window_direction = 1;
static volatile int window_angle = WINDOW_CLOSE_DEG;
uint8_t fan_is_on = 0;

uint32_t temp = 0;
uint32_t hum  = 0;
uint32_t dht_err_count = 0;
uint8_t dht_fail_streak = 0;
uint8_t dht_fault = 0;              // 1: DHT11 연속 오류 -> AUTO 모드에서 강제 환기

// Moisture
volatile uint32_t moisture_ADCvalue = 0;
uint32_t moist = 0;
uint8_t alarm_dry = 0;              // 0: 정상, 1: ALARM(DRY) 활성
uint8_t dry_cnt = 0;                // 500ms 루프 기준 10회 = 5초
uint8_t recover_cnt = 0;

// Bluetooth Command Parser State
volatile char bt_cmd_type = 0;
volatile int  bt_cmd_val  = -1;

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

void setup(void){
   RCC_PLL_init();                                 // System Clock = 84MHz
   SysTick_init();                                 // Systick init
   USART2_init();                                  // USB Serial Debug (9600 baud)

   // Bluetooth USART6 init (PA_11 = TX, PA_12 = RX, 9600 baud)
   USART_init(USART6, BT_TX_PIN, BT_RX_PIN, BAUD_9600);
   NVIC_SetPriority(USART6_IRQn, 1);               // ecUART2.c는 USART1/2 NVIC만 켜므로 직접 활성화
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

   // JADC 2채널 시퀀스 설정 (JDR1: 조도 PA_0, JDR2: 수분 PA_1, 125ms 주기)
   PinName_t jadc_pins[2] = {LIGHT_SENSOR_PIN, MOISTURE_SENSOR_PIN};
   JADC_init(LIGHT_SENSOR_PIN);
   JADC_init(MOISTURE_SENSOR_PIN);
   JADC_sequence(jadc_pins, 2);
   JADC_trigger_init(TIM5, 125, ADC_RISE);

   // TIM1 500ms 제어 루프 인터럽트 시작
   TIM_UI_init(TIM1, 500);
}

int main(void){
   setup();
   uint8_t loop_cnt = 0;

   // 시작 직후 초기 온습도 1회 측정
   update_HumTemp();

   // Infinite Loop ----------------------------------------------------------
   while(1){
      if (tick_500ms) {
         tick_500ms = 0;

         uint32_t LED   = led_ADCvalue;
         uint32_t light = LIGHT_INVERT ? (4095 - LED) : LED;

         // 1. 500 ms * 4 = 2초마다 1회 DHT11 측정
         if (++loop_cnt >= 4) {
            loop_cnt = 0;
            update_HumTemp();
         }

         // 2. 토양 수분 알람 상태 갱신 (5초 유지 판정)
         update_Moisture();

         // 3. FSM 상태 전이 및 액추에이터 출력 제어
         control_FSM(light);

         // 4. I2C LCD 상태 업데이트
         update_LCD();

         // 5. 시리얼 디버깅 출력
         const char *st_str = (current_state == STATE_ALARM)  ? "ALARM " :
                              (current_state == STATE_AUTO)   ? "AUTO  " : "MANUAL";
         printf("[%s] light=%4lu LED_duty=%3d%% hum=%2lu%% temp=%2luC moist=%4lu fan=%d win=%d delay=%d\r\n",
                st_str, (unsigned long)light, (int)(led_duty * 100.0f + 0.5f),
                (unsigned long)hum, (unsigned long)temp, (unsigned long)moist,
                fan_is_on, window_angle, auto_delay_ticks);
      }
   }
}

void control_FSM(uint32_t light){
   // 1. 상태 결정: 토양 건조 알람이 최우선, 해제 시 이전 선택 모드(AUTO/MANUAL)로 복귀
   if (alarm_dry) {
      current_state = STATE_ALARM;
   } else {
      current_state = selected_mode;
   }

   // 2. 현재 상태별 동작 수행
   switch (current_state) {
      case STATE_AUTO:

         // 푸시버튼으로 AUTO 진입 시 5초(10틱) 지연 후 조도 감지 시작
         if (auto_delay_ticks > 0) {
            auto_delay_ticks--;
            led_active = 0;
            led_on_ticks = 0;
            led_duty = 0.0f;
         } else {
            update_Lighting(light);
         }

         // 자동 환기 (온습도 히스테리시스 + DHT11 오류 시 강제 환기)
         control_Ventilation_Auto();

         // LED PWM 출력
         GPIO_mode(LED_PWM_PIN, AF);
         PWM_duty(LED_PWM_PIN, led_duty);
         break;

      case STATE_MANUAL:
         auto_delay_ticks = 0;
         led_active = 0;
         led_on_ticks = 0;

         // 블루투스 수동 명령값 적용
         led_duty = manual_led_duty;
         GPIO_mode(LED_PWM_PIN, AF);
         PWM_duty(LED_PWM_PIN, led_duty);

         apply_Ventilation_Outputs(manual_fan_on, manual_window_angle);
         break;

      case STATE_ALARM:
         // 토양 건조 경보: LED 1초 주기(500ms마다 반전) 점멸
         GPIO_mode(LED_PWM_PIN, OUTPUT);
         led_toggle(LED_PWM_PIN);

         // 환기는 선택된 모드 기준 유지
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
   } else {
      dht_err_count++;
      if (++dht_fail_streak >= 2) {
         dht_fault = 1;                            // 연속 실패 시 강제 환기 플래그 활성화
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
   window_direction = (angle <= 45) ? -1 : 1;

   float targetPWM = fan_is_on ? 1.0f : 0.0f;
   float fan_duty = fabsf((float)MOTOR_DIR - targetPWM);
   PWM_duty(FAN_PWM_PIN, fan_duty);

   control_window(window_angle);
}

void control_Ventilation_Auto(void){
   static uint8_t auto_fan_state = 0;
   static int     auto_win_angle = WINDOW_CLOSE_DEG;

   // DHT11 오류 상태이거나 온습도 상한 초과 시 팬 ON + 창문 OPEN
   if (dht_fault || temp > 26 || hum > 70) {
      auto_fan_state = 1;
      auto_win_angle = WINDOW_OPEN_DEG;
   } else if (temp <= 28 && hum < 60) {
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
            dry_cnt = 0;
         }
      } else {
         dry_cnt = 0;
      }
   } else {
      if (moist > MOIST_RECOVER_TH) {
         if (++recover_cnt >= 10) {
            alarm_dry = 0;
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
   static uint8_t alarm_blink = 0;                 // 500ms마다 반전 -> 1초 주기 점멸

   int duty_pct = (int)(led_duty * 100.0f + 0.5f);

   if (current_state == STATE_ALARM) {
      // State = ALARM : 1행 "ALARM", 2행 "ADD WATER" 1초 주기 점멸
      snprintf(line0, sizeof(line0), "ALARM           ");
      alarm_blink ^= 1;
      if (alarm_blink) {
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

// Bluetooth 수신 문자 처리 (단일 문자 명령 'A','M','L','l','F','f','W','w' 및 숫자 명령 'L70','W30' 지원)
void process_BT_char(uint8_t ch){
   // 줄바꿈(\r, \n) 수신 시 누적된 숫자 인자 확정 (예: "L70\n", "W30\n")
   if (ch == '\r' || ch == '\n') {
      if (bt_cmd_type != 0 && bt_cmd_val >= 0) {
         if (selected_mode == STATE_MANUAL) {
            if (bt_cmd_type == 'L' || bt_cmd_type == 'l') {
               int pct = (bt_cmd_val > 100) ? 100 : bt_cmd_val;
               manual_led_duty = (float)pct / 100.0f;
            } else if (bt_cmd_type == 'F' || bt_cmd_type == 'f') {
               manual_fan_on = (bt_cmd_val > 0) ? 1 : 0;
            } else if (bt_cmd_type == 'W' || bt_cmd_type == 'w') {
               int ang = (bt_cmd_val > 180) ? 180 : bt_cmd_val;
               manual_window_angle = ang;
            }
         }
      }
      bt_cmd_type = 0;
      bt_cmd_val  = -1;
      return;
   }

   // 숫자 입력 중이면 값 누적
   if (ch >= '0' && ch <= '9') {
      if (bt_cmd_type != 0) {
         if (bt_cmd_val < 0) bt_cmd_val = 0;
         bt_cmd_val = bt_cmd_val * 10 + (ch - '0');
      }
      return;
   }

   // 새 명령 문자 시작
   bt_cmd_type = (char)ch;
   bt_cmd_val  = -1;

   switch (ch) { 
      case 'A':
      case 'a':
         selected_mode = STATE_AUTO;
         auto_delay_ticks = 0;                     // BT로 AUTO 선택 시 즉시 시작
         led_active = 0;
         led_on_ticks = 0;
         BT_send_str("MODE: AUTO\r\n");
         break;

      case 'M':
      case 'm':
         selected_mode = STATE_MANUAL;
         auto_delay_ticks = 0;
         BT_send_str("MODE: MANUAL\r\n");
         break;

      case 'L':
         if (selected_mode == STATE_MANUAL) {
            manual_led_duty = 1.0f;                // 'L' 단독 전송 시 LED 100% ON
            BT_send_str("LED: ON\r\n");
         }
         break;

      case 'l':
         if (selected_mode == STATE_MANUAL) {
            manual_led_duty = 0.0f;                // 'l' 단독 전송 시 LED OFF
            BT_send_str("LED: OFF\r\n");
         }
         break;

      case 'F':
         if (selected_mode == STATE_MANUAL) {
            manual_fan_on = 1;                     // 'F' 전송 시 Fan ON
            BT_send_str("FAN: ON\r\n");
         }
         break;

      case 'f':
         if (selected_mode == STATE_MANUAL) {
            manual_fan_on = 0;                     // 'f' 전송 시 Fan OFF
            BT_send_str("FAN: OFF\r\n");
         }
         break;

      case 'W':
      case 'O':
      case 'o':
         if (selected_mode == STATE_MANUAL) {
            manual_window_angle = WINDOW_OPEN_DEG; // 'W' 또는 'O' 전송 시 창문 OPEN
            BT_send_str("WIN: OPEN\r\n");
         }
         break;

      case 'w':
      case 'C':
      case 'c':
         if (selected_mode == STATE_MANUAL) {
            manual_window_angle = WINDOW_CLOSE_DEG;// 'w' 또는 'C' 전송 시 창문 CLOSED
            BT_send_str("WIN: CLOSE\r\n");
         }
         break;

      default:
         break;
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
      // 200ms 소프트웨어 디바운싱
      if ((msTicks - last_btn_tick) >= 200) {
         last_btn_tick = msTicks;

         if (selected_mode == STATE_MANUAL) {
            selected_mode = STATE_AUTO;
            auto_delay_ticks = 10;                 // 푸시버튼으로 AUTO 진입 시 5초 지연 시작
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