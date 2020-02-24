/**
 * \file
 *         White (observer) sender program for the dual network.
 *         This program sends a messsage to the white sink whenever it is trigered via GPIO pin 1.0 by the connected black mote.
 *
 * \author
 *         Marie-Paule Uwase
 *         August 7, 2012
 *         Tested and modified on September 8, 2012.
 *         Send address is directly read from rime.
 *
 *         Modified receiver functions on January 12, 2014 by NGUYEN Thanh Long
 *         Modified by Maite Bezunartea in August 2014 to observe a black receiver in a dual network and transmit the collected 
 *         data to the white sink
 *         JT: This program has been used in Poix on April 26, 2015.
 *         This program has been refactored by JT on May 3, 2015 to
 *           Correct the power measurements
 *           Make the whole program more readable.
*         June 2, 2016: Port 6.7 (ADC input) has been initialised as input rather than output! 
 *         This program has been used for 2/3 CCA comparisons in Poix on november 28+29, 2016
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
//#include "sys/energest.h" // Module to measure ON time of hardware component
#include "dev/leds.h"
#include <msp430.h>
#include <legacymsp430.h>

#include "dev/battery-sensor.h"

/*
 * receiver mote id. (needs to be set in function of the destination)
 */
uint8_t receiver0 = 12; //This is normally the white sink
uint8_t receiver1 = 0;   //In this network the second byte of the rime addresses is not used.

/* In Rime communicating nodes must agree on a 16 bit virtual
 * channel number. For each virtual channel one can define 
 * the Rime modules for communicating over that channel.
 * Channel numbers < 128 are reserved by the system.
 */ 
uint16_t channel =       133;

/*
 * The sender mote id is set automatically from the rime address.
 */
uint8_t sender;          

/* sender power
 * possible values =  0dBm = 31;  -1dBm = 27;  -3dBm = 23;  -5dBm = 19; 
 *                    -7dBm = 15; -10dBm = 11; -15dBm =  7; -25dBM =  3;
 */ 
uint8_t power = 31;

/*
 * sent message counter
 */
uint16_t send = 0 ;

/*
 * Interval between consecutive probes of the triger bit P1.0
 */

#define GPIO_READ_INTERVAL (CLOCK_SECOND/128)

/* 
 * Data structure of sent messages
 */
uint16_t whiteseqno=0;
uint32_t  ADCResult=0;
uint32_t  counter=0;
uint8_t   flag;

PROCESS(temp_process, "Sending messages to the white sink about a black mote");
AUTOSTART_PROCESSES(&temp_process);

/*--------------------------------------------------------------------------------
 * SETTING THE GPIOS
 *-------------------------------------------------------------------------------*/

void
GPIOS_init(void)
{
	//configuring pin as GPIO

	P1SEL &= ~0xC1;		//GPIO P1.0 P1.6 P1.7
	P2SEL &= ~0x18;		//GPIO P2.3 P2.4
	P4SEL &= ~0x0D;		//GPIO P4.0 P4.2 P4.3

	//configuring pin as INPUT

	P1DIR &= ~BIT0;		//P1.0 AS INPUT will be the one generating the interrupt
	P1DIR &= ~BIT6;		//P1.6 AS INPUT
	P1DIR &= ~BIT7;		//P1.7 AS INPUT

	P2DIR &= ~BIT3;		//P2.3 AS INPUT
	P2DIR &= ~BIT4;		//P2.4 AS INPUT

	P4DIR &= ~BIT0;		//P4.0 AS INPUT
	P4DIR &= ~BIT2;		//P4.2 AS INPUT
	P4DIR &= ~BIT3;		//P4.3 AS INPUT

	P6DIR &= ~BIT7;		//P6.7 AS INPUT

}

uint8_t
read_GPIOS(void)
{
	//reading the value in each pin
	uint8_t  blackseqno=0;

	if (P1IN & BIT6)	blackseqno=blackseqno+1;		
	if (P1IN & BIT7)    blackseqno=blackseqno+2;
	if (P2IN & BIT3)	blackseqno=blackseqno+4; 
	if (P4IN & BIT0)	blackseqno=blackseqno+8;
	if (P4IN & BIT2)	blackseqno=blackseqno+16; 
	if (P4IN & BIT3)	blackseqno=blackseqno+32; 

	return blackseqno;
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
	linkaddr_t destination;
	whiteseqno++;
	// define the destination address
	destination.u8[0] = receiver0;
	destination.u8[1] = receiver1;


	/*Set general info*/

struct whitemsg {
	uint8_t  blackseqno;
	uint16_t whiteseqno;
	uint32_t energy;
	uint16_t counter_ADC;
	uint16_t timestamp_app;
        uint16_t timestamp_mac;
};

struct whitemsg msg;

        msg.blackseqno = read_GPIOS();	
    	msg.whiteseqno = whiteseqno;	
        msg.energy = ADCResult;
        msg.counter_ADC = counter;
        msg.timestamp_app = RTIMER_NOW();
        msg.timestamp_mac = 0;        

	/*Avoid sending to itself*/
	if(!linkaddr_cmp(&destination, &linkaddr_node_addr))
	{
	    packetbuf_copyfrom(&msg, sizeof(msg));
        
	    packetbuf_set_attr(PACKETBUF_ATTR_PACKET_TYPE,
				PACKETBUF_ATTR_PACKET_TYPE_TIMESTAMP);

	    unicast_send(&uc, &destination);

            printf("Black= %u White= %u Energy= %li counter_ADC = %u App.time= %u Dest= %d.%d\n",
                     msg.blackseqno,msg.whiteseqno,msg.energy,msg.counter_ADC,msg.timestamp_app, destination.u8[0], destination.u8[1]);

            
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
	SENSORS_ACTIVATE(battery_sensor);

	flag=(P1IN & BIT0);

	// adjust power
	cc2420_set_txpower(power);

	//tmp102_init();
	GPIOS_init();
        counter = 0;
	//ADC_init();
	unicast_open(&uc, channel, &unicast_callbacks);
	// infinite loop
	while(1)
	{
		//    wait for the GPIO_READ_INTERVAL time
		etimer_set(&et, GPIO_READ_INTERVAL);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		counter++;
 		int32_t bateria = battery_sensor.value(0);
		ADCResult = ADCResult +bateria;

		if (flag!=(P1IN & BIT0))		//read state of P1IN. if it has changed, we should send a packet
		{
			flag=(P1IN & BIT0);		//save the new state of P1.0
 
			packet_send();

			ADCResult=0;
                        counter=0;				

		}

	}

	SENSORS_DEACTIVATE(battery_sensor);

	PROCESS_END();
}
