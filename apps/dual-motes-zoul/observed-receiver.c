/**
 * \file
 *          Unicast receiving test program
 *          Accepts messages from everybody
 * \author
 *         Marie-Paule Uwase
 *         August 13, 2012
 *         Roald Van Glabbeek
 * 		   March 3, 2020
 * 
 *         Updated for newer contiki release en Zolertia Zoul (firefly)
 */

// imported libraries.

#include <stdio.h>
#include "contiki.h"
#include "net/rime/rime.h"
#include "dev/leds.h"
#include "core/net/netstack.h"
#include "cpu/cc2538/dev/gpio.h"


/* In Rime communicating nodes must agree on a 16 bit virtual
 * channel number. For each virtual channel one can define 
 * the Rime modules for communicating over that channel.
 * Channel numbers < 128 are reserved by the system.
 */ 
uint16_t channel =       133;

// sender power
// possible values =  0dBm = 31;  -1dBm = 27;  -3dBm = 23;  -5dBm = 19; 
//                   -7dBm = 15; -10dBm = 11; -15dBm =  7; -25dBM =  3; 
uint8_t power = 31;

/* CCA threshold, default value = -77 dBm  */
int8_t cca_threshold = -77;

// message counters
static uint16_t received = 0 ;

//variables for channel quality
//int8_t lqi_val ;
int16_t rssi_val ;

// Writes a title on the console

PROCESS(temp_process, "receiving numbered messages, observed (black) network");

AUTOSTART_PROCESSES(&temp_process);

struct testmsg {       
	uint16_t  blackseqno;
	uint16_t  timestamp_app;
        char      padding[44];
        uint16_t  timestamp_mac;        
};

/*--------------------------------------------------------------------------------
 * SETTING THE GPIOS
 *-------------------------------------------------------------------------------*/

void
GPIOS_init(void)
{

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

	GPIO_CLR_PIN(GPIO_PORT_TO_BASE(2),GPIO_PIN_MASK(0));		//GPIO PC0
	GPIO_CLR_PIN(GPIO_PORT_TO_BASE(2),GPIO_PIN_MASK(1));		//GPIO PC1
	GPIO_CLR_PIN(GPIO_PORT_TO_BASE(2),GPIO_PIN_MASK(4));		//GPIO PC4
	GPIO_CLR_PIN(GPIO_PORT_TO_BASE(2),GPIO_PIN_MASK(5));		//GPIO PC5
	GPIO_CLR_PIN(GPIO_PORT_TO_BASE(3),GPIO_PIN_MASK(1));		//GPIO PD1
	GPIO_CLR_PIN(GPIO_PORT_TO_BASE(3),GPIO_PIN_MASK(2));		//GPIO PD2
}

// This is the receiver function

recv_uc(struct unicast_conn *c, const linkaddr_t *from)
{
/* note the time of arrival of the message at application level.
	unsigned long rtime; // received time (long)
	rtime=clock_time();                             // Low resolution real-time clock.
	rtimer_clock_t now = RTIMER_NOW();		// Higher resolution real time clock
*/
	struct testmsg msg;
	//packetbuf_copyto(&msg);
	memcpy(&msg, packetbuf_dataptr(), sizeof(msg));
	static uint8_t seqno_bits[6];			
	uint8_t i;
	for (i = 0; i < 6; i++) {
		seqno_bits[i] = msg.blackseqno & (1 << i) ? 1 : 0;
	}		//least significant bit in seqno_bits[0]
	//struct testmsg msg;
	//memcpy(&msg, packetbuf_dataptr(), sizeof(msg));
	received ++;
/*
 *      convert seqno into bits and set the GPIO bits accordingly
 */
	printf("received msg from %x:%x\n", from->u8[0], from->u8[1]);

	clear_GPIOS();

	if ( seqno_bits[0]==1 )	GPIO_SET_PIN(GPIO_PORT_TO_BASE(2),GPIO_PIN_MASK(0));       //  write a 1 in C0
	if ( seqno_bits[1]==1 )	GPIO_SET_PIN(GPIO_PORT_TO_BASE(2),GPIO_PIN_MASK(1));       //  write a 1 in C1
	if ( seqno_bits[2]==1 )	GPIO_SET_PIN(GPIO_PORT_TO_BASE(2),GPIO_PIN_MASK(4));       //  write a 1 in C4
	if ( seqno_bits[3]==1 )	GPIO_SET_PIN(GPIO_PORT_TO_BASE(2),GPIO_PIN_MASK(5));       //  write a 1 in C5
	if ( seqno_bits[4]==1 )	GPIO_SET_PIN(GPIO_PORT_TO_BASE(3),GPIO_PIN_MASK(1));       //  write a 1 in D1
	if ( seqno_bits[5]==1 )	GPIO_SET_PIN(GPIO_PORT_TO_BASE(3),GPIO_PIN_MASK(2));       //  write a 1 in D2

	if (GPIO_READ_PIN(GPIO_PORT_TO_BASE(0),GPIO_PIN_MASK(2)) == 0)
		GPIO_SET_PIN(GPIO_PORT_TO_BASE(0),GPIO_PIN_MASK(2));
	else
		GPIO_CLR_PIN(GPIO_PORT_TO_BASE(0),GPIO_PIN_MASK(2));

/*
 *    Print received message for control purposes
  		NETSTACK_RADIO.get_value(RADIO_PARAM_RSSI, rssi_val);
        rssi_val = ( cc2420_last_rssi ) - 45; 
        printf("Rcvd = %5d from %4d, blcksqno = %5d length = % 3d, RSSI= %4d\n",received,from->u8[0], msg.blackseqno, sizeof(msg), rssi_val);
*/
}

static const struct unicast_callbacks unicast_callbacks = {recv_uc};

static struct unicast_conn uc;

PROCESS_THREAD(temp_process, ev, data)
{
	PROCESS_BEGIN();

printf("power level = %u, cca threshold = %d \n",power,cca_threshold);

/* adjust power                                          */
	NETSTACK_RADIO.set_value(RADIO_PARAM_TXPOWER,power);
/* adjust cca threshold                                  */
	NETSTACK_RADIO.set_value(RADIO_PARAM_CCA_THRESHOLD,cca_threshold);

	GPIOS_init();

	unicast_open(&uc, channel, &unicast_callbacks);

	PROCESS_END();
}
