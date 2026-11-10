#ifndef __EC_DHT11_H
#define __EC_DHT11_H

#include "stm32f411xe.h"
#include "ecGPIO2.h"
#include "ecRCC2.h"
#include "ecSysTick2.h"

// DHT11 single-wire temperature / humidity sensor.
// The data line is open drain and needs an external pull-up, which the usual
// breakout module carries. Use any plain GPIO pin.
//
// A transaction is about 22 ms, but only the last 3 to 5 ms carry data, and in
// that part a 0 and a 1 differ by roughly 43 us of pulse width. DHT11_read()
// therefore disables interrupts for the data phase only; the 18 ms start pulse
// is a minimum, so an interrupt that lengthens it does no harm.

// Return codes of DHT11_read()
#define DHT11_OK			 0
#define DHT11_ERR_TOOSOON	-1		// called again before DHT11_MIN_PERIOD_MS
#define DHT11_ERR_NO_REPLY	-2		// the sensor never answered the start pulse
#define DHT11_ERR_TIMEOUT	-3		// a bit edge did not arrive in time
#define DHT11_ERR_CHECKSUM	-4		// 40 bits arrived but the checksum is wrong

#define DHT11_MIN_PERIOD_MS	2000	// datasheet minimum between two reads

void DHT11_init(PinName_t pinName);

// Runs one 40-bit transaction. Returns DHT11_OK or one of the codes above.
int  DHT11_read(void);

// Values from the last successful read. A failed read leaves them unchanged.
int  DHT11_temperature(void);		// degC
int  DHT11_humidity(void);			// %RH

#endif
