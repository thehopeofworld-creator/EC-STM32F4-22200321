/*----------------------------------------------------------------\
@ Embedded Controller by Young-Keun Kim - Handong Global University
Author           : Jisu Park
Created          : 05-03-2021
Modified         : 12-10-2026
Language/ver     : C++ in VS Code

Description      : Stepper Library.
/----------------------------------------------------------------*/


#include "ecStepper2.h"


// Stepper Motor variable
static PinName_t stepperPins[4];				// A, B, A', B'
static volatile uint32_t _steps;				// remaining steps (set to 0 by stepper_stop)
static volatile uint32_t _step_delay = 100;		
static const StateStepper_t *fsm;				// points at fsm_full or fsm_half (chosen in stepper_init)
static volatile uint32_t state = 0;				// current FSM state
static uint32_t _step_per_rev = STEP_PER_REV;       // steps per revolution (FULL: 2048, HALF: 4096)

// Full-stepping FSM.
// next[0] : next state for CW,  next[1] : next state for CCW
// Output: A B A' B'
static const StateStepper_t fsm_Full[4] = {	
 {{S1,S3},{HIGH,HIGH,LOW,LOW}},	
 {{S2,S0},{LOW,HIGH,HIGH,LOW}},
 {{S3,S1},{LOW,LOW,HIGH,HIGH}},
 {{S0,S2},{HIGH,LOW,LOW,HIGH}}
};

// Half-stepping FSM. Output: A B A' B'
static const StateStepper_t fsm_Half[8] = {  
	{{S1,S7},{1,0,0,0}},
	{{S2,S0},{1,1,0,0}},
	{{S3,S1},{0,1,0,0}},
	{{S4,S2},{0,1,1,0}},
	{{S5,S3},{0,0,1,0}},
	{{S6,S4},{0,0,1,1}},
	{{S7,S5},{0,0,0,1}},
	{{S0,S6},{1,0,0,1}}
};


// Initialization of Stepper Motor
void stepper_init(uint32_t mode, PinName_t *pinStepper){
	
	if (mode == HALF) {
		fsm = fsm_Half;
		_step_per_rev = STEP_PER_REV * 2;
	}

	else{
		fsm = fsm_Full;
		_step_per_rev = STEP_PER_REV;
	}

	//  GPIO Digital Out Initiation
	// For A, B, AN, BN

	for (int i=0; i<4; i++){
		stepperPins[i] = pinStepper[i];
		GPIO_init(stepperPins[i], OUTPUT);
		GPIO_ospeed(stepperPins[i], FAST_SPEED);
	}
	state = 0;
}

// Converts Motor [rpm] to step delay in [msec]
void stepper_speed (uint32_t speedRPM){      // rpm
		_step_delay = 60*1000/(_step_per_rev*speedRPM); 		//in [msec]	
}

// Run Stepper Motor for given steps and direction
void stepper_step(uint32_t steps, uint32_t direction){
	// Exit if stepper_init() not called 
	if (fsm == 0) return;
	_steps = steps;

	// run for step size: 
	for (uint32_t i = 0; i < _steps; i++){
		// Update Present State
		if (direction == DIR_CW){
			state = fsm[state].next[0];
		}
		else  {		//DIR_CCW
			state = fsm[state].next[1];
		}

		// Output of Present State
		stepper_out(state);
	
		// Delay for steppermotor speed
		delay_ms(_step_delay);
	}
}

// Stepper Motor Output for given state
// stepperPins[0]=A, [1]=B, [2]=A', [3]=B'  ->  out[0]~out[3]
void stepper_out(uint32_t state){

	for (int i = 0; i < 4; i++){
		GPIO_write(stepperPins[i],fsm[state].out[i]);
	}

}


// Stop Stepper Motor
void stepper_stop(void) {
    _steps = 0;    
    
	// All pins DigitalOut '0'
	for (int i = 0; i < 4; i++){
		GPIO_write(stepperPins[i],LOW);
	}
}



