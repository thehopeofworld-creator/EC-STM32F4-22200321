#include "ecDHT11.h"

#define DHT11_START_MS		20		// start pulse, datasheet minimum is 18 ms
#define DHT11_SAMPLE_US		40		// a 0 is high for ~27 us, a 1 for ~70 us
#define DHT11_EDGE_US		200		// the longest legal level is 80 us

static PinName_t _pin;
static int       _temperature = 0;
static int       _humidity    = 0;
static uint32_t  _lastRead    = 0;
static uint32_t  _everRead    = 0;


// ========================================================
// Timing that does not depend on interrupts
// ========================================================

// SysTick is a 24-bit DOWN counter reloading every 1 ms. It keeps counting
// with interrupts disabled, but msTicks does not, so SysTick_us() and
// delay_us() both jump backwards once per millisecond inside the data phase.
// These helpers read the counter directly instead. They are valid for
// intervals shorter than one reload, which every DHT11 pulse is.
static uint32_t us_since(uint32_t startVal){
	uint32_t period = SysTick->LOAD + 1UL;			// ticks in 1 ms
	uint32_t now    = SysTick->VAL;
	uint32_t ticks  = (startVal >= now) ? (startVal - now)
										: (startVal + period - now);
	return ticks / ((uint32_t)EC_SYSCLK / 1000000UL);
}

static void wait_us(uint32_t usec){
	uint32_t t0 = SysTick->VAL;
	while(us_since(t0) < usec){;}
}

// Waits for the line to reach level. Returns 0, or -1 if it never does.
static int wait_level(int level, uint32_t timeout_us){
	uint32_t t0 = SysTick->VAL;
	while(GPIO_read(_pin) != level){
		if(us_since(t0) > timeout_us)	return -1;
	}
	return 0;
}


// ========================================================
// Public
// ========================================================
void DHT11_init(PinName_t pinName){
	_pin = pinName;

	GPIO_init(_pin, INPUT);
	GPIO_pupd(_pin, PULL_UP);				// idle high between transactions

	_everRead = 0;
}


int DHT11_read(void){
	uint8_t  data[5] = {0, 0, 0, 0, 0};
	uint32_t primask;
	int      err = DHT11_OK;

	// The sensor needs 2 s between conversions. Asking sooner returns stale or
	// corrupt data, so say so rather than hand back a bad reading.
	if(_everRead && (msTicks - _lastRead) < DHT11_MIN_PERIOD_MS)
		return DHT11_ERR_TOOSOON;

// 1. Start pulse : hold the line low for at least 18 ms. This is a minimum,
//    so interrupts stay enabled and delay_ms() may be used.
	GPIO_init(_pin, OUTPUT);
	GPIO_otype(_pin, PUSH_PULL);
	GPIO_write(_pin, LOW);
	delay_ms(DHT11_START_MS);

// 2. Release the line. Everything from here is timed in microseconds, so the
//    interrupts go off. PRIMASK is saved and restored instead of enabling
//    blindly, in case the caller had already disabled them.
	primask = __get_PRIMASK();
	__disable_irq();

	GPIO_init(_pin, INPUT);
	GPIO_pupd(_pin, PULL_UP);

	// The sensor answers with 80 us low, then 80 us high, then the first bit
	if(wait_level(LOW,  DHT11_EDGE_US) ||
	   wait_level(HIGH, DHT11_EDGE_US) ||
	   wait_level(LOW,  DHT11_EDGE_US))
		err = DHT11_ERR_NO_REPLY;

// 3. 40 bits, MSB first. Every bit opens with 50 us low; the high that follows
//    is short for a 0 and long for a 1, so one sample taken 40 us after the
//    rising edge separates them without measuring any width.
	if(!err){
		for(uint32_t i = 0; i < 40; i++){
			if(wait_level(HIGH, DHT11_EDGE_US)){		// the 50 us low ends
				err = DHT11_ERR_TIMEOUT;
				break;
			}

			wait_us(DHT11_SAMPLE_US);
			data[i / 8] <<= 1;
			if(GPIO_read(_pin) == HIGH)	data[i / 8] |= 0x01;

			if(wait_level(LOW, DHT11_EDGE_US)){			// the bit ends
				err = DHT11_ERR_TIMEOUT;
				break;
			}
		}
	}

	__set_PRIMASK(primask);
	_lastRead = msTicks;
	_everRead = 1;

	if(err)	return err;

// 4. Checksum : the low byte of the sum of the first four
	if((uint8_t)(data[0] + data[1] + data[2] + data[3]) != data[4])
		return DHT11_ERR_CHECKSUM;

	_humidity    = data[0];				// data[1] is always 0 on a DHT11
	_temperature = data[2];				// data[3] is always 0 on a DHT11
	return DHT11_OK;
}


int DHT11_temperature(void){	return _temperature;	}
int DHT11_humidity(void){		return _humidity;		}
