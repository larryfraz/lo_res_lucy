#include <msp430g2553.h>
#include "uart.h"
#define 	CLOCKHZ		16000000 / 8
#define		nNotes		24

//Lucy Lo-Res PWM Audio Synthesizer
// By Larry Frazier, based on code by NatureTM, 43Oh group

//Creative Commons, Attribution, Derivatives OK, Non-Commercial. If You want one boxed up stand alone or Eurorack, see
//www.perspective-sound.com for contact

// Poly not implemented, but this is how
// many notes can be hit at the same time
// without forgetting the first
#define MAX_POLYPHONY			5
#define MIDI_CHANNEL			0	// MIDI is 1-indexed so setting this to 0 is midi channel 1
#define USI_COUNTER_LOAD		16

// Synth variables
unsigned int periodSetting = 61156;
unsigned int osc2Period;
unsigned int osc2Delta = ((61156)>>12);
unsigned int osc2Diff;
//const unsigned int synthNotes[nNotes] = {61156, 57723, 54484, 51424, 48539, 45817, 43245, 40816, 38527, 36364, 34322, 32396};
 //const unsigned int synthNotes[nNotes] = {61156, 64428, 65233, 67951, 68801, 72481, 73387, 76445, 77401, 81541, 82561, 86000, 86977, 91734, 96642, 97850, 101927, 103201, 108722, 110081, 101927, 116101, 116101, 116101};
const unsigned int synthNotes[nNotes] = {61156, 60410,57334, 55040, 54361, 51600, 50963, 48925, 48321, 45867, 45301, 43489, 43000, 40771, 38700, 38223, 36694, 36241, 34400, 33976,33694, 32214, 32214, 30578};

		// MIDI Variables

char currentState = 1;
char bitStateCount = 0;
char nextBitFinal = 0;
char opcode;
 char midiByte;


char rawMidiByte;
char newByte = 0;
char newVelocity;
char newNote;
char notes[MAX_POLYPHONY] = {0};
char velocities[MAX_POLYPHONY] = {0};
char controllerNumber;
char controllerValue;
int noteIndex = 0;
unsigned int USIData = 0;
char newStat =0;
char nextByte;
char detuneAmt = 2;
char osc2Vol=2;
char osc2Pass =0;

char osc3Vol= 63;
char osc3Pass=0;
char osc3Delta=0;
unsigned int osc3Period;
char intervals[8] = {0,2,3,4,5,7,9,10};
char osc3Int = 5;
char osc3Phase =64;
char osc3LastPhase;
char osc3In= 64;
char attack=64;
int attVal;
int i = 0;
unsigned int out;
char lfo1Speed;
char lfo1Src;
char lfo1Val;
char lfo1Shape;
char lfo1Counter;
int vOsc2Detune =2;
char lfoOut;
int j;
int lfoCounter;

enum
{
	saw1,
	saw2,
	tri1,
	tri2
};


void updateSynth(char on);
char getNextByte();
void updateState();
void noteOn();
void noteOff();
void controlChange();
void updateControls();
void shiftLeft(char index);
char getNoteIndex(char note);
unsigned int getPWMValue();
char getAttValue();
unsigned int getOsc3Value();
void setOsc2Values();
void setOsc3Values();
char lfo( char shape, char value, char speed);
void setLfo();

void uart_rx_isr(unsigned char c) {
	uart_putc(c);

}
void main(void)
{
	volatile unsigned long i;

 	DCOCTL = CALDCO_16MHZ;
	BCSCTL1 = CALBC1_16MHZ;
	BCSCTL2 |= DIVS_2;						// SMCLK = MCLK / 4 = 4MHZ

	WDTCTL = WDTPW + WDTHOLD;               // Stop WDT
	_BIS_SR(GIE);							// enable interrupts
	P1DIR |=  BIT0 + BIT6;           	//  output
	P1OUT = 0;

	TA1CCTL1 = OUTMOD_7;						// CCR1 reset/set
	P2DIR |=  BIT1 + BIT2;
	P2OUT = 0;							 //output 2.2
	P2REN = 0xFF;						///ren all pins on P2---new change!!!!
	TA1CTL = TASSEL_2 + MC_1 + ID_3;         // SMCLK, up mode, /8 = 500KHz
	TA1CCTL0 |=  CCIE;
	TA1CCR0 = 1;

	 uart_init();

		// register ISR called when data was received
	    uart_set_rx_isr_ptr(uart_rx_isr);
	    __bis_SR_register(GIE);
  while(1){

	  opcode = getNextByte();

  	if(opcode == (0x90 | MIDI_CHANNEL))

  	noteOn();
  	else if(opcode == (0x80 | MIDI_CHANNEL))

		noteOff();

  	else if(opcode == (0xB0 | MIDI_CHANNEL))

  		controlChange();


  }
}


void noteOn(){
	do{
		newNote = getNextByte();
		if(newNote & 0x80){
			newStat = 1;
			break;
		}
		newVelocity = getNextByte();
		if(newVelocity & 0x80){
			newStat = 1;
			break;
		}
		updateState();
	}while(1);
}

void noteOff(){
	do{
		newNote = getNextByte();
		if(newNote & 0x80){
			newStat = 1;
			break;
		}
		newVelocity = 0;
		updateState();
	}while(1);
}

void controlChange(){
	do{
		controllerNumber = getNextByte();
		if(controllerNumber & 0x80){
			newStat = 1;
			break;
		}
		controllerValue = getNextByte();
		if(controllerValue & 0x80){
			newStat = 1;
			break;
		}
		updateControls();
	}while(1);
}

void updateControls(){
	if(controllerNumber ==1){// controller 1 == mod wheel ==osc2 detune
		detuneAmt = (controllerValue>>4)+1;//scaled input
		setOsc2Values();


	}
	if(controllerNumber == 7){//controller 7 == slider ==osc2 volume
		osc2Vol = controllerValue>>1;
		if (osc2Vol == 0)
			osc2Vol = 1;
	}

	if(controllerNumber == 11)
	{

		osc3Vol = (controllerValue>>4);// scaled osc3 volume
		if (osc3Vol == 0)
					osc3Vol = 1;}
	if(controllerNumber == 10)
		{

			osc3In = (controllerValue>>2);// this is not phase. equal number of Hz detune, I tried to fix this but it didn't sound as interesting
			if (osc3In == 0)
						osc3In = 1;
		if (osc3In > osc3LastPhase)
		{
			osc3Phase = periodSetting/(osc3In - osc3LastPhase);
		}else
		{ periodSetting/(osc3Phase = osc3LastPhase -osc3In);

		}
		}
	if(controllerNumber == 12)
	{

		osc3Int = intervals[controllerValue>>3];// choose interval to detune
		setOsc3Values();
	}
	if(controllerNumber == 13)
		{

			attack = (controllerValue>>4);// scaled attack value
			if (attack == 0)
						attack = 1;}
	if(controllerNumber == 91)
		{

			lfo1Shape = (controllerValue>>5);// index for lfo shape
			//if (lfo1Shape == 0)
						//lfo1Shape = 1;
			}
	if(controllerNumber == 93)
		{

			lfo1Speed = (controllerValue>>2   );// nothing special about these controller numbers, its just how my mk249c is laid out
			if (lfo1Speed == 0)
						lfo1Speed = 1;}
}
void setOsc2Values()
{osc2Diff = (periodSetting>>vOsc2Detune);
}

unsigned int getPWMValue(){
	unsigned int outVal;

	// shift left detuneAmt value bits => this is how much longer the period of osc2 is than osc1
	if(osc2Pass == 0)
	{
		osc2Delta =osc2Delta+(osc2Diff); // keep doubling the delta till it laps

	if(osc2Delta >= periodSetting)
	{
		osc2Pass = 1;

	}
	}
	else{
		osc2Delta =osc2Delta-(osc2Diff); // keep doubling the delta till it laps

			if(osc2Delta <= osc2Diff)
			{
				osc2Pass = 0;


			}
	}
	outVal =  osc2Delta;
	return outVal;
}
void setOsc3Values()
{
	 osc3Period = (synthNotes[notes[noteIndex+osc3Int] % nNotes]>> ((notes[noteIndex+osc3Int] / nNotes)-1)); //octave down
}
unsigned int getOsc3Value(){
	unsigned int outVal;


	// shift left detuneAmt value bits => this is how much longer the period of osc2 is than osc1
	if(osc3Pass == 0)
	{
		osc3Delta = osc3Phase +osc3Delta+(osc3Period-periodSetting); // keep doubling the delta till it laps

	if(osc3Delta >= periodSetting )
	{
		osc3Pass = 1;

	}
	}
	else{
		osc3Delta =(osc3Phase +osc3Period-periodSetting); // keep doubling the delta till it laps

			if(osc3Delta <= periodSetting )
			{
				osc3Pass = 0;


			}
	}
	outVal =  osc3Delta;
	return outVal;
}
char getAttValue()
{
	unsigned int time = 500000/attack;
	unsigned int cycles =(time/periodSetting);


		if ((attVal>1)&&(cycles>0))
		{
			if(out ==0)
			{
	attVal=attVal>>1;
		out =cycles;
			}else
			out-- ;
		}
		else
		attVal = 1;




return attVal;
}
void setLfo()
{
vOsc2Detune = lfo(lfo1Shape, detuneAmt, lfo1Speed);
setOsc2Values();
}

char lfo( char shape, char value, char speed)
{

	lfoCounter = lfo1Counter;
	char firstHalf =1;

	unsigned int time = 500000/speed;
		unsigned int cycles =(time/periodSetting);

	switch(shape)
	{
	case saw1:
	case saw2:
		{if ((lfoCounter>=1)&&(cycles>0))
				{
					if(j ==0)
					{
						lfoCounter=lfoCounter-1;
						j =cycles;
					}
					else
						j--;
				}
		else
			lfoCounter =1;

			if(shape == saw2)
				lfoOut = (value *(127 - lfoCounter))/127;//flip the saw wave
			else
					lfoOut = (value *lfoCounter)/127;
		}
		break;
	case tri1:
		case tri2:
			{if ((lfoCounter>=1)&&(cycles>0))
					{
						if(j ==0)
						{
							if(firstHalf)
								lfoCounter=lfoCounter-2;
							else
								lfoCounter=lfoCounter+2;
							j =cycles;
						}
						else
							j--;
						if(lfoCounter<=1)
							firstHalf = 0;
					}
			else
				lfoCounter =1;

				if(shape == tri2)
					lfoOut = (value *(127 - lfoCounter))/127;//flip the triangle wave
				else
						lfoOut = (value *lfoCounter)/127;
			}
			break;
	}
	return lfoOut;

}
char getNextByte(){

	if(newStat)
		{
		newStat = 0;
			return nextByte;
		}
	else
	{
		do{

			P1OUT &= ~BIT0;

		while(!newByte){
		}

		nextByte =  uart_getByte();
}while((nextByte == 0xFE) || (nextByte == 0xFF));
		P1OUT |= BIT0;


	return nextByte;
}
}


#pragma vector=TIMER1_A0_VECTOR
__interrupt void Timer1_A0 (void){

	setLfo();
	TA1CCR0 = periodSetting;
	TA1CCR1 = ((getPWMValue()/osc2Vol +periodSetting)+(getOsc3Value()/osc3Vol))/(4*getAttValue());//(CCR0 >> 1);

}

void updateState(){

	if(noteIndex == 0 && newVelocity != 0){
		notes[0] = newNote;
		velocities[0] = newVelocity;
		updateSynth(1);
		noteIndex++;
		attVal = 127;
					out =0;
					lfoOut =0xFF;
					lfo1Counter =127;
	}
	else{
		if(newVelocity != 0){
			// add a new note when the poly cache is full, replacing the oldest
			if(MAX_POLYPHONY == noteIndex){
				shiftLeft(0);
				notes[MAX_POLYPHONY - 1] = newNote;
				velocities[MAX_POLYPHONY - 1] = newVelocity;
				updateSynth(1);
			}
			// add a new note
			else{
				notes[noteIndex] = newNote;
				velocities[noteIndex] = newVelocity;
				updateSynth(1);
				noteIndex++;
			}
			attVal = 127;//init values for new note sounding
			out =0;
			lfoOut =0xFF;
			lfo1Counter =127;
		}
		else if(getNoteIndex(newNote) < MAX_POLYPHONY){
			shiftLeft(getNoteIndex(newNote));
			noteIndex -= 2;
			if(noteIndex >= 0){
				updateSynth(1);
				noteIndex++;
			}
			else{
				updateSynth(0);
				noteIndex = 0;
			}
		}
	}
}

void shiftLeft(char index){
	int i;
	for(i = index; i < MAX_POLYPHONY - 1; i++){
		notes[i] = notes[i + 1];
		velocities[i] = velocities[i + 1];
	}
}

char getNoteIndex(char note){
	int i;
	for(i = 0; i < MAX_POLYPHONY; i++)
		if(notes[i] == note)
			return i;
	return MAX_POLYPHONY + 1;
}

void updateSynth(char on){
	if(on){
		P2SEL |= BIT2;
		P1OUT ^= BIT6;

	}
	else{
		P2SEL &= ~BIT2;
		P1OUT ^=  BIT6;

	}
		if(noteIndex >= 0 && noteIndex < MAX_POLYPHONY){
			periodSetting = (synthNotes[notes[noteIndex] % 24] >> (notes[noteIndex] / 24));


		setOsc2Values();//set more vals here!?!
		setOsc3Values();


		}
}


