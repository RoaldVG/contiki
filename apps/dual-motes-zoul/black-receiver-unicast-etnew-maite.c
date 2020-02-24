/**
 * \file
 *          Unicast receiving test program
 *          Accepts messages from everybody
 * \author
 *         Marie-Paule Uwase
 *         August 13, 2012
 *         Tested and modified on September 8, 2012. Includes a useless but necessary timer!
 *         Writes with a fixed format in the serialdump to facilitate transfer of data to excel.
 *         Ack power is 0dBm, regardless of the sender settings.
 *         Version used for measurements on May 18, 2013 in Poix.
 *         Version modified by Maite Bezunartea in August 2014 for using it in the sink of the black part of a dual network 
 *         Version used by JT on April 26, 2015 in Poix 
 *         Refactored by JT on May 5, 2015.
 *         
 *         GPIOS initialisation bug corrected in August 2016
 *         GPIO P1.0 pulsed rather than toggled. August 7, 2016.
 *         Pulse duration based on RTIMER, August 28, 2016, **** untested! ****
 */

// imported libraries.

#include <stdio.h>
#include "contiki.h"
//#include "dev/i2cmaster.h"
//#include "dev/tmp102.h"
#include "net/rime/rime.h"
//#include "dev/cc2420/cc2420.h"          // To use RSSI and LQI variables
//#include "sys/energest.h"      // Module to measure ON time of hardware component
#include "dev/leds.h"
#include "core/net/netstack.h"
#include "cpu/cc2538/dev/gpio.h"


/* In Rime communicating nodes must agree on a 16 bit virtual
 * channel number. For each virtual channel one can define 
 * the Rime modules for communicating over that channel.
 * Channel numbers < 128 are reserved by the system.
 */ 
uint16_t channel_unicast =       134;

// sender power
// possible values =  0dBm = 31;  -1dBm = 27;  -3dBm = 23;  -5dBm = 19; 
//                   -7dBm = 15; -10dBm = 11; -15dBm =  7; -25dBM =  3; 
uint8_t power = 31;

// message counters
static uint16_t received = 0 ;

/* Timer for delay in black>white communication  */

 static uint8_t flag_white=0;
 static struct etimer et2;

//variables for channel quality
//int8_t lqi_val ;
int16_t rssi_val ;

/* Altough it doesn't have anything to do every 10 seconds this timer is needed in order 
 * for the receiver to work ???
 */
#define TMP102_READ_INTERVAL (CLOCK_SECOND*10)

/*
 * Duration of the triger bit P1.0 for communicating with the white mote
 */
#define PULSEWIDTH 2  // Two ticks of the clock counting 128 ticks/s

void tell_white (void);

// Writes a title on the console

PROCESS(temp_process, "receiving numbered black unicast messages");
AUTOSTART_PROCESSES(&temp_process);

struct testmsg {       
    uint16_t  blackseqno;
    uint16_t  timestamp_app;
    char      padding[44];
    uint16_t  timestamp_mac;
};
struct testmsg msg;

/*--------------------------------------------------------------------------------
 * SETTING THE GPIOS
 *-------------------------------------------------------------------------------*/

void
GPIOS_init(void)
{
	/*
	//configuring pin as GPIO

	P1SEL &= ~0xC1;			//GPIO P1.0 P1.6 P1.7
	P2SEL &= ~0x08;			//GPIO P2.3 
	P4SEL &= ~0x0D;			//GPIO P4.0 P4.2 P4.3

	//configuring pin as OUTPUT

	P1DIR |= 0xC1;                  //GPIO P1.0 P1.6 P1.7
	P2DIR |= 0x08;                  //GPIO P2.3              
	P4DIR |= 0x0D;                  //GPIO P4.0 P4.2 P4.3
	*/

	GPIO_SET_OUTPUT(GPIO_PORT_TO_BASE(0),GPIO_PIN_MASK(2));		//GPIO PA2
	GPIO_SET_OUTPUT(GPIO_PORT_TO_BASE(2),GPIO_PIN_MASK(0));		//GPIO PC0
	GPIO_SET_OUTPUT(GPIO_PORT_TO_BASE(2),GPIO_PIN_MASK(1));		//GPIO PC1
	GPIO_SET_OUTPUT(GPIO_PORT_TO_BASE(2),GPIO_PIN_MASK(4));		//GPIO PC4
	GPIO_SET_OUTPUT(GPIO_PORT_TO_BASE(2),GPIO_PIN_MASK(5));		//GPIO PC5
	GPIO_SET_OUTPUT(GPIO_PORT_TO_BASE(3),GPIO_PIN_MASK(1));		//GPIO PD1
	GPIO_SET_OUTPUT(GPIO_PORT_TO_BASE(3),GPIO_PIN_MASK(2));		//GPIO PD2
}

void
clear_GPIOS(void)
{
//clear all output pins
    /*
	P1OUT &= ~BIT0;     //P1.0 AS 0
	P1OUT &= ~BIT6;		//P1.6 AS 0
	P1OUT &= ~BIT7;		//P1.7 AS 0

	P2OUT &= ~BIT3;		//P2.3 AS 0

	P4OUT &= ~BIT0;		//P4.0 AS 0
	P4OUT &= ~BIT2;		//P4.2 AS 0
	P4OUT &= ~BIT3;		//P4.3 AS 0
	*/

	GPIO_CLR_PIN(GPIO_PORT_TO_BASE(0),GPIO_PIN_MASK(2));		//GPIO PA2
	GPIO_CLR_PIN(GPIO_PORT_TO_BASE(2),GPIO_PIN_MASK(0));		//GPIO PC0
	GPIO_CLR_PIN(GPIO_PORT_TO_BASE(2),GPIO_PIN_MASK(1));		//GPIO PC1
	GPIO_CLR_PIN(GPIO_PORT_TO_BASE(2),GPIO_PIN_MASK(4));		//GPIO PC4
	GPIO_CLR_PIN(GPIO_PORT_TO_BASE(2),GPIO_PIN_MASK(5));		//GPIO PC5
	GPIO_CLR_PIN(GPIO_PORT_TO_BASE(3),GPIO_PIN_MASK(1));		//GPIO PD1
	GPIO_CLR_PIN(GPIO_PORT_TO_BASE(3),GPIO_PIN_MASK(2));		//GPIO PD2
}

// This is the receiver interrupt handler

recv_uc(struct unicast_conn *c, const linkaddr_t *from)
{
/* note the time of arrival of the message at application level.
	unsigned long rtime; // received time (long)
	rtime=clock_time();                             // Low resolution real-time clock.
	rtimer_clock_t now = RTIMER_NOW();		// Higher resolution real time clock
*/
	
	memcpy(&msg, packetbuf_dataptr(), sizeof(msg));
	received ++;
	tell_white();
/*
 *    Print received message for control purposes
 */
	NETSTACK_RADIO.get_value(RADIO_PARAM_RSSI, rssi_val);
	//rssi_val = ( cc2420_last_rssi ) - 45;
	printf("unicast Rcvd = %5d from %4d, blcksqno = %5d length = % 3d, RSSI= %4d\n",received,from->u8[0], msg.blackseqno, sizeof(msg), rssi_val);

}

void tell_white (void)
{
/*
 *      convert seqno into bits and set the GPIO bits accordingly
 */
	static uint8_t seqno_bits[6];			
	uint8_t i;

	for (i = 0; i < 6; i++) {
		seqno_bits[i] = msg.blackseqno & (1 << i) ? 1 : 0;
	}		//least significant bit in seqno_bits[0]

	/*
	if ( seqno_bits[0]==1 )	P1OUT |= BIT6;       //  write a 1 in P1.6
	if ( seqno_bits[1]==1 )	P1OUT |= BIT7;       //  write a 1 in P1.7
	if ( seqno_bits[2]==1 )	P2OUT |= BIT3;       //  write a 1 in P2.3
	if ( seqno_bits[3]==1 )	P4OUT |= BIT0;       //  write a 1 in P4.0
	if ( seqno_bits[4]==1 )	P4OUT |= BIT2;       //  write a 1 in P4.2
	if ( seqno_bits[5]==1 )	P4OUT |= BIT3;       //  write a 1 in P4.3
	*/

	if ( seqno_bits[0]==1 )	GPIO_SET_PIN(GPIO_PORT_TO_BASE(2),GPIO_PIN_MASK(0));       //  write a 1 in C0
	if ( seqno_bits[1]==1 )	GPIO_SET_PIN(GPIO_PORT_TO_BASE(2),GPIO_PIN_MASK(1));       //  write a 1 in C1
	if ( seqno_bits[2]==1 )	GPIO_SET_PIN(GPIO_PORT_TO_BASE(2),GPIO_PIN_MASK(4));       //  write a 1 in C4
	if ( seqno_bits[3]==1 )	GPIO_SET_PIN(GPIO_PORT_TO_BASE(2),GPIO_PIN_MASK(5));       //  write a 1 in C5
	if ( seqno_bits[4]==1 )	GPIO_SET_PIN(GPIO_PORT_TO_BASE(3),GPIO_PIN_MASK(1));       //  write a 1 in D1
	if ( seqno_bits[5]==1 )	GPIO_SET_PIN(GPIO_PORT_TO_BASE(3),GPIO_PIN_MASK(2));       //  write a 1 in D2
/*
 *          set P1.0 to trigger reading by white mote
 */            
        //P1OUT |= BIT0; 
		GPIO_SET_PIN(GPIO_PORT_TO_BASE(0),GPIO_PIN_MASK(2));       //  write a 1 in A2
/*
 *      leave time for the white mote to read the GPIO.
 */
//        etimer_set(&et2, PULSEWIDTH);  
	     
        flag_white=1;
/*
 *      Clear the GPIO pins.
 */    
        //clear_GPIOS();
}


   // printf(" seqno %u %u %u %u %u %u \n",seqno_bits[0],seqno_bits[1],seqno_bits[2],seqno_bits[3],seqno_bits[4],seqno_bits[5]);
   // printf(" READ_GPIOS %u \n",read_GPIOS());

static const struct unicast_callbacks unicast_callbacks = {recv_uc};
static struct unicast_conn uc;


PROCESS_THREAD(temp_process, ev, data)
{
	PROCESS_BEGIN();

        printf("Begin of process \n");

	// adjust power
	NETSTACK_RADIO.set_value(RADIO_PARAM_TXPOWER,power);
 	//cc2420_set_txpower(power);

	GPIOS_init();

	unicast_open(&uc, channel_unicast, &unicast_callbacks);
//        while(1){
//		if(flag_white){
//		  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et2));
//        	  clear_GPIOS();
//		  flag_white=0;
//                }
//        }
	PROCESS_END();
}
