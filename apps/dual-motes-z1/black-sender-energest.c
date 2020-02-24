/**
 * \file
 *         Unicast sending test program
 * \author
 *         Marie-Paule Uwase
 *         August 7, 2012
 *         Tested and modified on September 8, 2012.
 *         Send address is directly read from rime.
 *
 *         Modified receiver functions on January 12, 2014 by NGUYEN Thanh Long
 *         Version used by JT on April 26, 2015 in Poix.
 *         Refactored by JT on May 10, 2015
 */

// imported libraries.

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "contiki.h"
#include "dev/i2cmaster.h"
#include "dev/tmp102.h"
#include "net/rime/rime.h"
#include "dev/cc2420/cc2420.h"   //To use RSSI and LQI variables
#include "sys/energest.h" // Module to measure ON time of hardware component
#include "dev/leds.h"
#include "lib/random.h"
#include <msp430.h>
#include <legacymsp430.h>


/* In Rime communicating nodes must agree on a 16 bit virtual
 * channel number. For each virtual channel one can define 
 * the Rime modules for communicating over that channel.
 * Channel numbers < 128 are reserved by the system.
 */ 
uint16_t channel =       133;
// sender mote id
uint8_t sender;          // set automatically

// sender power
// possible values =  0dBm = 31;  -1dBm = 27;  -3dBm = 23;  -5dBm = 19; 
//                   -7dBm = 15; -10dBm = 11; -15dBm =  7; -25dBM =  3; 
uint8_t power = 31;

// receiver mote id
uint8_t receiver = 211;

// message counters
uint16_t send = 0 ;

/* Selection of the time interval between messages */

#define AVERAGE_SEND_INTERVAL CLOCK_SECOND
#define RANDOM 1
#define MIN_SEND_INTERVAL 1
#define INTERVAL_RANGE (AVERAGE_SEND_INTERVAL - MIN_SEND_INTERVAL) * 2 

#if RANDOM
#define SEND_INTERVAL  ((INTERVAL_RANGE + random_rand()) % INTERVAL_RANGE) + MIN_SEND_INTERVAL
#else
#define SEND_INTERVAL  AVERAGE_SEND_INTERVAL
#endif

/* Data structure of messages sent from sender
 *
 */


struct testmsg {       
	uint16_t  blackseqno;
	uint16_t  timestamp_app;
	uint16_t cpu;
	uint16_t lpm;
	uint16_t transmit;
	uint16_t listen;
    char      padding[82];
    uint16_t  timestamp_mac;
};

uint16_t seqno=0;

PROCESS(temp_process, "Sending numbered messages");
AUTOSTART_PROCESSES(&temp_process);

/*--------------------------------------------------------------------------------
 * SETTING THE GPIOS
 *-------------------------------------------------------------------------------*/

void
GPIOS_init(void)
{
	//configuring pin as GPIO

	P1SEL &= ~0xC1;			//GPIO P1.0 P1.6 P1.7
	P2SEL &= ~0x18;			//GPIO P2.3 P2.4
	P4SEL &= ~0x0D;			//GPIO P4.0 P4.2 P4.3

	//configuring pin as OUTPUT

	P1DIR |= 0xC1;
	P2DIR |= 0x18;
	P4DIR |= 0x0D;


}

void
clear_GPIOS(void)
{
//clear all output pins
	P1OUT &= ~BIT6;		//P1.6 AS 0
	P1OUT &= ~BIT7;		//P1.7 AS 0

	P2OUT &= ~BIT3;		//P2.3 AS 0
	P2OUT &= ~BIT4;		//P2.4 AS 0

	P4OUT &= ~BIT0;		//P4.0 AS 0
	P4OUT &= ~BIT2;		//P4.2 AS 0
	P4OUT &= ~BIT3;		//P4.3 AS 0

}


/*--------------------------------------------------------------------------------
 * Receiver function which seems necessary to operate the sender with a normal MAC
 *-------------------------------------------------------------------------------*/

static void
recv_uc(struct unicast_conn *c, const linkaddr_t *from)
{
	printf("unicast message received from %d.%d\n",
			from->u8[0], from->u8[1]);
}

static const struct unicast_callbacks unicast_callbacks = {recv_uc};
static struct unicast_conn uc;

/*---------------------------------------------------------------------------*/
void
packet_send(void)
{
	unsigned long cpu, lpm, transmit, listen;
	static unsigned long last_cpu, last_lpm, last_transmit, last_listen;

	linkaddr_t destination;
	seqno++;
	// define the destination address
	destination.u8[0] = receiver;
	destination.u8[1] = 0;

        struct testmsg msg;
	/*Set general info*/
        msg.blackseqno=seqno;		
        msg.timestamp_app= clock_time();
        energest_flush();

        cpu = energest_type_time(ENERGEST_TYPE_CPU) - last_cpu;
        lpm = energest_type_time(ENERGEST_TYPE_LPM) - last_lpm;
        transmit = energest_type_time(ENERGEST_TYPE_TRANSMIT) - last_transmit;
        listen = energest_type_time(ENERGEST_TYPE_LISTEN) - last_listen;

        /* Make sure that the values are within 16 bits. If they are larger,
                      we scale them down to fit into 16 bits. */
        while(cpu >= 65536ul || lpm >= 65536ul ||
                 	transmit >= 65536ul || listen >= 65536ul) {
        	cpu /= 2;
         	lpm /= 2;
         	transmit /= 2;
         	listen /= 2;
         	}

        msg.cpu = cpu;
        msg.lpm = lpm;
        msg.transmit = transmit;
        msg.listen = listen;
/*
 *      convert seqno into bits and set the GPIO bits accordingly
 */
	static uint8_t seqno_bits[6];			
	uint8_t i;
	for (i = 0; i < 6; i++) {
		seqno_bits[i] = msg.blackseqno & (1 << i) ? 1 : 0;
	}		//least significant bit in seqno_bits[0]

	clear_GPIOS();
	if ( seqno_bits[0]==1 )	P1OUT |= BIT6;       //  write a 1 in P1.6
	if ( seqno_bits[1]==1 )	P1OUT |= BIT7;       //  write a 1 in P1.7
	if ( seqno_bits[2]==1 )	P2OUT |= BIT3;       //  write a 1 in P2.3
	if ( seqno_bits[3]==1 )	P4OUT |= BIT0;       //  write a 1 in P4.0
	if ( seqno_bits[4]==1 )	P4OUT |= BIT2;       //  write a 1 in P4.2
	if ( seqno_bits[5]==1 )	P4OUT |= BIT3;       //  write a 1 in P4.3
        P1OUT ^= BIT0;               // change state of P1.0 to trigger reading by white mote

	/*Avoid sending to itself*/
	if(!linkaddr_cmp(&destination, &linkaddr_node_addr))
	{
	    packetbuf_copyfrom(&msg, sizeof(msg));
	    packetbuf_set_attr(PACKETBUF_ATTR_PACKET_TYPE,
	    			PACKETBUF_ATTR_PACKET_TYPE_TIMESTAMP);


            last_cpu = energest_type_time(ENERGEST_TYPE_CPU);
            last_lpm = energest_type_time(ENERGEST_TYPE_LPM);
            last_transmit = energest_type_time(ENERGEST_TYPE_TRANSMIT);
            last_listen = energest_type_time(ENERGEST_TYPE_LISTEN);
            //printf(" energest values: %u %u %u %u \n", msg.cpu, msg.lpm, msg.transmit, msg.listen);

	    unicast_send(&uc, &destination);

	}
}
/*-------------------------------------------------------------------------------
 * Send packets with all needed statistic information
 *-------------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------------*/

static struct etimer et;

// Sending thread

PROCESS_THREAD(temp_process, ev, data)
{
	PROCESS_BEGIN();

	// adjust power
	cc2420_set_txpower(power);

	tmp102_init();
	GPIOS_init();
	//timerA_init();
	unicast_open(&uc, channel, &unicast_callbacks);
	// infinite loop
	while(1)
	{
		//    wait for the SEND_INTERVAL time
		etimer_set(&et, SEND_INTERVAL);
  
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		printf(" %lu\n",SEND_INTERVAL);

		clear_GPIOS();
		packet_send();

	}

	PROCESS_END();
}
