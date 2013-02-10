/*
 * SLLogicLogger - Simple logic analyser for Stellaris Launchpad
 *
 * SLLogicLogger supports the SUMP protocol over UART. The UART is mapped
 * through the debug/flash controller on the Stellaris Launchpad to a virtual
 * com port. It samples signals on PORTB with samplerate 10 Mhz, buffersize
 * 16kByte, sampling starts at any change on PORTB[0..7].
 * PB0 and PB1 are limited to 3.6 V! All other pins are 5 V tolerant.
 *
 * The idea for this simple analyser comes from http://jjmz.free.fr/?p=148
 *
 * Description of the SUMP protocol:
 * - http://www.sump.org/projects/analyzer/protocol/
 * - http://dangerousprototypes.com/docs/The_Logic_Sniffer%27s_extended_SUMP_protocol
 *
 * A multiplatform client which supports the SUMP protocol is OLS:
 * http://www.lxtreme.nl/ols/
 * To install device support for OLS, copy ols.profile* file into the
 * ols/plugins folder.
 *
 * Copyright (C) 2012 Thomas Fischl <tfischl@gmx.de> http://www.fischl.de
 * Last update: 2012-12-24
 *
 * Copyright (C) 2013 Lee Bowyer <lee@sodnpoo.com> http://www.sodnpoo.com
 * Last update: 2013-02-09
 *
 */

#include "inc/lm4f120h5qr.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/uart.h"
#include "driverlib/rom.h"

#define VERSION "SLLogicLogger v0.4"

// SUMP command defintions
#define SUMP_RESET 0x00
#define SUMP_ARM   0x01
#define SUMP_QUERY 0x02
#define SUMP_GET_METADATA 0x04
#define SUMP_SET_DIVIDER 0x80

// buffer size
#define MAX_SAMPLERATE 8000000
#define BUFFERSIZE 31744

// macro for sending one char over UART
#define uart_putch(ch) ROM_UARTCharPut(UART0_BASE, ch)

// sampling buffer
char buffer[BUFFERSIZE];

// send nullterminated string over UART
void uart_puts(char * s) {
    while (*s != 0) {
        uart_putch(*s);
        s++;
    }
}

// send 32bit unsigned integer as SUMP metadata
void sump_sendmeta_uint32(char type, unsigned int i) {
    uart_putch(type);
    uart_putch((i >> 24) & 0xff);
    uart_putch((i >> 16) & 0xff);
    uart_putch((i >> 8) & 0xff);
    uart_putch(i & 0xff);
}

// send 8bit unsigned integer as SUMP metadata
void sump_sendmeta_uint8(char type, unsigned char i) {
    uart_putch(type);
    uart_putch(i);
}

void doticksampling(){
  // use it as timeout for trigger
  unsigned int i = 0xfffffff;
  // get current gpio state an wait for change or timeout
  unsigned char v = GPIO_PORTB_DATA_R;
  while ((i-- != 0) && (GPIO_PORTB_DATA_R == v));

  ROM_SysTickIntEnable();
  ROM_SysTickEnable();
}

void SysTickHandler(void){
  static long i = 0;
  
  buffer[i] = GPIO_PORTB_DATA_R;
  i++;
  
  if(i > BUFFERSIZE){
    ROM_SysTickDisable();
    ROM_SysTickIntDisable();
    i = 0;
  
    // send it over uart
    for (int j = 0; j < BUFFERSIZE; j++){
      uart_putch(buffer[j]);
    }  
  } 
}

// main routine
int main(void) {

    // enable lazy stacking
    ROM_FPUEnable();
    ROM_FPULazyStackingEnable();

    // run from crystal, 80 MHz
    ROM_SysCtlClockSet(SYSCTL_SYSDIV_2_5 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ);

    // enable peripherals
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);

    // set UART pins
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    ROM_GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    // init PORTB
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    GPIO_PORTB_DIR_R = 0x00;
    GPIO_PORTB_DEN_R = 0xff;

    // configure uart
    ROM_UARTConfigSetExpClk(UART0_BASE, ROM_SysCtlClockGet(), 115200, (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));

    // Enable interrupts to the processor. - not sure if this is needed...
    ROM_IntMasterEnable();

    // main loop
    while (1) {

        if (ROM_UARTCharsAvail(UART0_BASE)) {
            unsigned char cmd;
            cmd = ROM_UARTCharGetNonBlocking(UART0_BASE);

            switch (cmd) {
                case SUMP_RESET:
                    break;
                case SUMP_ARM:
                    doticksampling();
                    break;
                case SUMP_QUERY:
                    uart_puts("1ALS");
                    break;
                case SUMP_GET_METADATA:

                    // device name
                    uart_putch(0x01);
                    uart_puts(VERSION);
                    uart_putch(0x00);

                    // amount of sample memory available (bytes) 
                    sump_sendmeta_uint32(0x21, BUFFERSIZE);

                    // maximum sample rate (hz)
                    sump_sendmeta_uint32(0x23, MAX_SAMPLERATE);

                    // number of usable probes (short) 
                    sump_sendmeta_uint8(0x40, 0x08);

                    // protocol version (short)
                    sump_sendmeta_uint8(0x41, 0x02);

                    // end of meta data
                    uart_putch(0x00);

                    break;
                
                case SUMP_SET_DIVIDER: {
                  /*
                  Set Divider (80h)
                  When x is written, the sampling frequency is set to f = clock / (x + 1)
                  */
                  unsigned long clock = 100000000; //no idea where this comes from...
                  //these three bytes are our clock divider - lsb first
                  unsigned char b0 = ROM_UARTCharGet(UART0_BASE);
                  unsigned char b1 = ROM_UARTCharGet(UART0_BASE);
                  unsigned char b2 = ROM_UARTCharGet(UART0_BASE);
                  ROM_UARTCharGet(UART0_BASE); //eat last byte

                  unsigned long rate = b0 | (b1 << 8) | (b2 << 16);
                  rate = clock / (rate+1);

                  ROM_SysTickPeriodSet(ROM_SysCtlClockGet() / rate);
                  break;
                }

                // long commands.. consume bytes from uart
                case 0xC0:
                case 0xC4:
                case 0xC8:
                case 0xCC:
                case 0xC1:
                case 0xC5:
                case 0xC9:
                case 0xCD:
                case 0xC2:
                case 0xC6:
                case 0xCA:
                case 0xCE:
                case 0x81:
                case 0x82:
                    ROM_UARTCharGet(UART0_BASE);
                    ROM_UARTCharGet(UART0_BASE);
                    ROM_UARTCharGet(UART0_BASE);
                    ROM_UARTCharGet(UART0_BASE);
                    break;
            }

        }

    }
}

