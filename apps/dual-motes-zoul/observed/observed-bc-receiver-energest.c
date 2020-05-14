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
#include "sys/energest.h" // Module to measure ON time of hardware component

#define DEBUG DEBUG_NONE
#include "net/ip/uip-debug.h"

// Bit-width of IO communication with observer
#define IO_WIDTH 11

static void recv_uc(struct unicast_conn *c, const linkaddr_t *from);
static const struct unicast_callbacks unicast_callbacks = {recv_uc};
static struct unicast_conn uc;

linkaddr_t energest_rx;
void prepare_energest( void );

/* In Rime communicating nodes must agree on a 16 bit virtual
 * channel number. For each virtual channel one can define 
 * the Rime modules for communicating over that channel.
 * Channel numbers < 128 are reserved by the system.
 */ 
uint16_t channel =       133;
uint16_t bc_channel = 129;

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

struct energestmsg {
	uint32_t 	cpu;
	uint32_t 	lpm;
	uint32_t 	transmit;
	uint32_t 	listen;
	uint16_t 	seqno;
};

struct energestmsg prev_energest_vals;

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

	GPIO_SET_OUTPUT(GPIO_A_BASE,GPIO_PIN_MASK(6));		//GPIO PA6
	GPIO_SET_OUTPUT(GPIO_A_BASE,GPIO_PIN_MASK(7));		//GPIO PA7
  
 	GPIO_SET_OUTPUT(GPIO_C_BASE,GPIO_PIN_MASK(0));		//GPIO PC0
	GPIO_SET_OUTPUT(GPIO_C_BASE,GPIO_PIN_MASK(1));		//GPIO PC1
  	GPIO_SET_OUTPUT(GPIO_C_BASE,GPIO_PIN_MASK(2));		//GPIO PC2
  	GPIO_SET_OUTPUT(GPIO_C_BASE,GPIO_PIN_MASK(3));		//GPIO PC3
	GPIO_SET_OUTPUT(GPIO_C_BASE,GPIO_PIN_MASK(4));		//GPIO PC4
	GPIO_SET_OUTPUT(GPIO_C_BASE,GPIO_PIN_MASK(5));		//GPIO PC5
  	GPIO_SET_OUTPUT(GPIO_C_BASE,GPIO_PIN_MASK(6));		//GPIO PC6

	GPIO_SET_OUTPUT(GPIO_D_BASE,GPIO_PIN_MASK(0));		//GPIO PD0
  	GPIO_SET_OUTPUT(GPIO_D_BASE,GPIO_PIN_MASK(1));		//GPIO PD1
	GPIO_SET_OUTPUT(GPIO_D_BASE,GPIO_PIN_MASK(2));		//GPIO PD2
}

void
clear_GPIOS(void)
{
//clear all output pins

	GPIO_CLR_PIN(GPIO_A_BASE,GPIO_PIN_MASK(6));		//GPIO PA6
	
 	GPIO_CLR_PIN(GPIO_C_BASE,GPIO_PIN_MASK(0));		//GPIO PC0
	GPIO_CLR_PIN(GPIO_C_BASE,GPIO_PIN_MASK(1));		//GPIO PC1
  	GPIO_CLR_PIN(GPIO_C_BASE,GPIO_PIN_MASK(2));		//GPIO PC2
  	GPIO_CLR_PIN(GPIO_C_BASE,GPIO_PIN_MASK(3));		//GPIO PC3
	GPIO_CLR_PIN(GPIO_C_BASE,GPIO_PIN_MASK(4));		//GPIO PC4
	GPIO_CLR_PIN(GPIO_C_BASE,GPIO_PIN_MASK(5));		//GPIO PC5
  	GPIO_CLR_PIN(GPIO_C_BASE,GPIO_PIN_MASK(6));		//GPIO PC6

	GPIO_CLR_PIN(GPIO_D_BASE,GPIO_PIN_MASK(0));		//GPIO PD0
  	GPIO_CLR_PIN(GPIO_D_BASE,GPIO_PIN_MASK(1));		//GPIO PD1
	GPIO_CLR_PIN(GPIO_D_BASE,GPIO_PIN_MASK(2));		//GPIO PD2
}

void
prepare_energest( void )
{
	struct energestmsg energest_values;

	PRINTF("Sending to energest sink %x:%x\n",energest_rx.u8[0],energest_rx.u8[1]);

	energest_flush();
	energest_values.cpu = energest_type_time(ENERGEST_TYPE_CPU) - prev_energest_vals.cpu;
	energest_values.lpm = energest_type_time(ENERGEST_TYPE_LPM) - prev_energest_vals.lpm;
	energest_values.transmit = energest_type_time(ENERGEST_TYPE_TRANSMIT) - prev_energest_vals.transmit;
	energest_values.listen = energest_type_time(ENERGEST_TYPE_LISTEN) - prev_energest_vals.listen;
	energest_values.seqno = received;
	
	
	packetbuf_copyfrom(&energest_values, sizeof(energest_values));
	packetbuf_set_attr(PACKETBUF_ATTR_PACKET_TYPE, PACKETBUF_ATTR_PACKET_TYPE_TIMESTAMP);

	prev_energest_vals.cpu = energest_values.cpu;
	prev_energest_vals.lpm = energest_values.lpm;
	prev_energest_vals.transmit = energest_values.transmit;
	prev_energest_vals.listen = energest_values.listen;
}

void
notify_observer(struct testmsg *msg)
{
	static uint8_t seqno_bits[IO_WIDTH];			
	uint8_t i;
	for (i = 0; i < IO_WIDTH; i++) {
		seqno_bits[i] = msg->blackseqno & (1 << i) ? 1 : 0;
	}		//least significant bit in seqno_bits[0]

	clear_GPIOS();

	if ( seqno_bits[0]==1 )		GPIO_SET_PIN(GPIO_A_BASE,GPIO_PIN_MASK(6));       //  write a 1 in A6
	if ( seqno_bits[1]==1 )		GPIO_SET_PIN(GPIO_C_BASE,GPIO_PIN_MASK(0));       //  write a 1 in C0
	if ( seqno_bits[2]==1 )		GPIO_SET_PIN(GPIO_C_BASE,GPIO_PIN_MASK(1));       //  write a 1 in C1
	if ( seqno_bits[3]==1 )		GPIO_SET_PIN(GPIO_C_BASE,GPIO_PIN_MASK(2));       //  write a 1 in C2
	if ( seqno_bits[4]==1 )		GPIO_SET_PIN(GPIO_C_BASE,GPIO_PIN_MASK(3));       //  write a 1 in C3
	if ( seqno_bits[5]==1 )		GPIO_SET_PIN(GPIO_C_BASE,GPIO_PIN_MASK(4));       //  write a 1 in C4
	if ( seqno_bits[6]==1 )		GPIO_SET_PIN(GPIO_C_BASE,GPIO_PIN_MASK(5));       //  write a 1 in C5
	if ( seqno_bits[7]==1 )		GPIO_SET_PIN(GPIO_C_BASE,GPIO_PIN_MASK(6));       //  write a 1 in C6
	if ( seqno_bits[8]==1 )		GPIO_SET_PIN(GPIO_D_BASE,GPIO_PIN_MASK(0));       //  write a 1 in D0
	if ( seqno_bits[9]==1 )		GPIO_SET_PIN(GPIO_D_BASE,GPIO_PIN_MASK(1));       //  write a 1 in D1
	if ( seqno_bits[10]==1 )	GPIO_SET_PIN(GPIO_D_BASE,GPIO_PIN_MASK(2));       //  write a 1 in D2`
	
	if (GPIO_READ_PIN(GPIO_PORT_TO_BASE(0),GPIO_PIN_MASK(7)) == 0)
		GPIO_SET_PIN(GPIO_PORT_TO_BASE(0),GPIO_PIN_MASK(7));
	else
		GPIO_CLR_PIN(GPIO_PORT_TO_BASE(0),GPIO_PIN_MASK(7));

	PRINTF("IO output\n");

	if (received%100 == 0)
	{
		prepare_energest();
		unicast_send(&uc, &energest_rx);
	}
}

// This is the receiver function
static void
recv_uc(struct unicast_conn *c, const linkaddr_t *from)
{
/* note the time of arrival of the message at application level.
	unsigned long rtime; // received time (long)
	rtime=clock_time();                             // Low resolution real-time clock.
	rtimer_clock_t now = RTIMER_NOW();		// Higher resolution real time clock
*/
	received++;
	struct testmsg msg;
	//packetbuf_copyto(&msg);
	memcpy(&msg, packetbuf_dataptr(), sizeof(msg));
	notify_observer(&msg);

/*
 *    Print received message for control purposes
  		NETSTACK_RADIO.get_value(RADIO_PARAM_RSSI, rssi_val);
        rssi_val = ( cc2420_last_rssi ) - 45; 
        PRINTF("Rcvd = %5d from %4d, blcksqno = %5d length = % 3d, RSSI= %4d\n",received,from->u8[0], msg.blackseqno, sizeof(msg), rssi_val);
*/
}

static void
broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
  	received++;
	struct testmsg msg;
	//packetbuf_copyto(&msg);
	memcpy(&msg, packetbuf_dataptr(), sizeof(msg));
	notify_observer(&msg);
}
static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
static struct broadcast_conn broadcast;

PROCESS_THREAD(temp_process, ev, data)
{
	PROCESS_BEGIN();

PRINTF("power level = %u, cca threshold = %d \n",power,cca_threshold);

/* adjust power                                          */
	NETSTACK_RADIO.set_value(RADIO_PARAM_TXPOWER,power);
/* adjust cca threshold                                  */
	NETSTACK_RADIO.set_value(RADIO_PARAM_CCA_THRESHOLD,cca_threshold);

	GPIOS_init();

	energest_rx.u8[0] = 0x9b;
	energest_rx.u8[1] = 0xf3;

	prev_energest_vals.cpu = 0;
	prev_energest_vals.lpm = 0;
	prev_energest_vals.transmit = 0;
	prev_energest_vals.listen = 0;
	prev_energest_vals.seqno = 0;

	unicast_open(&uc, channel, &unicast_callbacks);
	broadcast_open(&broadcast, bc_channel, &broadcast_call);

	PROCESS_END();
}
