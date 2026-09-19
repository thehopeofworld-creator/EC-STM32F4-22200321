#include <stdio.h>
#include <stdint.h>

void dec2bin(unsigned int n);

void main() {
    
	uint8_t vals=0;
	
	// Assume 8 LEDs are connected to Digital Out pins of Port A (PA)
	// LED0 at PA[0] to LED7 at PA[7]
	uint8_t PA;
	
	// Initial Values of PA
	PA = 0b00001111;			
	printf("\n Initial PA: ");
	dec2bin(PA);
	
	
	// Exercise_1-1: Turning ON LED4 at PA[4] 
	PA |= (1<<4); 	    // turn ON LED4 
	printf("\n ex1-1: ");
	dec2bin(PA);
	
	//Exercise_1-2:  Read the bit at PA[4], 4th from LSB
	vals = (PA>>4)&1;	// read bit of PA[4]
	printf("\n ex1-2: %d\n", vals);
	
	// Exercise_2-1: Turn off LED4 at PA[4]
	PA &= ~(1<<4);    	// turn off LED4  
	printf("\n ex2-1: ");
	dec2bin(PA);
	
	//Exercise_2-2:  Read the bit at PA[4], 4th from LSB
	vals = (PA>>4)&1;	// read bit of PA[4]
	printf("\n ex2-2: %d\n", vals);
	
	// Exercise_3-1: Turning ON LED5 and LED4,  at PA[5:4] 
	PA |=  (3<<4);	// turn ON LED5 and LED4
	printf("\n ex3-1: ");
	dec2bin(PA);
	
	//Exercise_3-2:  Read bits LED5 and LED4,  at PA[5:4] 
	vals =  (PA>>4)&(0x03);	// read bits at PA[5:4] 
	printf("\n ex3-2: %d\n", vals);
	
    //Exercise_4-1:  SET  PA[0] and PA[7] 
	PA |= 0b10000001; 
	printf("\n ex4-1: ");
	dec2bin(PA);
	
	//Exercise_4-2:  CLEAR  PA[0] and PA[7] 
	PA &= ~(129<<0);
	printf("\n ex4-2: ");
	dec2bin(PA);
	
	
	//Exercise_5-1:  Toggle LED7 PA[7]
	printf("\n");    
	PA ^=  1<<7;
	printf("\n ex5-1: ");
	dec2bin(PA);
	
	printf("\n");    
	
}

void dec2bin(unsigned int n) {
	unsigned int a = 0x80;
	for (int i = 0; i < 8; i++) {
		if ((a & n) == a)
			printf("1");
		else
			printf("0");
		a = a >> 1;
	}
}