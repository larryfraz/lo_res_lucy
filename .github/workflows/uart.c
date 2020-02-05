******************************************************************************
 * Hardware UART example for MSP430.
 *
 * Stefan Wendler
 * sw@kaltpost.de
 * http://gpio.kaltpost.de
 * modified by Larry Frazier (buffered input) larryfraz@yahoo.com
 * perspectivesound,blogspot.com
 ******************************************************************************/

#include <msp430.h>
#include "msp430g2553.h"
//#include <legacymsp430.h>

#include "uart.h"

/**
 * Receive Data (RXD) at P1.1
 */
#define RXD		BIT1

/**
 * Transmit Data (TXD) at P1.2
 */
#define TXD		BIT2

/**
 * Callback handler for receive
 */
extern char newByte;
extern char midiByte;
 char buffer[256];
char ptr = 0;
char bufOutPtr =0;

void (*uart_rx_isr_ptr)(unsigned char c);

void uart_init(void)
{
	uart_set_rx_isr_ptr(0L);

	P1SEL  = RXD ;
  	P1SEL2 = RXD ;
  	UCA0CTL1 |= UCSSEL_2;                     // SMCLK==4mhz
  	UCA0BR0 = 8;                            //  DIVIDES SMCLK  = 31250  = MIDI SPEC w/UCOS16 oversample
  	UCA0BR1 = 0;                              //
  	UCA0MCTL |= UCOS16;                        // 16x oversample


  			UCA0CTL1 &= ~UCSWRST ;                     // Initialize USCI state machine
  					IE2 |= UCA0RXIE;                          // Enable USCI_A0 RX interrupt
}

void uart_set_rx_isr_ptr(void (*isr_ptr)(unsigned char c)) 
{
	uart_rx_isr_ptr = isr_ptr;
}

unsigned char uart_getc()
{
    while (!(IFG2&UCA0RXIFG));                // USCI_A0 RX buffer ready?

	return UCA0RXBUF;
}

void uart_putc(unsigned char c)
{
	while (!(IFG2&UCA0TXIFG));              // USCI_A0 TX buffer ready?
  	UCA0TXBUF = c;                    		// TX
}

unsigned char uart_getByte()
{

	midiByte = buffer[bufOutPtr++];
	if (bufOutPtr>255)
		bufOutPtr=0;
	if (bufOutPtr==ptr)
			newByte = 0;


	return midiByte;

}


void uart_puts(const char *str)
{
     while(*str) uart_putc(*str++);
}
#pragma vector=USCIAB0RX_VECTOR
__interrupt void USCI0RX_ISR(void)   {
//interrupt(USCIAB0RX_VECTOR) USCI0RX_ISR(void)

	if(uart_rx_isr_ptr != 0L) {
		(uart_rx_isr_ptr)(UCA0RXBUF);}

		//if((UCA0RXBUF !=  0xFF )&& (UCA0RXBUF != 0xFE))
		//{ midi messages to not add
		buffer[ptr++] = UCA0RXBUF;
		if(ptr > 255)
					ptr = 0;
		newByte = 1;
		//}


}
