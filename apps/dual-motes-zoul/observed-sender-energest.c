/**
 * \file
 *         Unicast sending test program
 * \author
 *         Marie-Paule Uwase
 *         August 7, 2012
 *         Roald Van Glabbeek
 * 		   March 3, 2020
 * 
 *         Updated for newer contiki release en Zolertia Zoul (firefly)
 */

// imported libraries.

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "contiki.h"
#include "net/rime/rime.h"
#include "sys/energest.h" // Module to measure ON time of hardware component
#include "dev/leds.h"
#include "lib/random.h"
#include "core/net/netstack.h"
#include "cpu/cc2538/dev/gpio.h"


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
linkaddr_t destination;

// message counters
uint16_t send = 0 ;

static void recv_uc(struct unicast_conn *c, const linkaddr_t *from);
static void sent_uc(struct unicast_conn *c, int status, int num_tx);

static const struct unicast_callbacks unicast_callbacks = {sent_uc, recv_uc};
static struct unicast_conn uc;

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


/*--------------------------------------------------------------------------------
 * Receiver function which seems necessary to operate the sender with a normal MAC
 *-------------------------------------------------------------------------------*/

static void
recv_uc(struct unicast_conn *c, const linkaddr_t *from)
{
	//printf("unicast message received from %d.%d\n",
	//		from->u8[0], from->u8[1]);
}

/*---------------------------------------------------------------------------*/
void
prepare_payload( void )
{
	unsigned long cpu, lpm, transmit, listen;
	static unsigned long last_cpu, last_lpm, last_transmit, last_listen;
	struct testmsg msg;

	seqno++;

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
		cpu = cpu >> 1;
		lpm = lpm >> 1;
		transmit = transmit >> 2;
		listen = listen >> 2;
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

	packetbuf_copyfrom(&msg, sizeof(msg));
	packetbuf_set_attr(PACKETBUF_ATTR_PACKET_TYPE,
				PACKETBUF_ATTR_PACKET_TYPE_TIMESTAMP);

	last_cpu = energest_type_time(ENERGEST_TYPE_CPU);
	last_lpm = energest_type_time(ENERGEST_TYPE_LPM);
	last_transmit = energest_type_time(ENERGEST_TYPE_TRANSMIT);
	last_listen = energest_type_time(ENERGEST_TYPE_LISTEN);
	//printf(" energest values: %u %u %u %u \n", msg.cpu, msg.lpm, msg.transmit, msg.listen);
}
/*---------------------------------------------------------------------------*/
static void
sent_uc(struct unicast_conn *c, int status, int num_tx)
{
	const linkaddr_t *dest = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);
  	if(linkaddr_cmp(dest, &linkaddr_null)) {
    	return;
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
	NETSTACK_RADIO.set_value(RADIO_PARAM_TXPOWER,power);

	GPIOS_init();
	unicast_open(&uc, channel, &unicast_callbacks);

	destination.u8[0] = 0xde;
	destination.u8[1] = 0xae;

	while(1)
	{
		//    wait for the SEND_INTERVAL time
		etimer_set(&et, SEND_INTERVAL);
  
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		printf("Send interval: %u; Seqno: %u\n",SEND_INTERVAL, seqno);

		clear_GPIOS();
		prepare_payload();
		if(!linkaddr_cmp(&destination, &linkaddr_node_addr)) {
      		unicast_send(&uc, &destination);
		}
    }

	PROCESS_END();
}
